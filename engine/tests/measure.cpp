// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
//
// Measurement tool: drives the engine through AELI exactly as Android does, and prints one JSON
// object per line: first {"freqs": [...]}, then one per scenario (frequency response,
// distortion, loudness, width, decay). tests/measure-engine.sh prints them; scripts/
// gen-measurements.py turns them into the app's "Engine measurements" page. Use it to check a
// DSP change didn't alter how an effect sounds.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Protocol.h"
#include "android/android_effect_abi.h"

extern "C" audio_effect_library_t AELI;

namespace {

constexpr double kRate = 48000.0;
const effect_uuid_t kUuid = {0x4daa8eaa, 0x0842, 0x4a8d, 0x82e9, {0x6e, 0xee, 0x58, 0x27, 0xfb, 0xe4}};

struct Fx {
    effect_handle_t h = nullptr;
    Fx() {
        AELI.create_effect(&kUuid, 0, 0, &h);
        effect_config_t c{};
        c.inputCfg.samplingRate = c.outputCfg.samplingRate = static_cast<uint32_t>(kRate);
        c.inputCfg.channels = c.outputCfg.channels = 0x3;
        c.inputCfg.format = c.outputCfg.format = 5;  // float
        c.inputCfg.accessMode = 1;
        c.outputCfg.accessMode = 0;
        c.inputCfg.buffer.frameCount = c.outputCfg.buffer.frameCount = 512;
        c.inputCfg.mask = c.outputCfg.mask = 0x1E;
        int32_t reply = 0;
        uint32_t rs = 4;
        (*h)->command(h, 1 /*SET_CONFIG*/, sizeof(c), &c, &rs, &reply);
        rs = 4;
        (*h)->command(h, 3 /*ENABLE*/, 0, nullptr, &rs, &reply);
        set(36866, {1});      // processing status
        set(65586, {100});    // output 0 dB
        set(65588, {100});    // limiter at 0 dBFS
        set(65587, {0});      // centred
    }
    ~Fx() { AELI.release_effect(h); }
    void setRaw(int32_t param, const void* value, uint32_t vsize) {
        std::vector<uint8_t> buf(sizeof(effect_param_t) + 4 + vsize);
        auto* p = reinterpret_cast<effect_param_t*>(buf.data());
        p->psize = 4;
        p->vsize = vsize;
        std::memcpy(p->data, &param, 4);
        std::memcpy(p->data + 4, value, vsize);
        int32_t reply = 0;
        uint32_t rs = 4;
        (*h)->command(h, 5 /*SET_PARAM*/, static_cast<uint32_t>(buf.size()), buf.data(), &rs, &reply);
    }
    /// Array parameter: [count][floats], padded to `size` bytes.
    void floats(int32_t param, uint32_t count, std::initializer_list<float> v, uint32_t size = 1024) {
        std::vector<uint8_t> payload(size, 0);
        std::memcpy(payload.data(), &count, 4);
        std::vector<float> f(v);
        std::memcpy(payload.data() + 4, f.data(), f.size() * 4);
        setRaw(param, payload.data(), size);
    }
    void set(int32_t param, std::initializer_list<int32_t> v) {
        std::vector<int32_t> a(v);
        setRaw(param, a.data(), static_cast<uint32_t>(a.size() * 4));
    }
    void run(std::vector<float>& l, std::vector<float>& r) {
        const size_t frames = l.size();
        std::vector<float> io(512 * 2), out(512 * 2);
        for (size_t at = 0; at < frames; at += 512) {
            const size_t n = std::min<size_t>(512, frames - at);
            for (size_t i = 0; i < n; ++i) {
                io[i * 2] = l[at + i];
                io[i * 2 + 1] = r[at + i];
            }
            audio_buffer_t ib{n, {io.data()}}, ob{n, {out.data()}};
            (*h)->process(h, &ib, &ob);
            for (size_t i = 0; i < n; ++i) {
                l[at + i] = out[i * 2];
                r[at + i] = out[i * 2 + 1];
            }
        }
    }
};

using Setup = void (*)(Fx&);

double rms(const std::vector<float>& x, size_t from) {
    double s = 0;
    for (size_t i = from; i < x.size(); ++i) s += double(x[i]) * x[i];
    return std::sqrt(s / double(x.size() - from));
}

/// Amplitude of `hz` in x[from..] (single-bin DFT).
double toneAmp(const std::vector<float>& x, size_t from, double hz) {
    double re = 0, im = 0;
    const size_t n = x.size() - from;
    for (size_t i = 0; i < n; ++i) {
        const double ph = 2 * M_PI * hz * double(i) / kRate;
        re += x[from + i] * std::cos(ph);
        im -= x[from + i] * std::sin(ph);
    }
    return 2 * std::sqrt(re * re + im * im) / double(n);
}

std::string jnum(double v) {
    if (!std::isfinite(v)) return "null";
    char b[32];
    std::snprintf(b, sizeof b, "%.3f", v);
    return b;
}

/// Response (dB) to a steady sine, left and right fed according to `mode`:
/// 0 = identical on both, 1 = left only (measures crosstalk into right), 2 = left only, measure left.
double sineGainDb(Setup setup, double hz, double ampDb, int mode) {
    Fx fx;
    setup(fx);
    const size_t n = static_cast<size_t>(kRate * 0.8);
    const double a = std::pow(10.0, ampDb / 20.0);
    std::vector<float> l(n), r(n);
    for (size_t i = 0; i < n; ++i) {
        const float s = static_cast<float>(a * std::sin(2 * M_PI * hz * double(i) / kRate));
        l[i] = s;
        r[i] = mode == 0 ? s : 0.0f;
    }
    fx.run(l, r);
    const size_t from = n / 2;
    const double out = toneAmp(mode == 1 ? r : l, from, hz);
    return 20 * std::log10(std::max(out, 1e-9) / a);
}

/// THD (%) of a sine at hz through the chain.
double thdPercent(Setup setup, double hz, double ampDb) {
    Fx fx;
    setup(fx);
    const size_t n = static_cast<size_t>(kRate * 0.8);
    const double a = std::pow(10.0, ampDb / 20.0);
    std::vector<float> l(n), r(n);
    for (size_t i = 0; i < n; ++i) l[i] = r[i] = static_cast<float>(a * std::sin(2 * M_PI * hz * double(i) / kRate));
    fx.run(l, r);
    const size_t from = n / 2;
    const double f0 = toneAmp(l, from, hz);
    double h = 0;
    for (int k = 2; k <= 6; ++k)
        if (hz * k < kRate / 2) h += std::pow(toneAmp(l, from, hz * k), 2);
    return 100.0 * std::sqrt(h) / std::max(f0, 1e-12);
}

/// RMS change (dB) for pink-ish noise at a given level: the overall loudness effect.
double noiseGainDb(Setup setup, double levelDb) {
    Fx fx;
    setup(fx);
    const size_t n = static_cast<size_t>(kRate * 3);
    std::vector<float> l(n), r(n);
    uint32_t seed = 12345;
    double b0 = 0, b1 = 0, b2 = 0;
    const double a = std::pow(10.0, levelDb / 20.0);
    for (size_t i = 0; i < n; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const double w = (double(seed >> 8) / 8388608.0) - 1.0;
        b0 = 0.99765 * b0 + w * 0.0990460;  // Paul Kellet's economy pink filter
        b1 = 0.96300 * b1 + w * 0.2965164;
        b2 = 0.57000 * b2 + w * 1.0526913;
        const float p = static_cast<float>(a * (b0 + b1 + b2 + w * 0.1848) * 0.25);
        l[i] = p;
        r[i] = p;
    }
    std::vector<float> in = l;
    fx.run(l, r);
    return 20 * std::log10(std::max(rms(l, n / 2), 1e-12) / std::max(rms(in, n / 2), 1e-12));
}

/// Side/mid energy ratio (dB) for a partially correlated stereo signal: stereo width.
double widthDb(Setup setup) {
    Fx fx;
    setup(fx);
    const size_t n = static_cast<size_t>(kRate * 2);
    std::vector<float> l(n), r(n);
    uint32_t s1 = 1, s2 = 2;
    for (size_t i = 0; i < n; ++i) {
        s1 = s1 * 1664525u + 1013904223u;
        s2 = s2 * 22695477u + 1u;
        const double c = (double(s1 >> 8) / 8388608.0 - 1.0) * 0.1;
        const double d = (double(s2 >> 8) / 8388608.0 - 1.0) * 0.05;
        l[i] = static_cast<float>(c + d);
        r[i] = static_cast<float>(c - d);
    }
    fx.run(l, r);
    double m = 0, s = 0;
    for (size_t i = n / 2; i < n; ++i) {
        const double mid = 0.5 * (l[i] + r[i]), side = 0.5 * (l[i] - r[i]);
        m += mid * mid;
        s += side * side;
    }
    return 10 * std::log10(std::max(s, 1e-30) / std::max(m, 1e-30));
}

/// Energy (dB) in the 0.3-1.3 s after a click, relative to the click: reverberant tail.
double tailDb(Setup setup) {
    Fx fx;
    setup(fx);
    const size_t n = static_cast<size_t>(kRate * 1.5);
    std::vector<float> l(n), r(n);
    for (size_t i = 0; i < 48; ++i) l[i] = r[i] = 0.5f;
    std::vector<float> in = l;
    fx.run(l, r);
    double tail = 0;
    for (size_t i = static_cast<size_t>(kRate * 0.3); i < n; ++i) tail += double(l[i]) * l[i];
    double click = 0;
    for (size_t i = 0; i < 48; ++i) click += double(in[i]) * in[i];
    return 10 * std::log10(std::max(tail, 1e-30) / click);
}

// Third-octave centres, 20 Hz - 20 kHz.
const double kFreqs[] = {20,  25,  31.5, 40,   50,   63,   80,   100,  125,   160,   200,   250,   315,   400,   500,  630,
                         800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000};
constexpr size_t kFreqCount = sizeof(kFreqs) / sizeof(kFreqs[0]);

std::string jstr(const char* v) {
    std::string out = "\"";
    for (const char* c = v; *c; ++c) {
        if (*c == '"' || *c == '\\') out += '\\';
        out += *c;
    }
    return out + "\"";
}

enum Extra { kNone = 0, kDynamics = 1, kCrossfeed = 2, kSpatial = 4 };

/// One scenario: `section` is the app's section id, `name` what it shows, `settings` the
/// settings measured (the app's defaults unless stated).
void report(const char* section, const char* name, const char* settings, Setup setup, int extra = kNone) {
    std::string out = "{\"section\":" + jstr(section) + ",\"name\":" + jstr(name) + ",\"settings\":" + jstr(settings) +
                      ",\"response\":[";
    for (size_t i = 0; i < kFreqCount; ++i) out += (i ? "," : "") + jnum(sineGainDb(setup, kFreqs[i], -24.0, 0));
    out += "],\"noiseDb\":" + jnum(noiseGainDb(setup, -20.0));
    out += ",\"thd100\":" + jnum(thdPercent(setup, 100, -12.0));
    out += ",\"thd1k\":" + jnum(thdPercent(setup, 1000, -12.0));
    if (extra & kDynamics) {
        out += ",\"gain1kLoud\":" + jnum(sineGainDb(setup, 1000, -3.0, 0));
        out += ",\"gain1kQuiet\":" + jnum(sineGainDb(setup, 1000, -40.0, 0));
        out += ",\"noiseLoudDb\":" + jnum(noiseGainDb(setup, -6.0));
        out += ",\"noiseQuietDb\":" + jnum(noiseGainDb(setup, -40.0));
    }
    if (extra & kCrossfeed) {
        out += ",\"crosstalk\":[";
        for (size_t i = 0; i < kFreqCount; ++i) out += (i ? "," : "") + jnum(sineGainDb(setup, kFreqs[i], -24.0, 1));
        out += "]";
    }
    if (extra & kSpatial) {
        out += ",\"widthDb\":" + jnum(widthDb(setup));
        out += ",\"tailDb\":" + jnum(tailDb(setup));
    }
    std::printf("%s}\n", out.c_str());
    std::fflush(stdout);
}

}  // namespace

using namespace rfx::proto;

int main() {
    std::string head = "{\"freqs\":[";
    for (size_t i = 0; i < kFreqCount; ++i) head += (i ? "," : "") + jnum(kFreqs[i]);
    std::printf("%s]}\n", head.c_str());

    report("master", "Bypass", "every effect off: output gain, balance and limiter only", [](Fx&) {});
    report("master", "Output -6 dB, balance left", "output -6 dB, balance 50 % left",
           [](Fx& f) { f.set(kOutputVolume, {50}); f.set(kChannelPan, {-50}); });
    report("master", "Limiter at -6 dB", "limiter threshold -6 dB", [](Fx& f) { f.set(kLimiterCeiling, {50}); }, kDynamics);

    report("loud", "Loudness compensation", "fixed level 60 phon, reference 80 phon, strength 70 %", [](Fx& f) {
        f.set(kLoudnessMode, {1}); f.set(kLoudnessManual, {60}); f.set(kLoudnessReference, {80});
        f.set(kLoudnessStrength, {70}); f.set(kLoudnessEnable, {1});
    });
    report("peq", "Parametric EQ", "low shelf 100 Hz +4 dB, peak 3 kHz -3 dB Q 1.4", [](Fx& f) {
        f.floats(kUserEqFilters, 2, {0.0f, 1, 100, 4, 0.7f, 0, 3000, -3, 1.4f});
        f.set(kUserEqEnable, {1});
    });
    report("geq", "Fine graphic EQ", "31 bands: +4 dB lows, -2 dB at 2.5 kHz, +3 dB highs", [](Fx& f) {
        f.set(kGraphicEqLayout, {2});
        f.floats(kGraphicEqGains, 31, {4, 4, 4, 4, 3.5f, 3, 2, 1, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, -0.5f, -1, -1.5f, -2,
                                       -1.5f, -1, 0, 0.5f, 1, 2, 3, 3, 3});
        f.set(kGraphicEqEnable, {1});
    });
    report("eq", "Equalizer (bass boost)", "10 bands: +10, +6.5, +4 dB in the lowest three", [](Fx& f) {
        const int g[10] = {1000, 650, 400, 0, 0, 0, 0, 0, 0, 0};
        for (int b = 0; b < 10; ++b) f.set(kEqBand, {b, g[b]});
        f.set(kEqEnable, {1});
    });
    report("eq", "Equalizer (V-shape)", "10 bands: +6 dB at the ends, -2 dB in the middle", [](Fx& f) {
        const int g[10] = {600, 400, 200, 0, -200, -200, 0, 200, 400, 600};
        for (int b = 0; b < 10; ++b) f.set(kEqBand, {b, g[b]});
        f.set(kEqEnable, {1});
    });

    report("agc", "Playback gain control", "gentle, target -14 LUFS, up to 4x, never turns down", [](Fx& f) {
        f.set(kAgcRatio, {50}); f.set(kAgcVolume, {100}); f.set(kAgcMaxScaler, {400}); f.set(kAgcEnable, {1});
    }, kDynamics);
    report("fet", "FET compressor", "the app's defaults (automatic knee, gain, attack)", [](Fx& f) {
        f.set(kCompThreshold, {100}); f.set(kCompRatio, {100}); f.set(kCompAutoKnee, {1}); f.set(kCompAutoGain, {1});
        f.set(kCompAutoAttack, {1}); f.set(kCompEnable, {1});
    }, kDynamics);
    report("fet", "FET compressor (-24 dB threshold)", "threshold 60 %, ratio 50 %, automatic knee, gain, attack",
           [](Fx& f) {
        f.set(kCompThreshold, {60}); f.set(kCompRatio, {50}); f.set(kCompAutoKnee, {1}); f.set(kCompAutoGain, {1});
        f.set(kCompAutoAttack, {1}); f.set(kCompEnable, {1});
    }, kDynamics);
    report("mbc", "Multiband compressor", "3 bands, threshold -18 dB, ratio 2.5:1, speed 50 %, makeup on", [](Fx& f) {
        f.floats(kMultibandSettings, 33, {3, 6, 1, 1, 200, 3000, 8000, 14000, -15, -18, -18, -18, -18,
                                          2.125f, 2.5f, 2.5f, 2.5f, 2.5f, 42, 21, 21, 21, 21, 460, 230, 230, 230, 230,
                                          0, 0, 0, 0, 0});
        f.set(kMultibandEnable, {1});
    }, kDynamics);
    report("deq", "Dynamic EQ", "tame boom 4 dB, tame harshness 4 dB", [](Fx& f) {
        f.floats(kDynamicEqBands, 4, {1, 0, 120, 1.2f, -24, -4, 3, 5, 150, 0, 1, 0, 3500, 1.5f, -30, -4, 3, 5, 150, 0,
                                      0, 1, 100, 0.7f, -40, 0, 3, 5, 150, 1, 0, 2, 8000, 0.7f, -45, 0, 3, 5, 150, 1});
        f.set(kDynamicEqEnable, {1});
    }, kDynamics);
    report("deess", "De-esser", "6.5 kHz, threshold -28 dB, up to 8 dB, split band", [](Fx& f) {
        f.set(kDeEsserFreq, {6500}); f.set(kDeEsserThreshold, {-2800}); f.set(kDeEsserAmount, {800});
        f.set(kDeEsserMode, {0}); f.set(kDeEsserEnable, {1});
    }, kDynamics);

    report("vocal", "Vocals & ambience", "vocals +4 dB, ambience 0 dB, focus 50 %", [](Fx& f) {
        f.set(kVocalCentre, {400}); f.set(kVocalAmbience, {0}); f.set(kVocalLow, {120}); f.set(kVocalHigh, {10000});
        f.set(kVocalFocus, {50}); f.set(kVocalEnable, {1});
    }, kSpatial);
    report("spatial", "Spatial audio", "speakers at +-30 degrees, room 30 %", [](Fx& f) {
        f.set(kSpatialAngle, {30}); f.set(kSpatialRoom, {30}); f.set(kSpatialEnable, {1});
    }, kCrossfeed | kSpatial);
    report("field", "Field surround", "strength 4, mid image 50 %", [](Fx& f) {
        f.set(kFieldSurroundDepth, {500}); f.set(kFieldSurroundMidImage, {150}); f.set(kFieldSurroundEnable, {1});
    }, kSpatial);
    report("diff", "Differential surround", "5 ms", [](Fx& f) { f.set(kDiffSurroundDelay, {500}); f.set(kDiffSurroundEnable, {1}); },
           kSpatial);
    report("vhe", "Headphone surround+", "quality 3", [](Fx& f) {
        f.set(kHeadphoneSurroundLevel, {2}); f.set(kHeadphoneSurroundEnable, {1});
    }, kCrossfeed | kSpatial);
    report("reverb", "Reverberation", "room 50 %, width 50 %, damping 30 %, wet 40 %", [](Fx& f) {
        f.set(kReverbRoomSize, {50}); f.set(kReverbWidth, {50}); f.set(kReverbDamping, {30}); f.set(kReverbWet, {40});
        f.set(kReverbDry, {50}); f.set(kReverbEnable, {1});
    }, kSpatial);
    report("cure", "Auditory protection (mild)", "binaural level mild", [](Fx& f) {
        f.set(kCrossfeedLevel, {0}); f.set(kCrossfeedEnable, {1});
    }, kCrossfeed);
    report("cure", "Auditory protection (strong)", "binaural level strong", [](Fx& f) {
        f.set(kCrossfeedLevel, {2}); f.set(kCrossfeedEnable, {1});
    }, kCrossfeed);

    report("bass", "Bass enhancer (natural)", "natural, 70 Hz, 13 dB", [](Fx& f) {
        f.set(kBassMode, {0}); f.set(kBassFreq, {70}); f.set(kBassGain, {350}); f.set(kBassEnable, {1});
    });
    report("bass", "Bass enhancer (pure bass+)", "pure bass+, 70 Hz, 13 dB", [](Fx& f) {
        f.set(kBassMode, {1}); f.set(kBassFreq, {70}); f.set(kBassGain, {350}); f.set(kBassEnable, {1});
    });
    report("bass", "Bass enhancer (subwoofer)", "subwoofer, 70 Hz, 13 dB", [](Fx& f) {
        f.set(kBassMode, {2}); f.set(kBassFreq, {70}); f.set(kBassGain, {350}); f.set(kBassEnable, {1});
    });
    report("clarity", "Clarity enhancer (natural)", "natural, 10 dB", [](Fx& f) {
        f.set(kClarityMode, {0}); f.set(kClarityGain, {250}); f.set(kClarityEnable, {1});
    });
    report("clarity", "Clarity enhancer (OZone+)", "OZone+, 10 dB", [](Fx& f) {
        f.set(kClarityMode, {1}); f.set(kClarityGain, {250}); f.set(kClarityEnable, {1});
    });
    report("clarity", "Clarity enhancer (XHiFi)", "XHiFi, 10 dB", [](Fx& f) {
        f.set(kClarityMode, {2}); f.set(kClarityGain, {250}); f.set(kClarityEnable, {1});
    });
    report("vse", "Spectrum extension", "7.6 kHz, strength 100 %", [](Fx& f) {
        f.set(kSpectrumExtReference, {7600}); f.set(kSpectrumExtAmount, {560}); f.set(kSpectrumExtEnable, {1});
    });
    report("dyn", "Dynamic system", "bass strength 100 %", [](Fx& f) {
        f.set(kDynamicSystemX, {100, 5600}); f.set(kDynamicSystemY, {40, 80}); f.set(kDynamicSystemSide, {50, 50});
        f.set(kDynamicSystemStrength, {1100}); f.set(kDynamicSystemEnable, {1});
    });
    report("tube", "Tube simulator", "on", [](Fx& f) { f.set(kTubeEnable, {1}); });
    report("analogx", "AnalogX", "medium", [](Fx& f) { f.set(kAnalogLevel, {1}); f.set(kAnalogEnable, {1}); });

    static const char* kProfiles[] = {"Phone", "Tablet", "Laptop", "Small Bluetooth speaker",
                                      "Large Bluetooth or home speaker", "Car", "TV"};
    static int profile = 0;
    for (profile = 0; profile < 7; ++profile) {
        const std::string name = std::string("Speaker correction (") + kProfiles[profile] + ")";
        report("speaker", name.c_str(), "strength 100 %", [](Fx& f) {
            f.set(kSpeakerProfile, {profile}); f.set(kSpeakerStrength, {100}); f.set(kSpeakerOptimization, {1});
        });
    }
    report("protect", "Speaker protection (phone)", "phone (700 Hz), bass ceiling -12 dB", [](Fx& f) {
        f.set(kSpeakerProtectResonance, {700}); f.set(kSpeakerProtectLimit, {-1200}); f.set(kSpeakerProtectEnable, {1});
    }, kDynamics);
    report("xtc", "Speaker widening", "width 60 %, speakers 15 cm apart, 40 cm away", [](Fx& f) {
        f.set(kCrosstalkAmount, {60}); f.set(kCrosstalkSpacing, {15}); f.set(kCrosstalkDistance, {40});
        f.set(kCrosstalkEnable, {1});
    }, kCrossfeed | kSpatial);
    return 0;
}
