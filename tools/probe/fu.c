// Read-only: UAC1 Feature Unit 5 (mic) volume/mute as the OS sees it.
#include <libusb.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static int unit = 5;
static int get(libusb_device_handle *h, uint8_t req, uint8_t cs, int16_t *out, int len) {
    unsigned char b[2] = {0};
    int r = libusb_control_transfer(h, 0xA1, req, (uint16_t)(cs << 8), (uint16_t)(unit << 8), b, len, 500);
    *out = (int16_t)(b[0] | (len > 1 ? b[1] << 8 : 0));
    return r;
}

int main(int argc, char **argv) {
    if (argc > 1) unit = atoi(argv[1]);
    libusb_context *ctx;
    if (libusb_init(&ctx)) return 1;
    libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, 0x0fd9, 0x0070);
    if (!h) return 2;
    const char *names[] = {"CUR", "MIN", "MAX", "RES"};
    for (int i = 0; i < 4; i++) {
        int16_t v; int r = get(h, 0x81 + i, 2, &v, 2);
        printf("FU%d volume %s -> r=%d raw=0x%04x (%.2f dB)\n", unit, names[i], r, (uint16_t)v, v / 256.0);
    }
    int16_t m; int r = get(h, 0x81, 1, &m, 1);
    printf("FU%d mute CUR -> r=%d %d\n", unit, r, m & 0xff);
    libusb_close(h);
    libusb_exit(ctx);
    return 0;
}
