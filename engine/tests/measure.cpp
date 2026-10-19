// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
//
// Measurement tool: drives the engine through AELI exactly as Android does, and prints one JSON
// object per scenario (frequency response, distortion, width, decay) for
// tests/measure-engine.sh. Use it to check a DSP change didn't alter how an effect sounds.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

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

const double kFreqs[] = {25, 40, 63, 100, 160, 250, 400, 630, 1000, 1600, 2500, 4000, 6300, 10000, 14000, 18000};

void report(const char* name, Setup setup, bool dynamics = false, bool crossfeed = false, bool spatial = false) {
    std::string out = std::string("{\"name\":\"") + name + "\",\"response\":[";
    for (size_t i = 0; i < sizeof(kFreqs) / sizeof(kFreqs[0]); ++i)
        out += (i ? "," : "") + jnum(sineGainDb(setup, kFreqs[i], -24.0, 0));
    out += "],\"noiseDb\":" + jnum(noiseGainDb(setup, -20.0));
    out += ",\"thd100\":" + jnum(thdPercent(setup, 100, -12.0));
    out += ",\"thd1k\":" + jnum(thdPercent(setup, 1000, -12.0));
    if (dynamics) {
        out += ",\"gain1kLoud\":" + jnum(sineGainDb(setup, 1000, -3.0, 0));
        out += ",\"gain1kQuiet\":" + jnum(sineGainDb(setup, 1000, -40.0, 0));
        out += ",\"noiseLoudDb\":" + jnum(noiseGainDb(setup, -6.0));
        out += ",\"noiseQuietDb\":" + jnum(noiseGainDb(setup, -40.0));
    }
    if (crossfeed) {
        out += ",\"crosstalk\":[";
        for (size_t i = 0; i < sizeof(kFreqs) / sizeof(kFreqs[0]); ++i)
            out += (i ? "," : "") + jnum(sineGainDb(setup, kFreqs[i], -24.0, 1));
        out += "]";
    }
    if (spatial) {
        out += ",\"widthDb\":" + jnum(widthDb(setup));
        out += ",\"tailDb\":" + jnum(tailDb(setup));
    }
    std::printf("%s}\n", out.c_str());
    std::fflush(stdout);
}

}  // namespace

int main() {
    report("off", [](Fx&) {});
    report("bass natural 13 dB", [](Fx& f) { f.set(65575, {0}); f.set(65576, {70}); f.set(65577, {350}); f.set(65574, {1}); });
    report("bass natural 6 dB", [](Fx& f) { f.set(65575, {0}); f.set(65576, {70}); f.set(65577, {100}); f.set(65574, {1}); });
    report("bass pure", [](Fx& f) { f.set(65575, {1}); f.set(65576, {70}); f.set(65577, {350}); f.set(65574, {1}); });
    report("bass subwoofer", [](Fx& f) { f.set(65575, {2}); f.set(65576, {70}); f.set(65577, {350}); f.set(65574, {1}); });
    report("clarity natural", [](Fx& f) { f.set(65579, {0}); f.set(65580, {250}); f.set(65578, {1}); });
    report("clarity ozone", [](Fx& f) { f.set(65579, {1}); f.set(65580, {250}); f.set(65578, {1}); });
    report("clarity xhifi", [](Fx& f) { f.set(65579, {2}); f.set(65580, {250}); f.set(65578, {1}); });
    report("eq bass booster", [](Fx& f) {
        const int g[10] = {1000, 650, 400, 0, 0, 0, 0, 0, 0, 0};
        for (int b = 0; b < 10; ++b) f.set(65552, {b, g[b]});
        f.set(65551, {1});
    });
    report("eq v-shape", [](Fx& f) {
        const int g[10] = {600, 400, 200, 0, -200, -200, 0, 200, 400, 600};
        for (int b = 0; b < 10; ++b) f.set(65552, {b, g[b]});
        f.set(65551, {1});
    });
    report("spectrum extension", [](Fx& f) { f.set(65549, {7600}); f.set(65550, {560}); f.set(65548, {1}); });
    report("field surround", [](Fx& f) { f.set(65556, {500}); f.set(65555, {150}); f.set(65553, {1}); }, false, false, true);
    report("differential surround", [](Fx& f) { f.set(65558, {500}); f.set(65557, {1}); }, false, false, true);
    report("headphone surround 3", [](Fx& f) { f.set(65545, {2}); f.set(65544, {1}); }, false, true, true);
    report("reverb medium", [](Fx& f) {
        f.set(65560, {50}); f.set(65561, {50}); f.set(65562, {30}); f.set(65563, {40}); f.set(65564, {50}); f.set(65559, {1});
    }, false, false, true);
    report("dynamic system", [](Fx& f) {
        f.set(65570, {100, 5600}); f.set(65571, {40, 80}); f.set(65572, {50, 50}); f.set(65573, {1100}); f.set(65569, {1});
    });
    report("crossfeed default", [](Fx& f) { f.set(65582, {0}); f.set(65581, {1}); }, false, true);
    report("crossfeed meier", [](Fx& f) { f.set(65582, {2}); f.set(65581, {1}); }, false, true);
    report("tube", [](Fx& f) { f.set(65583, {1}); });
    report("analogx medium", [](Fx& f) { f.set(65585, {1}); f.set(65584, {1}); });
    report("speaker optimization", [](Fx& f) { f.set(65603, {1}); });
    report("fet compressor", [](Fx& f) {
        f.set(65611, {60}); f.set(65612, {50}); f.set(65614, {1}); f.set(65616, {1}); f.set(65618, {1}); f.set(65610, {1});
    }, true);
    report("playback gain", [](Fx& f) { f.set(65566, {100}); f.set(65567, {80}); f.set(65568, {300}); f.set(65565, {1}); }, true);
    report("output -6 dB pan left", [](Fx& f) { f.set(65586, {50}); f.set(65587, {-50}); });
    report("limiter -6 dB", [](Fx& f) { f.set(65588, {50}); }, true);
    return 0;
}
