// sigtool - small signal utilities for the ground-truth captures (no external deps).
//
// build: clang++ -std=c++17 -O2 -o sigtool sigtool.cpp -framework AudioToolbox -framework CoreFoundation
//
//   sigtool mix <speech-file> <out.wav> <snr_db> <seed> [channels=1] [lead_silence_s=1.0]
//       speech (resampled to 48 kHz mono) + pink noise + 50/100/150 Hz hum, noise scaled to snr_db
//       relative to the speech RMS over active samples. channels=2 writes speech+noise in L and an
//       independent noise realisation (no speech) in R.
//   sigtool diff <a.wav> <b.wav> [maxlag=0]
//       per channel: max |a-b|, rms(a-b), SNR of a vs (a-b) in dB, and (if maxlag>0) the lag of b
//       relative to a that maximises cross-correlation.
//   sigtool fr <ir.wav> <out.csv> [points=400] [fmin=10] [fmax=24000] [scale=1]
//       magnitude (dB) / phase (deg) of the impulse response in channel 0 at log-spaced frequencies,
//       H(f) = sum h[n] e^{-j 2 pi f n / fs} / scale.
//   sigtool stats <file.wav> [block=480]
//       per-channel RMS (dBFS) per block (one line per block) — used to look at gating.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

typedef std::vector<std::vector<float>> Audio;

static void die(const char* m, OSStatus s = 0) { fprintf(stderr, "error: %s (%d)\n", m, (int) s); exit(1); }
#define CHECK(e, m) do { OSStatus _s = (e); if (_s != noErr) die(m, _s); } while (0)

static Audio readAudio(const char* path, double* srOut, double forceSr = 0, int forceCh = 0) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*) path, (CFIndex) strlen(path), false);
    ExtAudioFileRef f;
    CHECK(ExtAudioFileOpenURL(url, &f), "open");
    CFRelease(url);
    AudioStreamBasicDescription ff{};
    UInt32 sz = sizeof ff;
    CHECK(ExtAudioFileGetProperty(f, kExtAudioFileProperty_FileDataFormat, &sz, &ff), "fmt");
    int ch = forceCh ? forceCh : (int) ff.mChannelsPerFrame;
    double sr = forceSr ? forceSr : ff.mSampleRate;
    AudioStreamBasicDescription cl{};
    cl.mSampleRate = sr;
    cl.mFormatID = kAudioFormatLinearPCM;
    cl.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    cl.mChannelsPerFrame = (UInt32) ch;
    cl.mBitsPerChannel = 32;
    cl.mBytesPerFrame = cl.mBytesPerPacket = 4;
    cl.mFramesPerPacket = 1;
    CHECK(ExtAudioFileSetProperty(f, kExtAudioFileProperty_ClientDataFormat, sizeof cl, &cl), "client");
    Audio a((size_t) ch);
    const UInt32 chunk = 8192;
    Audio tmp((size_t) ch, std::vector<float>(chunk));
    std::vector<uint8_t> mem(sizeof(AudioBufferList) + sizeof(AudioBuffer) * (size_t) ch);
    auto* abl = (AudioBufferList*) mem.data();
    for (;;) {
        abl->mNumberBuffers = (UInt32) ch;
        for (int c = 0; c < ch; ++c) { abl->mBuffers[c].mNumberChannels = 1; abl->mBuffers[c].mDataByteSize = chunk * 4; abl->mBuffers[c].mData = tmp[(size_t) c].data(); }
        UInt32 n = chunk;
        CHECK(ExtAudioFileRead(f, &n, abl), "read");
        if (!n) break;
        for (int c = 0; c < ch; ++c) a[(size_t) c].insert(a[(size_t) c].end(), tmp[(size_t) c].begin(), tmp[(size_t) c].begin() + n);
    }
    ExtAudioFileDispose(f);
    if (srOut) *srOut = sr;
    return a;
}

static void writeWav(const char* path, const Audio& d, double sr) {
    int ch = (int) d.size();
    size_t frames = d[0].size();
    AudioStreamBasicDescription ff{};
    ff.mSampleRate = sr; ff.mFormatID = kAudioFormatLinearPCM; ff.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    ff.mChannelsPerFrame = (UInt32) ch; ff.mBitsPerChannel = 32; ff.mBytesPerFrame = ff.mBytesPerPacket = (UInt32) (4 * ch); ff.mFramesPerPacket = 1;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8*) path, (CFIndex) strlen(path), false);
    ExtAudioFileRef f;
    CHECK(ExtAudioFileCreateWithURL(url, kAudioFileWAVEType, &ff, nullptr, kAudioFileFlags_EraseFile, &f), "create");
    CFRelease(url);
    AudioStreamBasicDescription cl = ff;
    cl.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    cl.mBytesPerFrame = cl.mBytesPerPacket = 4;
    CHECK(ExtAudioFileSetProperty(f, kExtAudioFileProperty_ClientDataFormat, sizeof cl, &cl), "client");
    std::vector<uint8_t> mem(sizeof(AudioBufferList) + sizeof(AudioBuffer) * (size_t) ch);
    auto* abl = (AudioBufferList*) mem.data();
    abl->mNumberBuffers = (UInt32) ch;
    for (int c = 0; c < ch; ++c) { abl->mBuffers[c].mNumberChannels = 1; abl->mBuffers[c].mDataByteSize = (UInt32) (frames * 4); abl->mBuffers[c].mData = (void*) d[(size_t) c].data(); }
    CHECK(ExtAudioFileWrite(f, (UInt32) frames, abl), "write");
    ExtAudioFileDispose(f);
}

// Paul Kellet's refined pink noise filter on white noise.
struct Pink {
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    double next(double w) {
        b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759; b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856; b4 = 0.55000 * b4 + w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
        double p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362; b6 = w * 0.115926; return p * 0.11;
    }
};

static std::vector<float> makeNoise(size_t n, unsigned seed, double sr) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> g(0.0, 1.0);
    Pink pk;
    std::vector<float> v(n);
    for (size_t i = 0; i < n; ++i) {
        double t = (double) i / sr;
        double hum = 0.5 * std::sin(2 * M_PI * 50 * t) + 0.3 * std::sin(2 * M_PI * 100 * t + 0.3) + 0.2 * std::sin(2 * M_PI * 150 * t + 1.1);
        v[i] = (float) (pk.next(g(rng)) + 0.15 * hum + 0.05 * g(rng));
    }
    return v;
}

static double rms(const std::vector<float>& v, bool activeOnly) {
    double s = 0; size_t n = 0;
    for (float x : v) if (!activeOnly || std::fabs(x) > 1e-3f) { s += (double) x * x; ++n; }
    return n ? std::sqrt(s / (double) n) : 0.0;
}

int main(int argc, char** argv) {
    if (argc < 2) die("usage: see header");
    std::string cmd = argv[1];
    if (cmd == "mix" && argc >= 6) {
        double sr = 48000;
        Audio sp = readAudio(argv[2], nullptr, sr, 1);
        double snr = atof(argv[4]);
        unsigned seed = (unsigned) atoi(argv[5]);
        int ch = argc > 6 ? atoi(argv[6]) : 1;
        double lead = argc > 7 ? atof(argv[7]) : 1.0;
        size_t leadN = (size_t) (lead * sr);
        std::vector<float> speech(leadN, 0.0f);
        speech.insert(speech.end(), sp[0].begin(), sp[0].end());
        speech.resize(speech.size() + leadN, 0.0f);
        // normalise speech to -20 dBFS RMS (active)
        double r = rms(speech, true);
        for (auto& x : speech) x = (float) (x * (0.1 / r));
        Audio out((size_t) ch);
        for (int c = 0; c < ch; ++c) {
            std::vector<float> nz = makeNoise(speech.size(), seed + (unsigned) c * 7919u, sr);
            double nr = rms(nz, false);
            double target = 0.1 / std::pow(10.0, snr / 20.0);
            out[(size_t) c].resize(speech.size());
            for (size_t i = 0; i < speech.size(); ++i)
                out[(size_t) c][i] = (c == 0 ? speech[i] : 0.0f) + (float) (nz[i] * target / nr);
        }
        writeWav(argv[3], out, sr);
        printf("wrote %s: %zu frames, %d ch, speech -20 dBFS(active), SNR %.1f dB\n", argv[3], speech.size(), ch, snr);
        return 0;
    }
    if (cmd == "diff" && argc >= 4) {
        double sra, srb;
        Audio a = readAudio(argv[2], &sra), b = readAudio(argv[3], &srb);
        int maxlag = argc > 4 ? atoi(argv[4]) : 0;
        size_t chs = std::min(a.size(), b.size());
        for (size_t c = 0; c < chs; ++c) {
            size_t n = std::min(a[c].size(), b[c].size());
            double maxd = 0, sd = 0, sa = 0; size_t imax = 0, ndiff = 0;
            for (size_t i = 0; i < n; ++i) {
                double d = (double) a[c][i] - (double) b[c][i];
                if (std::fabs(d) > maxd) { maxd = std::fabs(d); imax = i; }
                if (d != 0) ++ndiff;
                sd += d * d; sa += (double) a[c][i] * a[c][i];
            }
            printf("ch%zu: n=%zu differing=%zu max|d|=%.3g (at %zu) rms(d)=%.3g snr=%.1f dB", c, n, ndiff, maxd, imax,
                   std::sqrt(sd / (double) n), sd > 0 ? 10 * std::log10(sa / sd) : 999.0);
            if (maxlag > 0) {
                double best = -1e300; int bl = 0;
                for (int lag = -maxlag; lag <= maxlag; ++lag) {
                    double s = 0;
                    for (size_t i = 0; i < n; ++i) { long j = (long) i + lag; if (j >= 0 && (size_t) j < n) s += (double) a[c][i] * b[c][(size_t) j]; }
                    if (s > best) { best = s; bl = lag; }
                }
                printf(" bestlag(b vs a)=%d", bl);
            }
            printf("\n");
        }
        return 0;
    }
    if (cmd == "fr" && argc >= 4) {
        double sr;
        Audio a = readAudio(argv[2], &sr);
        int pts = argc > 4 ? atoi(argv[4]) : 400;
        double fmin = argc > 5 ? atof(argv[5]) : 10, fmax = argc > 6 ? atof(argv[6]) : std::min(24000.0, sr / 2);
        double scale = argc > 7 ? atof(argv[7]) : 1.0;
        FILE* o = fopen(argv[3], "w");
        fprintf(o, "freq_hz,mag_db,phase_deg\n");
        const auto& h = a[0];
        for (int k = 0; k < pts; ++k) {
            double f = fmin * std::pow(fmax / fmin, (double) k / (pts - 1));
            std::complex<double> H = 0;
            double w = -2 * M_PI * f / sr;
            for (size_t n = 0; n < h.size(); ++n) H += (double) h[n] * std::polar(1.0, w * (double) n);
            H /= scale;
            fprintf(o, "%.4f,%.5f,%.3f\n", f, 20 * std::log10(std::abs(H) + 1e-30), std::arg(H) * 180 / M_PI);
        }
        fclose(o);
        return 0;
    }
    if (cmd == "stats" && argc >= 3) {
        double sr;
        Audio a = readAudio(argv[2], &sr);
        size_t blk = argc > 3 ? (size_t) atoi(argv[3]) : 480;
        for (size_t s = 0; s < a[0].size(); s += blk) {
            printf("%8zu", s);
            for (auto& c : a) {
                double e = 0; size_t n = 0;
                for (size_t i = s; i < std::min(s + blk, c.size()); ++i, ++n) e += (double) c[i] * c[i];
                printf(" %8.2f", n ? 10 * std::log10(e / (double) n + 1e-20) : -200.0);
            }
            printf("\n");
        }
        return 0;
    }
    die("bad command/arguments");
}
