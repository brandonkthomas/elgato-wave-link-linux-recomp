// Read-only probe: dump Wave:3 audio-control descriptors and query the vendor protocol version.
#include <libusb.h>
#include <stdio.h>

static const char *ac_subtype(int st) {
    switch (st) {
    case 1: return "HEADER"; case 2: return "INPUT_TERMINAL"; case 3: return "OUTPUT_TERMINAL";
    case 4: return "MIXER_UNIT"; case 5: return "SELECTOR_UNIT"; case 6: return "FEATURE_UNIT";
    case 7: return "PROCESSING_UNIT"; case 8: return "EXTENSION_UNIT"; default: return "?";
    }
}

int main(void) {
    libusb_context *ctx;
    if (libusb_init(&ctx)) return 1;
    libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, 0x0fd9, 0x0070);
    if (!h) { fprintf(stderr, "Wave:3 not found / not openable\n"); return 2; }
    struct libusb_config_descriptor *cfg;
    if (libusb_get_active_config_descriptor(libusb_get_device(h), &cfg)) return 3;
    int ac_if = -1;
    for (int i = 0; i < cfg->bNumInterfaces; i++) {
        for (int a = 0; a < cfg->interface[i].num_altsetting; a++) {
            const struct libusb_interface_descriptor *d = &cfg->interface[i].altsetting[a];
            printf("if %d alt %d class %d sub %d proto %d eps %d extra %d\n", d->bInterfaceNumber,
                   d->bAlternateSetting, d->bInterfaceClass, d->bInterfaceSubClass,
                   d->bInterfaceProtocol, d->bNumEndpoints, d->extra_length);
            if (d->bInterfaceClass == 1 && d->bInterfaceSubClass == 1) {
                ac_if = d->bInterfaceNumber;
                for (int off = 0; off + 2 < d->extra_length; off += d->extra[off]) {
                    const unsigned char *x = d->extra + off;
                    if (x[0] == 0) break;
                    printf("  AC desc len %2d type 0x%02x %-16s id %3d :", x[0], x[1],
                           x[1] == 0x24 ? ac_subtype(x[2]) : "", x[1] == 0x24 && x[2] > 1 ? x[3] : -1);
                    for (int k = 0; k < x[0]; k++) printf(" %02x", x[k]);
                    printf("\n");
                }
            }
        }
    }
    printf("audio control interface = %d\n", ac_if);
    unsigned char buf[2] = {0};
    // UAC1 GET_MEM (0x85), class request to interface, wValue = 10, wIndex = (0x33 << 8) | ac_if
    int r = libusb_control_transfer(h, 0xA1, 0x85, 10, (uint16_t)(0x3300 | ac_if), buf, 2, 500);
    printf("GET_MEM(10) iface-recipient -> %d (%s) data %02x %02x\n", r, r < 0 ? libusb_error_name(r) : "ok", buf[0], buf[1]);
    libusb_free_config_descriptor(cfg);
    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}
