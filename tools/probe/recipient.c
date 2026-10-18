// Read-only: does the Wave:3 answer GET_MEM when bmRequestType uses device/other recipients?
#include <libusb.h>
#include <stdio.h>

int main(void) {
    libusb_context *ctx;
    if (libusb_init(&ctx)) return 1;
    libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, 0x0fd9, 0x0070);
    if (!h) return 2;
    const struct { unsigned char bm; const char *name; } kinds[] = {
        {0xA1, "class|interface"}, {0xA0, "class|device"}, {0xA3, "class|other"}, {0xC0, "vendor|device"}, {0xC1, "vendor|interface"},
    };
    for (unsigned k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
        unsigned char buf[16] = {0};
        int r = libusb_control_transfer(h, kinds[k].bm, 0x85, 0, 0x3300, buf, 16, 500);
        printf("bmRequestType 0x%02x %-17s -> %3d %-14s", kinds[k].bm, kinds[k].name, r, r < 0 ? libusb_error_name(r) : "");
        for (int i = 0; i < (r > 0 ? r : 0); i++) printf(" %02x", buf[i]);
        printf("\n");
    }
    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}
