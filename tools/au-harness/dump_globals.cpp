// Print juce::String / juce::Colour globals of a loaded plugin image after its static initialisers ran.
// usage: dump_globals <plugin-macho> <anchor-symbol> <anchor-vmaddr-hex> <s|c>:<vmaddr-hex>[:label] ...
//   anchor: any exported symbol and its vmaddr from `nm`, used to compute the ASLR slide.
//   s = juce::String (pointer to UTF-8 text), c = juce::Colour (uint32 ARGB).
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage\n"); return 1; }
    void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    void* anchor = dlsym(h, argv[2]);
    if (!anchor) { fprintf(stderr, "no anchor %s\n", argv[2]); return 1; }
    uintptr_t slide = (uintptr_t) anchor - (uintptr_t) strtoull(argv[3], nullptr, 16);
    for (int i = 4; i < argc; ++i) {
        char kind = argv[i][0];
        char* rest = argv[i] + 2;
        char* label = strchr(rest, ':');
        if (label) *label++ = 0;
        uintptr_t addr = slide + (uintptr_t) strtoull(rest, nullptr, 16);
        if (kind == 's') {
            const char* text = *(const char**) addr;
            printf("%-12s %-34s \"%s\"\n", rest, label ? label : "", text ? text : "(null)");
        } else if (kind == 'c') {
            uint32_t argb = *(uint32_t*) addr;
            printf("%-12s %-34s ARGB 0x%08x\n", rest, label ? label : "", argb);
        } else if (kind == 'f') {
            printf("%-12s %-34s %g\n", rest, label ? label : "", *(float*) addr);
        } else if (kind == 'i') {
            printf("%-12s %-34s %d\n", rest, label ? label : "", *(int*) addr);
        }
    }
    return 0;
}
