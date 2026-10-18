// auhost - minimal offline AudioUnit (aufx) host that loads a .component bundle *by path*
// (no installation), registers its factory in-process with AudioComponentRegister and renders
// a signal through it.  Used to capture ground truth for the Elgato EQ / Noise Removal plugins.
//
// build: clang++ -std=c++17 -O2 -o auhost auhost.cpp -framework AudioToolbox -framework CoreFoundation
//
// usage: auhost --bundle <X.component> [options]
//   --list                       print parameters (AU id, name, unit, min/max/default, flags), latency, tail
//   --sr <hz>                    sample rate (default 48000)
//   --ch <n>                     channels (default 1)
//   --block <n>                  frames per render call (default 480)
//   --in <file>                  input audio file (any format ExtAudioFile reads; converted to --sr, --ch)
//   --gen impulse:<len>[:amp[:pos]] | silence:<len> | sine:<hz>:<len>:<amp> | noise:<len>:<amp>[:seed]
//   --pad <n>                    append n frames of silence (flush latency) (default 0)
//   --set <name|id>=<value>      AU parameter value (JUCE AU units: floats 0..1 normalised, discrete 0..steps-1)
//   --bypass <0|1>               kAudioUnitProperty_BypassEffect
//   --state-out <file.plist>     write kAudioUnitProperty_ClassInfo after processing
//   --state-in <file.plist>      apply kAudioUnitProperty_ClassInfo before processing
//   --out <file.wav>             32-bit float WAV output
//   --pump <ms>                  run the CFRunLoop this long after setup (lets async JUCE work finish) (default 50)
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

static void die(const char* what, OSStatus st = 0) {
    fprintf(stderr, "error: %s (%d)\n", what, (int) st);
    exit(1);
}
#define CHECK(expr, what) do { OSStatus _s = (expr); if (_s != noErr) die(what, _s); } while (0)

static std::string cfstr(CFStringRef s) {
    if (!s) return "";
    char buf[1024];
    if (CFStringGetCString(s, buf, sizeof buf, kCFStringEncodingUTF8)) return buf;
    return "?";
}

static OSType fourcc(CFStringRef s) {
    std::string t = cfstr(s);
    if (t.size() != 4) die("bad four-char code in Info.plist");
    return (OSType) ((uint8_t) t[0] << 24 | (uint8_t) t[1] << 16 | (uint8_t) t[2] << 8 | (uint8_t) t[3]);
}

struct Source {
    std::vector<std::vector<float>> data;  // [ch][frame]
    size_t frames = 0;
};

static Source* g_src = nullptr;
static size_t g_pos = 0;

static OSStatus inputCallback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32 frames,
                              AudioBufferList* io) {
    for (UInt32 b = 0; b < io->mNumberBuffers; ++b) {
        float* dst = (float*) io->mBuffers[b].mData;
        const auto& src = g_src->data[std::min<size_t>(b, g_src->data.size() - 1)];
        for (UInt32 i = 0; i < frames; ++i) {
            size_t p = g_pos + i;
            dst[i] = p < g_src->frames ? src[p] : 0.0f;
        }
    }
    return noErr;
}

static Source readFile(const char* path, double sr, int ch) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*) path, (CFIndex) strlen(path), false);
    ExtAudioFileRef f;
    CHECK(ExtAudioFileOpenURL(url, &f), "ExtAudioFileOpenURL");
    CFRelease(url);
    AudioStreamBasicDescription fileFmt{};
    UInt32 sz = sizeof fileFmt;
    CHECK(ExtAudioFileGetProperty(f, kExtAudioFileProperty_FileDataFormat, &sz, &fileFmt), "file format");
    AudioStreamBasicDescription cl{};
    cl.mSampleRate = sr;
    cl.mFormatID = kAudioFormatLinearPCM;
    cl.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    cl.mChannelsPerFrame = (UInt32) ch;
    cl.mBitsPerChannel = 32;
    cl.mBytesPerFrame = cl.mBytesPerPacket = 4;
    cl.mFramesPerPacket = 1;
    CHECK(ExtAudioFileSetProperty(f, kExtAudioFileProperty_ClientDataFormat, sizeof cl, &cl), "client format");
    if (fileFmt.mSampleRate != sr)
        fprintf(stderr, "note: converting %s from %.0f Hz to %.0f Hz\n", path, fileFmt.mSampleRate, sr);
    Source s;
    s.data.assign((size_t) ch, {});
    const UInt32 chunk = 4096;
    std::vector<std::vector<float>> tmp((size_t) ch, std::vector<float>(chunk));
    std::vector<uint8_t> ablMem(sizeof(AudioBufferList) + sizeof(AudioBuffer) * (size_t) ch);
    auto* abl = (AudioBufferList*) ablMem.data();
    for (;;) {
        abl->mNumberBuffers = (UInt32) ch;
        for (int c = 0; c < ch; ++c) {
            abl->mBuffers[c].mNumberChannels = 1;
            abl->mBuffers[c].mDataByteSize = chunk * 4;
            abl->mBuffers[c].mData = tmp[(size_t) c].data();
        }
        UInt32 n = chunk;
        CHECK(ExtAudioFileRead(f, &n, abl), "ExtAudioFileRead");
        if (n == 0) break;
        for (int c = 0; c < ch; ++c) s.data[(size_t) c].insert(s.data[(size_t) c].end(), tmp[(size_t) c].begin(), tmp[(size_t) c].begin() + n);
    }
    ExtAudioFileDispose(f);
    s.frames = s.data[0].size();
    return s;
}

static void writeWav(const char* path, const std::vector<std::vector<float>>& d, double sr) {
    int ch = (int) d.size();
    size_t frames = d[0].size();
    AudioStreamBasicDescription ff{};
    ff.mSampleRate = sr;
    ff.mFormatID = kAudioFormatLinearPCM;
    ff.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    ff.mChannelsPerFrame = (UInt32) ch;
    ff.mBitsPerChannel = 32;
    ff.mBytesPerFrame = ff.mBytesPerPacket = (UInt32) (4 * ch);
    ff.mFramesPerPacket = 1;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*) path, (CFIndex) strlen(path), false);
    ExtAudioFileRef f;
    CHECK(ExtAudioFileCreateWithURL(url, kAudioFileWAVEType, &ff, nullptr, kAudioFileFlags_EraseFile, &f), "create wav");
    CFRelease(url);
    AudioStreamBasicDescription cl = ff;
    cl.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    cl.mBytesPerFrame = cl.mBytesPerPacket = 4;
    CHECK(ExtAudioFileSetProperty(f, kExtAudioFileProperty_ClientDataFormat, sizeof cl, &cl), "wav client fmt");
    std::vector<uint8_t> ablMem(sizeof(AudioBufferList) + sizeof(AudioBuffer) * (size_t) ch);
    auto* abl = (AudioBufferList*) ablMem.data();
    abl->mNumberBuffers = (UInt32) ch;
    for (int c = 0; c < ch; ++c) {
        abl->mBuffers[c].mNumberChannels = 1;
        abl->mBuffers[c].mDataByteSize = (UInt32) (frames * 4);
        abl->mBuffers[c].mData = (void*) d[(size_t) c].data();
    }
    CHECK(ExtAudioFileWrite(f, (UInt32) frames, abl), "wav write");
    ExtAudioFileDispose(f);
}

static const char* unitName(AudioUnitParameterUnit u) {
    switch (u) {
        case kAudioUnitParameterUnit_Generic: return "Generic";
        case kAudioUnitParameterUnit_Indexed: return "Indexed";
        case kAudioUnitParameterUnit_Boolean: return "Boolean";
        case kAudioUnitParameterUnit_Percent: return "Percent";
        case kAudioUnitParameterUnit_Seconds: return "Seconds";
        case kAudioUnitParameterUnit_Hertz: return "Hertz";
        case kAudioUnitParameterUnit_Decibels: return "Decibels";
        case kAudioUnitParameterUnit_LinearGain: return "LinearGain";
        case kAudioUnitParameterUnit_Milliseconds: return "Milliseconds";
        default: return "other";
    }
}

struct ParamDesc { AudioUnitParameterID id; std::string name; AudioUnitParameterInfo info; };

static std::vector<ParamDesc> listParams(AudioUnit au) {
    std::vector<ParamDesc> out;
    UInt32 sz = 0;
    Boolean w;
    if (AudioUnitGetPropertyInfo(au, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, &sz, &w) != noErr) return out;
    std::vector<AudioUnitParameterID> ids(sz / sizeof(AudioUnitParameterID));
    CHECK(AudioUnitGetProperty(au, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, ids.data(), &sz), "param list");
    for (auto id : ids) {
        AudioUnitParameterInfo info{};
        UInt32 isz = sizeof info;
        CHECK(AudioUnitGetProperty(au, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, id, &info, &isz), "param info");
        std::string name = (info.flags & kAudioUnitParameterFlag_HasCFNameString) ? cfstr(info.cfNameString) : std::string(info.name);
        out.push_back({id, name, info});
    }
    return out;
}

static double parseNum(const std::string& s) { return strtod(s.c_str(), nullptr); }

int main(int argc, char** argv) {
    std::string bundlePath, inPath, outPath, gen, stateOut, stateIn;
    double sr = 48000;
    int ch = 1, block = 480, pad = 0, bypass = -1, pumpMs = 50;
    bool list = false;
    std::vector<std::pair<std::string, double>> sets;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) die(("missing value for " + a).c_str()); return argv[++i]; };
        if (a == "--bundle") bundlePath = next();
        else if (a == "--list") list = true;
        else if (a == "--sr") sr = parseNum(next());
        else if (a == "--ch") ch = atoi(next().c_str());
        else if (a == "--block") block = atoi(next().c_str());
        else if (a == "--in") inPath = next();
        else if (a == "--gen") gen = next();
        else if (a == "--pad") pad = atoi(next().c_str());
        else if (a == "--out") outPath = next();
        else if (a == "--bypass") bypass = atoi(next().c_str());
        else if (a == "--state-out") stateOut = next();
        else if (a == "--state-in") stateIn = next();
        else if (a == "--pump") pumpMs = atoi(next().c_str());
        else if (a == "--set") {
            std::string kv = next();
            auto eq = kv.rfind('=');
            if (eq == std::string::npos) die("--set needs name=value");
            sets.push_back({kv.substr(0, eq), parseNum(kv.substr(eq + 1))});
        } else die(("unknown option " + a).c_str());
    }
    if (bundlePath.empty()) die("--bundle required");

    // --- load bundle and register its factory in-process --------------------------------------
    CFURLRef burl = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*) bundlePath.c_str(), (CFIndex) bundlePath.size(), true);
    CFBundleRef bundle = CFBundleCreate(nullptr, burl);
    if (!bundle) die("CFBundleCreate");
    auto comps = (CFArrayRef) CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("AudioComponents"));
    if (!comps || CFArrayGetCount(comps) < 1) die("no AudioComponents in Info.plist");
    auto c0 = (CFDictionaryRef) CFArrayGetValueAtIndex(comps, 0);
    AudioComponentDescription desc{};
    desc.componentType = fourcc((CFStringRef) CFDictionaryGetValue(c0, CFSTR("type")));
    desc.componentSubType = fourcc((CFStringRef) CFDictionaryGetValue(c0, CFSTR("subtype")));
    desc.componentManufacturer = fourcc((CFStringRef) CFDictionaryGetValue(c0, CFSTR("manufacturer")));
    auto factoryName = (CFStringRef) CFDictionaryGetValue(c0, CFSTR("factoryFunction"));
    auto compName = (CFStringRef) CFDictionaryGetValue(c0, CFSTR("name"));
    UInt32 version = 0;
    if (auto v = (CFNumberRef) CFDictionaryGetValue(c0, CFSTR("version"))) CFNumberGetValue(v, kCFNumberSInt32Type, &version);
    CFErrorRef err = nullptr;
    if (!CFBundleLoadExecutableAndReturnError(bundle, &err)) die("CFBundleLoadExecutable");
    auto factory = (AudioComponentFactoryFunction) CFBundleGetFunctionPointerForName(bundle, factoryName);
    if (!factory) die("factory function not found");
    AudioComponent comp = AudioComponentRegister(&desc, compName, version, factory);
    if (!comp) die("AudioComponentRegister");
    AudioUnit au;
    CHECK(AudioComponentInstanceNew(comp, &au), "AudioComponentInstanceNew");
    fprintf(stderr, "loaded %s (version 0x%x)\n", cfstr(compName).c_str(), version);

    // --- formats -----------------------------------------------------------------------------
    AudioStreamBasicDescription fmt{};
    fmt.mSampleRate = sr;
    fmt.mFormatID = kAudioFormatLinearPCM;
    fmt.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    fmt.mChannelsPerFrame = (UInt32) ch;
    fmt.mBitsPerChannel = 32;
    fmt.mBytesPerFrame = fmt.mBytesPerPacket = 4;
    fmt.mFramesPerPacket = 1;
    CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &fmt, sizeof fmt), "input format");
    CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &fmt, sizeof fmt), "output format");
    UInt32 maxFrames = (UInt32) std::max(block, 4096);
    CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, sizeof maxFrames), "max frames");
    AURenderCallbackStruct cb{inputCallback, nullptr};
    CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof cb), "render callback");

    if (!stateIn.empty()) {
        CFURLRef surl = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*) stateIn.c_str(), (CFIndex) stateIn.size(), false);
        CFReadStreamRef rs = CFReadStreamCreateWithFile(nullptr, surl);
        CFReadStreamOpen(rs);
        CFPropertyListRef pl = CFPropertyListCreateWithStream(nullptr, rs, 0, kCFPropertyListImmutable, nullptr, nullptr);
        CFReadStreamClose(rs);
        if (!pl) die("cannot read state plist");
        CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &pl, sizeof pl), "set ClassInfo");
    }

    CHECK(AudioUnitInitialize(au), "AudioUnitInitialize");

    auto params = listParams(au);
    for (auto& [key, val] : sets) {
        bool found = false;
        for (auto& p : params) {
            if (p.name == key || std::to_string(p.id) == key) {
                CHECK(AudioUnitSetParameter(au, p.id, kAudioUnitScope_Global, 0, (AudioUnitParameterValue) val, 0), "set param");
                found = true;
            }
        }
        if (!found) die(("no parameter " + key).c_str());
    }
    if (bypass >= 0) {
        UInt32 b = (UInt32) bypass;
        CHECK(AudioUnitSetProperty(au, kAudioUnitProperty_BypassEffect, kAudioUnitScope_Global, 0, &b, sizeof b), "bypass");
    }
    if (pumpMs > 0) CFRunLoopRunInMode(kCFRunLoopDefaultMode, pumpMs / 1000.0, false);

    if (list) {
        Float64 lat = 0, tail = 0;
        UInt32 sz = sizeof lat;
        AudioUnitGetProperty(au, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &lat, &sz);
        sz = sizeof tail;
        AudioUnitGetProperty(au, kAudioUnitProperty_TailTime, kAudioUnitScope_Global, 0, &tail, &sz);
        printf("latency_s=%.9f tail_s=%.9f\n", lat, tail);
        UInt32 isz = 0; Boolean w;
        if (AudioUnitGetPropertyInfo(au, kAudioUnitProperty_SupportedNumChannels, kAudioUnitScope_Global, 0, &isz, &w) == noErr && isz) {
            std::vector<AUChannelInfo> ci(isz / sizeof(AUChannelInfo));
            AudioUnitGetProperty(au, kAudioUnitProperty_SupportedNumChannels, kAudioUnitScope_Global, 0, ci.data(), &isz);
            printf("channel configs:");
            for (auto& c : ci) printf(" [%d in, %d out]", c.inChannels, c.outChannels);
            printf("\n");
        }
        printf("%-12s %-36s %-9s %10s %10s %10s %-10s %s\n", "au_id", "name", "unit", "min", "max", "default", "flags", "current");
        for (auto& p : params) {
            AudioUnitParameterValue cur = 0;
            AudioUnitGetParameter(au, p.id, kAudioUnitScope_Global, 0, &cur);
            printf("%-12u %-36s %-9s %10g %10g %10g 0x%08x %g\n", p.id, ("\"" + p.name + "\"").c_str(), unitName(p.info.unit),
                   p.info.minValue, p.info.maxValue, p.info.defaultValue, p.info.flags, cur);
            if (p.info.unit == kAudioUnitParameterUnit_Indexed) {
                CFArrayRef strs = nullptr;
                UInt32 ssz = sizeof strs;
                if (AudioUnitGetProperty(au, kAudioUnitProperty_ParameterValueStrings, kAudioUnitScope_Global, p.id, &strs, &ssz) == noErr && strs) {
                    printf("             values:");
                    for (CFIndex k = 0; k < CFArrayGetCount(strs) && k < 12; ++k) printf(" [%ld]=%s", (long) k, cfstr((CFStringRef) CFArrayGetValueAtIndex(strs, k)).c_str());
                    if (CFArrayGetCount(strs) > 12) printf(" ... (%ld total)", (long) CFArrayGetCount(strs));
                    printf("\n");
                    CFRelease(strs);
                }
            }
            // display strings for a few values via kAudioUnitProperty_ParameterStringFromValue
            AudioUnitParameterStringFromValue sv{};
            sv.inParamID = p.id;
            for (float v : {p.info.minValue, p.info.defaultValue, p.info.maxValue}) {
                sv.inValue = &v;
                sv.outString = nullptr;
                UInt32 svsz = sizeof sv;
                if (AudioUnitGetProperty(au, kAudioUnitProperty_ParameterStringFromValue, kAudioUnitScope_Global, 0, &sv, &svsz) == noErr && sv.outString) {
                    printf("             text(%g)=\"%s\"\n", v, cfstr(sv.outString).c_str());
                    CFRelease(sv.outString);
                }
            }
        }
    }

    // --- source -----------------------------------------------------------------------------
    Source src;
    if (!inPath.empty()) src = readFile(inPath.c_str(), sr, ch);
    else if (!gen.empty()) {
        std::vector<std::string> f;
        size_t s0 = 0;
        for (size_t k = 0; k <= gen.size(); ++k)
            if (k == gen.size() || gen[k] == ':') { f.push_back(gen.substr(s0, k - s0)); s0 = k + 1; }
        auto num = [&](size_t k, double d) { return k < f.size() ? parseNum(f[k]) : d; };
        std::vector<float> mono;
        if (f[0] == "impulse") {
            mono.assign((size_t) num(1, 8192), 0.0f);
            size_t pos = (size_t) num(3, 0);
            if (pos < mono.size()) mono[pos] = (float) num(2, 1.0);
        } else if (f[0] == "silence") {
            mono.assign((size_t) num(1, 48000), 0.0f);
        } else if (f[0] == "sine") {
            double hz = num(1, 1000), amp = num(3, 0.5);
            mono.resize((size_t) num(2, 48000));
            for (size_t k = 0; k < mono.size(); ++k) mono[k] = (float) (amp * std::sin(2 * M_PI * hz * (double) k / sr));
        } else if (f[0] == "noise") {
            std::mt19937 rng((unsigned) num(3, 1));
            std::uniform_real_distribution<float> u(-1.0f, 1.0f);
            float amp = (float) num(2, 0.1);
            mono.resize((size_t) num(1, 48000));
            for (auto& x : mono) x = amp * u(rng);
        } else die("unknown generator");
        src.data.assign((size_t) ch, mono);
        src.frames = mono.size();
    } else if (!list) die("need --in or --gen");

    if (src.frames > 0) {
        for (auto& c : src.data) c.resize(src.frames + (size_t) pad, 0.0f);
        src.frames += (size_t) pad;
        g_src = &src;
        std::vector<std::vector<float>> out((size_t) ch, std::vector<float>(src.frames, 0.0f));
        std::vector<std::vector<float>> obuf((size_t) ch, std::vector<float>((size_t) block));
        std::vector<uint8_t> ablMem(sizeof(AudioBufferList) + sizeof(AudioBuffer) * (size_t) ch);
        auto* abl = (AudioBufferList*) ablMem.data();
        AudioTimeStamp ts{};
        ts.mFlags = kAudioTimeStampSampleTimeValid;
        for (g_pos = 0; g_pos < src.frames;) {
            UInt32 n = (UInt32) std::min<size_t>((size_t) block, src.frames - g_pos);
            abl->mNumberBuffers = (UInt32) ch;
            for (int c = 0; c < ch; ++c) {
                abl->mBuffers[c].mNumberChannels = 1;
                abl->mBuffers[c].mDataByteSize = n * 4;
                abl->mBuffers[c].mData = obuf[(size_t) c].data();
            }
            AudioUnitRenderActionFlags flags = 0;
            ts.mSampleTime = (Float64) g_pos;
            CHECK(AudioUnitRender(au, &flags, &ts, 0, n, abl), "AudioUnitRender");
            for (int c = 0; c < ch; ++c) memcpy(out[(size_t) c].data() + g_pos, abl->mBuffers[c].mData, n * 4);
            g_pos += n;
        }
        if (!outPath.empty()) writeWav(outPath.c_str(), out, sr);
        fprintf(stderr, "rendered %zu frames x %d ch at %.0f Hz, block %d\n", src.frames, ch, sr, block);
    }

    if (!stateOut.empty()) {
        CFPropertyListRef pl = nullptr;
        UInt32 sz = sizeof pl;
        CHECK(AudioUnitGetProperty(au, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &pl, &sz), "get ClassInfo");
        CFDataRef xml = CFPropertyListCreateData(nullptr, pl, kCFPropertyListXMLFormat_v1_0, 0, nullptr);
        FILE* f = fopen(stateOut.c_str(), "wb");
        if (f) { fwrite(CFDataGetBytePtr(xml), 1, (size_t) CFDataGetLength(xml), f); fclose(f); }
        CFRelease(xml);
        CFRelease(pl);
    }

    AudioUnitUninitialize(au);
    AudioComponentInstanceDispose(au);
    return 0;
}
