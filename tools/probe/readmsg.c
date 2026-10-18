// Read-only: GET_MEM the Wave:3 /config (0), /status (1) and /version (10) messages and hex-dump them.
#include <libusb.h>
#include <stdio.h>

static void dump(libusb_device_handle *h, const char *name, uint16_t id, uint16_t len) {
    unsigned char buf[64] = {0};
    int r = libusb_control_transfer(h, 0xA1, 0x85, id, 0x3300, buf, len, 500);
    printf("%-9s id %2u len %2u -> %d:", name, id, len, r);
    for (int i = 0; i < (r > 0 ? r : 0); i++) printf(" %02x", buf[i]);
    printf("\n");
    if (r > 0) {
        printf("%-9s ascii: ", "");
        for (int i = 0; i < r; i++) putchar(buf[i] >= 32 && buf[i] < 127 ? buf[i] : '.');
        printf("\n");
    }
}

int main(void) {
    libusb_context *ctx;
    if (libusb_init(&ctx)) return 1;
    libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, 0x0fd9, 0x0070);
    if (!h) { fprintf(stderr, "Wave:3 not found\n"); return 2; }
    dump(h, "/config", 0, 16);
    dump(h, "/status", 1, 8);
    dump(h, "/version", 10, 52);
    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}
