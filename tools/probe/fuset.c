// Send the same SET_CUR an OS mixer sends: UAC1 Feature Unit <unit> volume, master channel.
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: fuset <unit> <dB>\n"); return 2; }
    int unit = atoi(argv[1]);
    int16_t raw = (int16_t)(atof(argv[2]) * 256.0);
    libusb_context *ctx;
    if (libusb_init(&ctx)) return 1;
    libusb_device_handle *h = libusb_open_device_with_vid_pid(ctx, 0x0fd9, 0x0070);
    if (!h) return 2;
    unsigned char b[2] = {(unsigned char)(raw & 0xff), (unsigned char)((uint16_t)raw >> 8)};
    int r = libusb_control_transfer(h, 0x21, 0x01, 0x0200, (uint16_t)(unit << 8), b, 2, 500);
    printf("SET_CUR FU%d volume %s dB -> %d\n", unit, argv[2], r);
    libusb_close(h);
    libusb_exit(ctx);
    return r == 2 ? 0 : 1;
}
