// USB access to the Wave:3 vendor messages.
//
// Messages travel as USB Audio Class 1 memory requests (GET_MEM 0x85 / SET_MEM 0x05) sent
// as class requests to an interface, with wValue = message id and
// wIndex = (0x33 << 8) | interface number.
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct libusb_context;
struct libusb_device_handle;

namespace wave3 {

constexpr uint16_t kElgatoVid = 0x0fd9;
constexpr uint16_t kWave3Pid = 0x0070;
constexpr uint16_t kWave3DfuPid = 0x0071;  // same mic while in firmware-update mode

class UsbError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Transport {
public:
    virtual ~Transport() = default;
    virtual std::vector<uint8_t> get_mem(uint8_t id, uint16_t length) = 0;
    virtual void set_mem(uint8_t id, const std::vector<uint8_t>& data) = 0;
};

struct DeviceEntry {
    uint8_t bus;
    uint8_t address;
    uint16_t pid;
    std::string serial;  // empty when the device could not be opened
    std::string error;   // why it could not be opened, if it could not
};

struct OpenOptions {
    std::string serial;          // empty: first Wave:3 found
    int interface_number = -1;   // -1: choose automatically
    bool detach_audio = false;   // Linux: allow detaching snd-usb-audio as a last resort
};

class UsbTransport : public Transport {
public:
    explicit UsbTransport(const OpenOptions& options);
    ~UsbTransport() override;
    UsbTransport(const UsbTransport&) = delete;
    UsbTransport& operator=(const UsbTransport&) = delete;

    std::vector<uint8_t> get_mem(uint8_t id, uint16_t length) override;
    void set_mem(uint8_t id, const std::vector<uint8_t>& data) override;

    const std::string& serial() const { return serial_; }
    int interface_number() const { return interface_; }
    // How the control interface was chosen, for diagnostics.
    const std::string& route() const { return route_; }

    static std::vector<DeviceEntry> list();

private:
    void choose_interface(const OpenOptions& options);

    libusb_context* ctx_ = nullptr;
    libusb_device_handle* handle_ = nullptr;
    std::string serial_;
    int interface_ = 0;
    bool claimed_ = false;
    bool detached_ = false;
    std::string route_;
};

}  // namespace wave3
