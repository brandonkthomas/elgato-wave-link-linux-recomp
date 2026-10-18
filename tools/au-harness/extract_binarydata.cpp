// Dump every JUCE BinaryData resource of a plugin binary by calling its own exported
// BinaryData::getNamedResource / getNamedResourceOriginalFilename.
// usage: extract_binarydata <plugin-macho> <outdir>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

typedef const char* (*GetNamedResource)(const char*, int&);
typedef const char* (*GetOrigName)(const char*);

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <plugin-macho> <outdir>\n", argv[0]); return 1; }
    void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    auto get = (GetNamedResource) dlsym(h, "_ZN10BinaryData16getNamedResourceEPKcRi");
    auto orig = (GetOrigName) dlsym(h, "_ZN10BinaryData32getNamedResourceOriginalFilenameEPKc");
    auto list = (const char**) dlsym(h, "_ZN10BinaryData17namedResourceListE");
    auto origList = (const char**) dlsym(h, "_ZN10BinaryData17originalFilenamesE");
    if (!get || !list || !origList) { fprintf(stderr, "symbols missing\n"); return 1; }
    size_t n = (size_t) (origList - list);   // originalFilenames directly follows namedResourceList
    mkdir(argv[2], 0755);
    printf("%zu resources\n", n);
    for (size_t i = 0; i < n; ++i) {
        int size = 0;
        const char* data = get(list[i], size);
        const char* of = orig ? orig(list[i]) : nullptr;
        std::string fname = std::string(argv[2]) + "/" + (of ? of : list[i]);
        FILE* f = fopen(fname.c_str(), "wb");
        if (f && data) { fwrite(data, 1, (size_t) size, f); fclose(f); }
        printf("%-40s %-45s %9d bytes\n", list[i], of ? of : "?", size);
    }
    return 0;
}
