#include "wave3/transport.hpp"

#include <libusb.h>

namespace wave3 {

namespace {

constexpr uint8_t kGetMem = 0x85;           // UAC1 GET_MEM
constexpr uint8_t kSetMem = 0x05;           // UAC1 SET_MEM
constexpr uint8_t kMemEntity = 0x33;        // firmware-only entity, not in the descriptors
constexpr uint8_t kClassInterfaceIn = 0xA1;
constexpr uint8_t kClassInterfaceOut = 0x21;
constexpr unsigned kTimeoutMs = 500;        // same timeout Wave Link uses

std::string usb_error(const char* what, int code) {
    return std::string(what) + ": " + libusb_error_name(code);
}

std::string serial_of(libusb_device_handle* h, const libusb_device_descriptor& d) {
    if (!d.iSerialNumber) return {};
    unsigned char buf[128] = {0};
    int r = libusb_get_string_descriptor_ascii(h, d.iSerialNumber, buf, sizeof buf - 1);
    return r > 0 ? std::string(reinterpret_cast<char*>(buf), size_t(r)) : std::string();
}

bool is_wave3(const libusb_device_descriptor& d) {
    return d.idVendor == kElgatoVid && (d.idProduct == kWave3Pid || d.idProduct == kWave3DfuPid);
}

}  // namespace

std::vector<DeviceEntry> UsbTransport::list() {
    libusb_context* ctx = nullptr;
    if (int r = libusb_init(&ctx); r != 0) throw UsbError(usb_error("libusb_init", r));
    std::vector<DeviceEntry> out;
    libusb_device** devs = nullptr;
    ssize_t n = libusb_get_device_list(ctx, &devs);
    for (ssize_t i = 0; i < n; i++) {
        libusb_device_descriptor d{};
        if (libusb_get_device_descriptor(devs[i], &d) != 0 || !is_wave3(d)) continue;
        DeviceEntry e{libusb_get_bus_number(devs[i]), libusb_get_device_address(devs[i]),
                      d.idProduct, {}, {}};
        libusb_device_handle* h = nullptr;
        if (int r = libusb_open(devs[i], &h); r == 0) {
            e.serial = serial_of(h, d);
            libusb_close(h);
        } else {
            e.error = libusb_error_name(r);
        }
        out.push_back(e);
    }
    if (devs) libusb_free_device_list(devs, 1);
    libusb_exit(ctx);
    return out;
}

UsbTransport::UsbTransport(const OpenOptions& options) {
    if (int r = libusb_init(&ctx_); r != 0) throw UsbError(usb_error("libusb_init", r));
    libusb_device** devs = nullptr;
    ssize_t n = libusb_get_device_list(ctx_, &devs);
    std::string last_error = "no Elgato Wave:3 (0fd9:0070) found";
    for (ssize_t i = 0; i < n && !handle_; i++) {
        libusb_device_descriptor d{};
        if (libusb_get_device_descriptor(devs[i], &d) != 0 || !is_wave3(d)) continue;
        if (d.idProduct == kWave3DfuPid) {
            last_error = "the Wave:3 is in firmware-update (DFU) mode";
            continue;
        }
        libusb_device_handle* h = nullptr;
        if (int r = libusb_open(devs[i], &h); r != 0) {
            last_error = usb_error("cannot open the Wave:3", r);
            if (r == LIBUSB_ERROR_ACCESS)
                last_error += " (install packaging/70-elgato-wave3.rules or run as root)";
            continue;
        }
        std::string sn = serial_of(h, d);
        if (!options.serial.empty() && sn != options.serial) {
            libusb_close(h);
            last_error = "no Wave:3 with serial " + options.serial;
            continue;
        }
        handle_ = h;
        serial_ = sn;
    }
    if (devs) libusb_free_device_list(devs, 1);
    if (!handle_) {
        libusb_exit(ctx_);
        throw UsbError(last_error);
    }
    try {
        choose_interface(options);
    } catch (...) {
        libusb_close(handle_);
        libusb_exit(ctx_);
        throw;
    }
}

void UsbTransport::choose_interface(const OpenOptions& options) {
    libusb_config_descriptor* cfg = nullptr;
    if (int r = libusb_get_active_config_descriptor(libusb_get_device(handle_), &cfg); r != 0)
        throw UsbError(usb_error("cannot read the configuration descriptor", r));
    int audio_control = -1;
    std::vector<int> vendor;
    for (int i = 0; i < cfg->bNumInterfaces; i++) {
        const libusb_interface_descriptor& alt = cfg->interface[i].altsetting[0];
        if (alt.bInterfaceClass == LIBUSB_CLASS_AUDIO && alt.bInterfaceSubClass == 1)
            audio_control = alt.bInterfaceNumber;
        else if (alt.bInterfaceClass == LIBUSB_CLASS_VENDOR_SPEC)
            vendor.push_back(alt.bInterfaceNumber);
    }
    libusb_free_config_descriptor(cfg);
    if (audio_control < 0) throw UsbError("no audio-control interface in the descriptors");

    if (options.interface_number >= 0) {
        interface_ = options.interface_number;
        route_ = "interface " + std::to_string(interface_) + " (forced)";
#if defined(__linux__)
        if (int r = libusb_claim_interface(handle_, interface_); r != 0)
            throw UsbError(usb_error(("cannot claim interface " + std::to_string(interface_)).c_str(), r));
        claimed_ = true;
#endif
        return;
    }

#if defined(__linux__)
    // usbfs only forwards class requests aimed at an interface this process has claimed.
    // snd-usb-audio owns the audio-control interface, but the firmware answers the same
    // request on any audio-function interface number, including the vendor interface that no
    // kernel driver binds. Claiming that one leaves audio untouched.
    if (libusb_kernel_driver_active(handle_, audio_control) == 0 &&
        libusb_claim_interface(handle_, audio_control) == 0) {
        interface_ = audio_control;
        claimed_ = true;
        route_ = "audio-control interface " + std::to_string(interface_) + " (no kernel driver bound)";
        return;
    }
    for (int v : vendor) {
        if (libusb_kernel_driver_active(handle_, v) == 0 && libusb_claim_interface(handle_, v) == 0) {
            interface_ = v;
            claimed_ = true;
            route_ = "vendor interface " + std::to_string(v) + " (snd-usb-audio keeps interface " +
                     std::to_string(audio_control) + ")";
            return;
        }
    }
    if (!options.detach_audio)
        throw UsbError("no free interface to send control requests through; retry with "
                       "--detach-audio (interrupts audio while the tool runs)");
    libusb_set_auto_detach_kernel_driver(handle_, 1);
    if (int r = libusb_claim_interface(handle_, audio_control); r != 0)
        throw UsbError(usb_error("cannot detach snd-usb-audio", r));
    interface_ = audio_control;
    claimed_ = true;
    detached_ = true;
    route_ = "audio-control interface " + std::to_string(interface_) + " (snd-usb-audio detached)";
#else
    // macOS and Windows deliver control requests without claiming the interface, which is
    // what Wave Link itself does.
    (void)vendor;
    interface_ = audio_control;
    route_ = "audio-control interface " + std::to_string(interface_);
#endif
}

UsbTransport::~UsbTransport() {
    if (claimed_) libusb_release_interface(handle_, interface_);
    // Re-binding only the audio-control interface leaves snd-usb-audio without its streams;
    // a reset makes the kernel rebind the whole device.
    if (detached_) libusb_reset_device(handle_);
    libusb_close(handle_);
    libusb_exit(ctx_);
}

std::vector<uint8_t> UsbTransport::get_mem(uint8_t id, uint16_t length) {
    std::vector<uint8_t> buf(length);
    const uint16_t index = uint16_t(kMemEntity << 8 | interface_);
    int r = libusb_control_transfer(handle_, kClassInterfaceIn, kGetMem, id, index, buf.data(),
                                    length, kTimeoutMs);
    if (r < 0) throw UsbError(usb_error(("GET_MEM " + std::to_string(id)).c_str(), r));
    if (r != length)
        throw UsbError("GET_MEM " + std::to_string(id) + ": short read (" + std::to_string(r) +
                       " of " + std::to_string(length) + " bytes)");
    return buf;
}

void UsbTransport::set_mem(uint8_t id, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> buf = data;
    const uint16_t index = uint16_t(kMemEntity << 8 | interface_);
    int r = libusb_control_transfer(handle_, kClassInterfaceOut, kSetMem, id, index, buf.data(),
                                    uint16_t(buf.size()), kTimeoutMs);
    if (r < 0) throw UsbError(usb_error(("SET_MEM " + std::to_string(id)).c_str(), r));
    if (size_t(r) != buf.size())
        throw UsbError("SET_MEM " + std::to_string(id) + ": short write");
}

}  // namespace wave3
