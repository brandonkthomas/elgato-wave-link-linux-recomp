// Read-only: does the Wave:3 firmware care which interface number is in wIndex's low byte?
#include <libusb.h>
#include <stdio.h>

int main(void) {
    libusb_context *ctx;
    if (libusb_init(&ctx)) return 1;
    libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, 0x0fd9, 0x0070);
    if (!h) return 2;
    for (int ifnum = 0; ifnum <= 5; ifnum++) {
        unsigned char buf[16] = {0};
        int r = libusb_control_transfer(h, 0xA1, 0x85, 0, (uint16_t)(0x3300 | ifnum), buf, 16, 500);
        printf("wIndex 0x%04x -> %3d %-18s", 0x3300 | ifnum, r, r < 0 ? libusb_error_name(r) : "");
        for (int i = 0; i < (r > 0 ? r : 0); i++) printf(" %02x", buf[i]);
        printf("\n");
    }
    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}
