// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

// Host-side tests for the ResonanceFX engine. Drives the library exactly like Android's effect
// framework: through the exported AELI entry point and the effect_interface_s command/process
// calls. Build with ASan/UBSan (see tests/run-engine-tests.sh).

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <zlib.h>

#include "android/android_effect_abi.h"
#include "rfx/modules/Binaural.h"
#include "test_protocol.h"

using rfx::HrtfSpatializer;

extern "C" audio_effect_library_t AELI;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static const effect_uuid_t kUuid = {0x4daa8eaa, 0x0842, 0x4a8d, 0x82e9, {0x6e, 0xee, 0x58, 0x27, 0xfb, 0xe4}};

struct Fx {
    effect_handle_t h = nullptr;
    Fx() { CHECK(AELI.create_effect(&kUuid, 0, 0, &h) == 0, "create_effect"); }
    ~Fx() { AELI.release_effect(h); }

    int32_t cmd(uint32_t code, uint32_t size = 0, void *data = nullptr, uint32_t *replySize = nullptr, void *reply = nullptr) {
        return (*h)->command(h, code, size, data, replySize, reply);
    }
    int32_t simple(uint32_t code) {
        int32_t reply = 0; uint32_t rs = sizeof(reply);
        int32_t r = cmd(code, 0, nullptr, &rs, &reply);
        return r != 0 ? r : reply;
    }
    int32_t config(uint32_t rate, uint32_t channels, uint8_t format, size_t frames = 256, uint8_t access = EFFECT_BUFFER_ACCESS_WRITE) {
        effect_config_t c{};
        c.inputCfg.samplingRate = c.outputCfg.samplingRate = rate;
        c.inputCfg.channels = c.outputCfg.channels = channels;
        c.inputCfg.format = c.outputCfg.format = format;
        c.inputCfg.accessMode = EFFECT_BUFFER_ACCESS_READ;
        c.outputCfg.accessMode = access;
        c.inputCfg.buffer.frameCount = c.outputCfg.buffer.frameCount = frames;
        c.inputCfg.mask = c.outputCfg.mask = EFFECT_CONFIG_SMP_RATE | EFFECT_CONFIG_CHANNELS | EFFECT_CONFIG_FORMAT | EFFECT_CONFIG_ACC_MODE;
        int32_t reply = 0; uint32_t rs = sizeof(reply);
        return cmd(EFFECT_CMD_SET_CONFIG, sizeof(c), &c, &rs, &reply);
    }
    int32_t setParam(int32_t param, const void *value, uint32_t vsize) {
        std::vector<uint8_t> buf(sizeof(effect_param_t) + 4 + vsize);
        auto *p = (effect_param_t *) buf.data();
        p->psize = 4; p->vsize = vsize;
        memcpy(p->data, &param, 4);
        memcpy(p->data + 4, value, vsize);
        int32_t reply = 0; uint32_t rs = sizeof(reply);
        int32_t r = cmd(EFFECT_CMD_SET_PARAM, buf.size(), buf.data(), &rs, &reply);
        return r != 0 ? r : reply;
    }
    int32_t set(int32_t param, std::initializer_list<int32_t> v) { std::vector<int32_t> a(v); return setParam(param, a.data(), a.size() * 4); }
    // Returns reply status; value bytes copied to out.
    int32_t get(int32_t param, void *out, uint32_t outSize, uint32_t replyCapacity = 256) {
        std::vector<uint8_t> req(sizeof(effect_param_t) + 4);
        auto *p = (effect_param_t *) req.data();
        p->psize = 4; p->vsize = outSize;
        memcpy(p->data, &param, 4);
        std::vector<uint8_t> reply(replyCapacity + 64, 0xAB); // guard bytes after capacity
        uint32_t rs = replyCapacity;
        int32_t r = cmd(EFFECT_CMD_GET_PARAM, req.size(), req.data(), &rs, reply.data());
        for (size_t i = replyCapacity; i < reply.size(); i++) CHECK(reply[i] == 0xAB, "GET_PARAM %d wrote past reply buffer", param);
        if (r == 0) {
            auto *rp = (effect_param_t *) reply.data();
            memcpy(out, rp->data + 4, std::min(outSize, rp->vsize));
        }
        return r;
    }
    int32_t getInt(int32_t param) { int32_t v = -12345; get(param, &v, 4); return v; }
    int32_t process(audio_buffer_t *in, audio_buffer_t *out) { return (*h)->process(h, in, out); }
};

static std::vector<float> sine(size_t frames, uint32_t channels, float amp, float freq = 1000, float rate = 48000) {
    std::vector<float> v(frames * channels);
    for (size_t i = 0; i < frames; i++)
        for (uint32_t c = 0; c < channels; c++) v[i * channels + c] = amp * sinf(2 * M_PI * freq * i / rate + c);
    return v;
}

static void testLibraryInfo() {
    CHECK(AELI.tag == AUDIO_EFFECT_LIBRARY_TAG, "AELI tag");
    effect_descriptor_t d{};
    CHECK(AELI.get_descriptor(&kUuid, &d) == 0, "get_descriptor");
    CHECK(strlen(d.name) > 0 && strlen(d.name) < EFFECT_STRING_LEN_MAX, "descriptor name");
    effect_uuid_t other = kUuid; other.timeLow ^= 1;
    effect_handle_t h = nullptr;
    CHECK(AELI.create_effect(&other, 0, 0, &h) == -ENOENT, "wrong uuid must be rejected");
}

static void testLifecycleAndGetters() {
    Fx fx;
    CHECK(fx.simple(EFFECT_CMD_INIT) == 0, "INIT");
    CHECK(fx.getInt(PARAM_GET_CONFIGURE) == 0, "unconfigured effect must not report configured");
    CHECK(fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT) == 0, "SET_CONFIG");
    CHECK(fx.getInt(PARAM_GET_CONFIGURE) == 1, "configured");
    CHECK(fx.getInt(PARAM_GET_SAMPLING_RATE) == 48000, "sampling rate");
    CHECK(fx.getInt(PARAM_GET_CHANNEL_COUNT) == 2, "channel count");
    char arch[16] = {}; fx.get(PARAM_GET_ARCHITECTURE, arch, sizeof(arch) - 1);
    CHECK(strcmp(arch, "x86_64") == 0, "architecture: %s", arch);
    // A reply buffer too small for the value must be refused, never overrun.
    char tiny[4];
    CHECK(fx.get(PARAM_GET_DRIVER_VERSION_NAME, tiny, 4, sizeof(effect_param_t) + 4) != 0, "small reply must fail");
    // Routing notifications are accepted.
    uint32_t device = 2; int32_t reply = -1; uint32_t rs = 4;
    CHECK(fx.cmd(EFFECT_CMD_SET_DEVICE, 4, &device, &rs, &reply) == 0, "SET_DEVICE");
}

static void testRejectsBadPayloads() {
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    // cmdSize claims less data than psize/vsize describe.
    std::vector<uint8_t> buf(sizeof(effect_param_t) + 8);
    auto *p = (effect_param_t *) buf.data();
    p->psize = 4; p->vsize = 8192;
    int32_t param = PARAM_CONVOLUTION_SET_BUFFER; memcpy(p->data, &param, 4);
    int32_t reply = 0; uint32_t rs = 4;
    CHECK(fx.cmd(EFFECT_CMD_SET_PARAM, buf.size(), buf.data(), &rs, &reply) != 0, "truncated SET_PARAM must fail");
    // DDC count larger than the payload.
    std::vector<uint8_t> ddc(1024, 0); uint32_t huge = 100000; memcpy(ddc.data(), &huge, 4);
    CHECK(fx.setParam(PARAM_DDC_COEFFICIENTS, ddc.data(), ddc.size()) != 0, "oversized DDC count must fail");
}

static void testStereoFloatProcessing() {
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_FIDELITY_BASS_ENABLE, {1});
    fx.set(PARAM_FIDELITY_BASS_GAIN, {170});
    fx.set(PARAM_FIDELITY_CLARITY_ENABLE, {1});
    fx.set(PARAM_GATE_OUTPUT_VOLUME, {200});
    auto in = sine(256, 2, 0.95f, 80);
    std::vector<float> out(in.size());
    for (int block = 0; block < 200; block++) {
        audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
        CHECK(fx.process(&ib, &ob) == 0, "process");
        for (float s : out) {
            if (!(std::isfinite(s) && s >= -1.0f && s <= 1.0f)) { CHECK(false, "float output out of range: %f", s); return; }
        }
    }
}

static void testClampingInt16() {
    // Heavy gain on a full-scale sine must clip, never wrap around. A wrapped int16 sample shows
    // up as a jump of more than half the full range between neighbouring samples, which a
    // 100 Hz sine (max step ~430 at 48 kHz) can't produce even after +6 dB and bass boost.
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT);
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_GATE_OUTPUT_VOLUME, {200});
    fx.set(PARAM_GATE_LIMIT, {100});
    fx.set(PARAM_FIDELITY_BASS_ENABLE, {1});
    fx.set(PARAM_FIDELITY_BASS_GAIN, {170});
    int wraps = 0, clipped = 0;
    int16_t last[2] = {0, 0};
    for (int block = 0; block < 200; block++) {
        std::vector<int16_t> in(512), out(512);
        for (size_t i = 0; i < 256; i++)
            in[i * 2] = in[i * 2 + 1] = (int16_t) lrint(32000 * sin(2 * M_PI * 100 * (block * 256 + i) / 48000.0));
        audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
        fx.process(&ib, &ob);
        for (size_t i = 0; i < 512; i++) {
            if (block > 20 && abs(out[i] - last[i & 1]) > 32768) wraps++;
            if (out[i] == 32767 || out[i] == -32767 || out[i] == -32768) clipped++;
            last[i & 1] = out[i];
        }
    }
    CHECK(wraps == 0, "int16 output wrapped %d times", wraps);
    (void) clipped; // DspEngine's own limiter usually keeps this path below full scale.

    // Accumulate mode adds into whatever is already in the output buffer: near-full-scale
    // existing content plus our signal must saturate, not wrap negative.
    Fx acc;
    acc.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT, 256, EFFECT_BUFFER_ACCESS_ACCUMULATE);
    acc.simple(EFFECT_CMD_ENABLE);
    acc.set(PARAM_GATE_OUTPUT_VOLUME, {100});
    int negative = 0;
    for (int block = 0; block < 60; block++) {
        std::vector<int16_t> in(512, 30000), out(512, 30000);
        audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
        acc.process(&ib, &ob);
        if (block > 40) for (int16_t v : out) if (v < 0) negative++;
    }
    CHECK(negative == 0, "accumulate overflow wrapped to negative %d times", negative);
}

// Runs a sine through the engine with every module off at unity gain. The engine buffers
// internally (latency), so compare after aligning by cross-correlation.
template <typename T>
static void roundTrip(uint8_t format, double scale, const char *name) {
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, format);
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
    fx.set(PARAM_GATE_LIMIT, {100});
    const int blocks = 80, frames = 256;
    std::vector<double> inL, outL;
    for (int block = 0; block < blocks; block++) {
        std::vector<T> in(frames * 2), out(frames * 2);
        for (int i = 0; i < frames; i++) {
            double v = 0.5 * sin(2 * M_PI * 997 * (block * frames + i) / 48000.0);
            in[i * 2] = in[i * 2 + 1] = (T) llrint(v * scale);
            inL.push_back(v);
        }
        audio_buffer_t ib{(size_t) frames, {in.data()}}, ob{(size_t) frames, {out.data()}};
        CHECK(fx.process(&ib, &ob) == 0, "%s process", name);
        for (int i = 0; i < frames; i++) outL.push_back(out[i * 2] / scale);
    }
    int bestLag = 0; double best = -1e9;
    for (int lag = 0; lag < 8192; lag++) {
        double c = 0;
        for (size_t i = 10000; i < 14000; i++) c += inL[i] * outL[i + lag];
        if (c > best) { best = c; bestLag = lag; }
    }
    double maxErr = 0;
    for (size_t i = 10000; i < inL.size() - bestLag; i++) maxErr = std::max(maxErr, fabs(outL[i + bestLag] - inL[i]));
    CHECK(maxErr < 0.01, "%s round trip error %f (latency %d frames)", name, maxErr, bestLag);
}

static void testPacked24() {
    Fx fx;
    CHECK(fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_24_BIT_PACKED) == 0, "24-bit packed config");
    CHECK(fx.getInt(PARAM_GET_CONFIGURE) == 1, "24-bit packed accepted");
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
    fx.set(PARAM_GATE_LIMIT, {100});
    double inEnergy = 0, outEnergy = 0;
    for (int block = 0; block < 80; block++) {
        std::vector<uint8_t> in(256 * 2 * 3), out(256 * 2 * 3);
        for (int i = 0; i < 512; i++) {
            double v = 0.5 * sin(2 * M_PI * 997 * (block * 256 + i / 2) / 48000.0);
            int32_t q = (int32_t) lrint(v * 8388607.0);
            in[i * 3] = q & 0xFF; in[i * 3 + 1] = (q >> 8) & 0xFF; in[i * 3 + 2] = (q >> 16) & 0xFF;
            if (block >= 40) inEnergy += v * v;
        }
        audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
        fx.process(&ib, &ob);
        if (block < 40) continue;
        for (int i = 0; i < 512; i++) {
            int32_t q = (int32_t) ((uint32_t) out[i * 3] << 8 | (uint32_t) out[i * 3 + 1] << 16 | (uint32_t) out[i * 3 + 2] << 24) >> 8;
            double v = q / 8388608.0;
            outEnergy += v * v;
        }
    }
    double ratioDb = 10 * log10(outEnergy / inEnergy);
    CHECK(fabs(ratioDb) < 0.5, "24-bit packed level changed by %.2f dB", ratioDb);
}

static void testMultichannelPassthrough() {
    // 5.1: front L/R are processed, center/LFE/surrounds must pass through bit-exact.
    const uint32_t mask = 0x3F, ch = 6;
    Fx fx;
    CHECK(fx.config(48000, mask, AUDIO_FORMAT_PCM_16_BIT) == 0, "5.1 config");
    CHECK(fx.getInt(PARAM_GET_CONFIGURE) == 1, "5.1 must be accepted (upstream rejected anything but stereo)");
    CHECK(fx.getInt(PARAM_GET_CHANNEL_COUNT) == 6, "5.1 channel count");
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_FIDELITY_BASS_ENABLE, {1});
    std::vector<int16_t> in(256 * ch), out(256 * ch);
    for (size_t i = 0; i < in.size(); i++) in[i] = (int16_t) ((i * 37) % 20000 - 10000);
    audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
    CHECK(fx.process(&ib, &ob) == 0, "5.1 process");
    int mismatches = 0;
    for (size_t f = 0; f < 256; f++)
        for (uint32_t c = 2; c < ch; c++) if (out[f * ch + c] != in[f * ch + c]) mismatches++;
    CHECK(mismatches == 0, "%d surround samples altered", mismatches);

    // Index masks (AIDL HALs) are understood too.
    Fx fx2;
    CHECK(fx2.config(48000, 0x8000000F, AUDIO_FORMAT_PCM_FLOAT) == 0 && fx2.getInt(PARAM_GET_CHANNEL_COUNT) == 4, "index mask 4ch");
}

static void testMono() {
    Fx fx;
    CHECK(fx.config(48000, AUDIO_CHANNEL_OUT_MONO, AUDIO_FORMAT_PCM_FLOAT) == 0 && fx.getInt(PARAM_GET_CONFIGURE) == 1, "mono accepted");
    fx.simple(EFFECT_CMD_ENABLE);
    auto in = sine(256, 1, 0.5f);
    std::vector<float> out(in.size());
    audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
    CHECK(fx.process(&ib, &ob) == 0, "mono process");
}

static void testZeroFrameCountConfig() {
    // AIDL effect HALs configure with frameCount 0 and pass real sizes to process().
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT, 0);
    CHECK(fx.getInt(PARAM_GET_CONFIGURE) == 1, "frameCount 0 must be accepted");
    fx.simple(EFFECT_CMD_ENABLE);
    auto in = sine(8192, 2, 0.3f);
    std::vector<float> out(in.size());
    audio_buffer_t ib{8192, {in.data()}}, ob{8192, {out.data()}};
    CHECK(fx.process(&ib, &ob) == 0, "process larger than configured");
}

static void testAccumulate() {
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT, 256, EFFECT_BUFFER_ACCESS_ACCUMULATE);
    fx.simple(EFFECT_CMD_ENABLE);
    std::vector<float> in(512, 0.0f), out(512, 0.25f);
    audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
    fx.process(&ib, &ob);
    CHECK(fabsf(out[100] - 0.25f) < 0.01f, "accumulate must add to existing output (got %f)", out[100]);
}

static void testConvolverUpload() {
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    fx.simple(EFFECT_CMD_ENABLE);

    // Stereo kernel: unit impulse on both channels, 3000 frames (spans several 8 KB chunks).
    const uint32_t frames = 3000, channels = 2, kernelId = 0x1234, nameCrc = 0xCAFE;
    std::vector<float> kernel(frames * channels, 0.0f);
    kernel[0] = kernel[1] = 1.0f;
    uint32_t dataCrc = crc32(0, (const Bytef *) kernel.data(), kernel.size() * 4);

    auto upload = [&](uint32_t id, uint32_t crc) {
        fx.set(PARAM_CONVOLUTION_PREPARE_BUFFER, {(int32_t) id, (int32_t) channels, 0});
        const uint8_t *bytes = (const uint8_t *) kernel.data();
        size_t total = kernel.size() * 4;
        for (size_t off = 0; off < total; off += 8184) {
            size_t len = std::min<size_t>(8184, total - off);
            std::vector<uint8_t> chunk(8192, 0);
            int32_t hdr[2] = {(int32_t) id, (int32_t) (len / 4)};
            memcpy(chunk.data(), hdr, 8);
            memcpy(chunk.data() + 8, bytes + off, len);
            CHECK(fx.setParam(PARAM_CONVOLUTION_SET_BUFFER, chunk.data(), chunk.size()) == 0, "SET_BUFFER");
        }
        fx.set(PARAM_CONVOLUTION_COMMIT_BUFFER, {(int32_t) id, (int32_t) crc, (int32_t) nameCrc});
    };

    upload(kernelId, dataCrc ^ 1);
    CHECK(fx.getInt(PARAM_GET_CONVOLUTION_KERNEL_ID) == 0, "kernel with bad CRC must be rejected");

    upload(kernelId, dataCrc);
    CHECK((uint32_t) fx.getInt(PARAM_GET_CONVOLUTION_KERNEL_ID) == nameCrc, "kernel must load (id %x)", fx.getInt(PARAM_GET_CONVOLUTION_KERNEL_ID));

    // With an identity kernel the convolver must pass a delayed copy of the signal.
    fx.set(PARAM_CONVOLUTION_ENABLE, {1});
    double energy = 0;
    for (int block = 0; block < 100; block++) {
        auto in = sine(256, 2, 0.5f);
        std::vector<float> out(in.size());
        audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
        CHECK(fx.process(&ib, &ob) == 0, "convolver process");
        if (block > 60) for (float s : out) energy += s * s;
    }
    CHECK(energy > 1.0, "convolver produced silence (energy %f)", energy);

    // Clearing unloads it.
    fx.set(PARAM_CONVOLUTION_PREPARE_BUFFER, {0, 0, 1});
    CHECK(fx.getInt(PARAM_GET_CONVOLUTION_KERNEL_ID) == 0, "clear kernel");
}

static void testHeadphoneSurround() {
    // VHE uses the same partitioned convolution: it must now produce sound at every level.
    for (int level = 0; level < 5; level++) {
        Fx fx;
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.simple(EFFECT_CMD_ENABLE);
        fx.set(PARAM_HEADPHONE_SURROUND_STRENGTH, {level});
        fx.set(PARAM_HEADPHONE_SURROUND_ENABLE, {1});
        double energy = 0; bool finite = true;
        for (int block = 0; block < 120; block++) {
            auto in = sine(256, 2, 0.3f, 440);
            std::vector<float> out(in.size());
            audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
            fx.process(&ib, &ob);
            if (block > 80) for (float s : out) { energy += s * s; finite &= std::isfinite(s) && fabsf(s) <= 1.0f; }
        }
        CHECK(finite, "VHE level %d produced invalid samples", level);
        CHECK(energy > 0.5, "VHE level %d produced silence (energy %f)", level, energy);
    }
}

// Sends a parametric EQ profile: preamp, then (type, Hz, dB, Q) per filter, 1024-byte payload.
static int32_t setPeq(Fx &fx, float preamp, std::initializer_list<std::array<float, 4>> filters) {
    std::vector<uint8_t> payload(1024, 0);
    uint32_t count = filters.size();
    memcpy(payload.data(), &count, 4);
    std::vector<float> f{preamp};
    for (auto &x : filters) f.insert(f.end(), x.begin(), x.end());
    memcpy(payload.data() + 4, f.data(), f.size() * 4);
    return fx.setParam(PARAM_PARAMETRIC_EQ_FILTERS, payload.data(), payload.size());
}

// Steady-state gain in dB of a sine at `hz` through the engine (latency-independent: RMS ratio).
static double measureGainDb(Fx &fx, double hz, uint32_t rate) {
    const int frames = 512;
    double inE = 0, outE = 0;
    for (int block = 0; block < 400; block++) {
        std::vector<float> in(frames * 2), out(frames * 2);
        for (int i = 0; i < frames; i++)
            in[i * 2] = in[i * 2 + 1] = 0.1f * (float) sin(2 * M_PI * hz * (block * frames + i) / rate);
        audio_buffer_t ib{(size_t) frames, {in.data()}}, ob{(size_t) frames, {out.data()}};
        fx.process(&ib, &ob);
        if (block < 200) continue;
        for (int i = 0; i < frames * 2; i++) { inE += in[i] * in[i]; outE += out[i] * out[i]; }
    }
    return 10 * log10(outE / inE);
}

static void testParametricEq() {
    {
        Fx fx;
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        uint32_t features = 0; fx.get(PARAM_GET_FEATURES, &features, 4);
        CHECK(features & RFX_FEATURE_PARAMETRIC_EQ, "features must advertise the parametric EQ");
    }
    for (uint32_t rate : {48000u, 96000u}) {
        Fx fx;
        fx.config(rate, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.simple(EFFECT_CMD_ENABLE);
        fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
        fx.set(PARAM_GATE_LIMIT, {100});
        double base1k = measureGainDb(fx, 1000, rate);
        double base100 = measureGainDb(fx, 100, rate);
        CHECK(setPeq(fx, -3.0f, {{0, 1000, 6, 1.0f}, {1, 100, 4, 0.7f}}) == 0, "PEQ payload accepted");
        fx.set(PARAM_PARAMETRIC_EQ_ENABLE, {1});
        double g1k = measureGainDb(fx, 1000, rate) - base1k;
        double g100 = measureGainDb(fx, 100, rate) - base100;
        // Peak +6 dB at 1 kHz (low shelf contributes ~0 there) minus 3 dB preamp = +3 dB.
        CHECK(fabs(g1k - 3.0) < 0.3, "%u Hz: 1 kHz gain %.2f dB, expected +3", rate, g1k);
        // Shelf corner (+4 dB shelf: about +2 dB at the corner) with the peak's skirt, minus preamp.
        CHECK(g100 > -1.5 && g100 < 1.5, "%u Hz: 100 Hz gain %.2f dB, expected about 0", rate, g100);
    }
    {
        Fx fx;
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        CHECK(setPeq(fx, 0, {{7, 1000, 6, 1}}) == 0, "bad type is acknowledged but ignored");
        std::vector<uint8_t> payload(1024, 0); uint32_t huge = 1000; memcpy(payload.data(), &huge, 4);
        CHECK(fx.setParam(PARAM_PARAMETRIC_EQ_FILTERS, payload.data(), payload.size()) != 0, "oversized count must fail");
        // A filter above Nyquist is skipped, not turned into an unstable biquad.
        CHECK(setPeq(fx, 0, {{0, 30000, 6, 1}}) == 0, "above-Nyquist filter accepted");
        fx.simple(EFFECT_CMD_ENABLE);
        fx.set(PARAM_PARAMETRIC_EQ_ENABLE, {1});
        bool finite = true;
        for (int block = 0; block < 50; block++) {
            auto in = sine(256, 2, 0.5f);
            std::vector<float> out(in.size());
            audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
            fx.process(&ib, &ob);
            for (float v : out) finite &= std::isfinite(v);
        }
        CHECK(finite, "PEQ with an out-of-range filter produced invalid samples");
    }
}

// Steady-state gain (dB) of a sine at `hz` and `amp`; `side` drives the channels in antiphase.
static double measureGainDbAt(Fx &fx, double hz, uint32_t rate, float amp, bool side) {
    const int frames = 512;
    double inE = 0, outE = 0;
    for (int block = 0; block < 400; block++) {
        std::vector<float> in(frames * 2), out(frames * 2);
        for (int i = 0; i < frames; i++) {
            const float v = amp * (float) sin(2 * M_PI * hz * (block * frames + i) / rate);
            in[i * 2] = v;
            in[i * 2 + 1] = side ? -v : v;
        }
        audio_buffer_t ib{(size_t) frames, {in.data()}}, ob{(size_t) frames, {out.data()}};
        fx.process(&ib, &ob);
        if (block < 200) continue;
        for (int i = 0; i < frames * 2; i++) { inE += in[i] * in[i]; outE += out[i] * out[i]; }
    }
    return 10 * log10(outE / inE);
}

static void testSpeakerModules() {
    {
        Fx fx;
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        uint32_t features = 0; fx.get(PARAM_GET_FEATURES, &features, 4);
        CHECK(features & kFeatureSpeakerProfiles, "features must advertise speaker profiles");
        CHECK(features & kFeatureSpeakerProtection, "features must advertise speaker protection");
        CHECK(features & kFeatureCrosstalk, "features must advertise crosstalk cancellation");
    }
    {
        // Measured correction: a +6 dB peak at 1 kHz, scaled by strength.
        Fx fx;
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.simple(EFFECT_CMD_ENABLE);
        fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
        fx.set(PARAM_GATE_LIMIT, {100});
        const double base = measureGainDb(fx, 1000, 48000);
        std::vector<uint8_t> payload(1024, 0);
        uint32_t count = 1;
        float f[5] = {0.0f, 0.0f, 1000.0f, 6.0f, 1.0f};
        memcpy(payload.data(), &count, 4);
        memcpy(payload.data() + 4, f, sizeof(f));
        CHECK(fx.setParam(kSpeakerCorrection, payload.data(), payload.size()) == 0, "speaker correction accepted");
        fx.set(kSpeakerProfile, {7});
        fx.set(kSpeakerOptimization, {1});
        const double full = measureGainDb(fx, 1000, 48000) - base;
        CHECK(fabs(full - 6.0) < 0.3, "measured correction at 100%%: %.2f dB, expected +6", full);
        fx.set(kSpeakerStrength, {50});
        const double half = measureGainDb(fx, 1000, 48000) - base;
        CHECK(fabs(half - 3.0) < 0.3, "measured correction at 50%%: %.2f dB, expected +3", half);
        uint32_t huge = 100; memcpy(payload.data(), &huge, 4);
        CHECK(fx.setParam(kSpeakerCorrection, payload.data(), payload.size()) != 0, "oversized correction must fail");
        // Every built-in profile is stable and finite.
        for (int profile = 0; profile < 7; profile++) {
            fx.set(kSpeakerProfile, {profile});
            fx.set(kSpeakerStrength, {100});
            const double g = measureGainDb(fx, 1000, 48000) - base;
            CHECK(std::isfinite(g) && fabs(g) < 4.0, "profile %d: 1 kHz gain %.2f dB", profile, g);
        }
    }
    {
        // Protection: loud bass below the driver's resonance is held down; quiet bass and the
        // midrange are untouched.
        Fx fx;
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.simple(EFFECT_CMD_ENABLE);
        fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
        fx.set(PARAM_GATE_LIMIT, {100});
        const double bassBase = measureGainDbAt(fx, 150, 48000, 0.7f, false);
        const double midBase = measureGainDbAt(fx, 3000, 48000, 0.7f, false);
        const double quietBase = measureGainDbAt(fx, 150, 48000, 0.02f, false);
        fx.set(kSpeakerProtectResonance, {200});
        fx.set(kSpeakerProtectLimit, {-1200});
        fx.set(kSpeakerProtectEnable, {1});
        const double bass = measureGainDbAt(fx, 150, 48000, 0.7f, false) - bassBase;
        const double mid = measureGainDbAt(fx, 3000, 48000, 0.7f, false) - midBase;
        const double quiet = measureGainDbAt(fx, 150, 48000, 0.02f, false) - quietBase;
        // -3.1 dBFS, predicted excursion about -5 dB (subsonic high-pass and excursion model)
        // against a -12 dB ceiling: about 7 dB of reduction.
        CHECK(bass < -4.0 && bass > -11.0, "protected 150 Hz at -3 dBFS: %.2f dB, expected about -7", bass);
        CHECK(fabs(mid) < 0.5, "protection changed 3 kHz by %.2f dB", mid);
        // Only the subsonic high-pass (100 Hz) acts on quiet bass: about -0.8 dB at 150 Hz.
        CHECK(fabs(quiet) < 1.5, "protection changed quiet 150 Hz by %.2f dB", quiet);
        // Measured mode: only the bass band (below 125 Hz) is limited, at -20 dBFS.
        std::vector<uint8_t> payload(1024, 0);
        const uint32_t bands = 5;
        const float ceilings[5] = {-20.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        memcpy(payload.data(), &bands, 4);
        memcpy(payload.data() + 4, ceilings, sizeof(ceilings));
        CHECK(fx.setParam(kSpeakerProtectCeilings, payload.data(), payload.size()) == 0, "ceilings accepted");
        fx.set(kSpeakerProtectMode, {1});
        const double low = measureGainDbAt(fx, 60, 48000, 0.5f, false) - measureGainDbAt(fx, 60, 48000, 0.01f, false);
        const double high = measureGainDbAt(fx, 2000, 48000, 0.5f, false) - midBase;
        // A -6 dBFS 60 Hz tone held at -20 dBFS: about -14 dB relative to a quiet one.
        CHECK(low < -11.0 && low > -16.0, "measured protection at 60 Hz: %.2f dB, expected about -14", low);
        CHECK(fabs(high) < 0.5, "measured protection changed 2 kHz by %.2f dB", high);
        uint32_t wrong = 4;
        memcpy(payload.data(), &wrong, 4);
        CHECK(fx.setParam(kSpeakerProtectCeilings, payload.data(), payload.size()) != 0, "wrong band count must fail");
        fx.set(kSpeakerProtectMode, {0});
    }
    {
        // Crosstalk cancellation: the centre stays as it is, the side is lifted, and the
        // recursion stays bounded on full-scale noise.
        Fx fx;
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.simple(EFFECT_CMD_ENABLE);
        fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
        fx.set(PARAM_GATE_LIMIT, {100});
        const double midBase = measureGainDbAt(fx, 1000, 48000, 0.1f, false);
        const double sideBase = measureGainDbAt(fx, 1000, 48000, 0.1f, true);
        fx.set(kCrosstalkAmount, {100});
        fx.set(kCrosstalkSpacing, {15});
        fx.set(kCrosstalkDistance, {40});
        fx.set(kCrosstalkEnable, {1});
        const double mid = measureGainDbAt(fx, 1000, 48000, 0.1f, false) - midBase;
        const double side = measureGainDbAt(fx, 1000, 48000, 0.1f, true) - sideBase;
        CHECK(fabs(mid) < 0.1, "crosstalk cancellation changed the centre by %.2f dB", mid);
        CHECK(side > 3.0, "crosstalk cancellation side gain %.2f dB, expected a lift", side);
        // Held upright the speakers are above and below the listener: the effect fades out.
        fx.set(kCrosstalkOrientation, {0});
        const double upright = measureGainDbAt(fx, 1000, 48000, 0.1f, true) - sideBase;
        CHECK(fabs(upright) < 0.2, "crosstalk cancellation upright: side %.2f dB, expected 0", upright);
        fx.set(kCrosstalkOrientation, {100});
        uint32_t seed = 1;
        bool bounded = true;
        for (int block = 0; block < 2000; block++) {
            std::vector<float> in(512), out(512);
            for (float &v : in) { seed = seed * 1664525u + 1013904223u; v = (float) ((seed >> 8) / 8388608.0 - 1.0); }
            audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
            fx.process(&ib, &ob);
            for (float v : out) bounded &= std::isfinite(v) && fabsf(v) <= 1.0f;
        }
        CHECK(bounded, "crosstalk cancellation output not bounded on noise");
    }
}

// Runs `seconds` of a 1 kHz stereo sine at `amp` and returns the gain (dB) over the last `tail`
// seconds of it.
static double sineGainDb(Fx &fx, float amp, double seconds, double tail) {
    const int frames = 480;
    const int blocks = (int) (seconds * 48000 / frames), measured = (int) (tail * 48000 / frames);
    static long phase = 0;
    double inE = 0, outE = 0;
    for (int block = 0; block < blocks; block++) {
        std::vector<float> in(frames * 2), out(frames * 2);
        for (int i = 0; i < frames; i++, phase++) in[i * 2] = in[i * 2 + 1] = amp * (float) sin(2 * M_PI * 1000.0 * phase / 48000);
        audio_buffer_t ib{(size_t) frames, {in.data()}}, ob{(size_t) frames, {out.data()}};
        fx.process(&ib, &ob);
        if (block < blocks - measured) continue;
        for (int i = 0; i < frames * 2; i++) { inE += in[i] * in[i]; outE += out[i] * out[i]; }
    }
    return 10 * log10(outE / inE);
}

static void startAgc(Fx &fx) {
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
    fx.set(PARAM_GATE_LIMIT, {100});
    fx.set(kAgcVolume, {100});     // target -14 LUFS
    fx.set(kAgcMaxScaler, {400});  // up to +12 dB
    fx.set(kAgcRatio, {50});       // gentle
    fx.set(kAgcEnable, {1});
}

static void testBassStepless() {
    // Stepless gain (dB x10): 0 is truly flat, small values give small lifts, max is full boost.
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
    fx.set(PARAM_GATE_LIMIT, {100});
    fx.set(PARAM_FIDELITY_BASS_ENABLE, {1});
    fx.set(kBassMode, {0});
    fx.set(kBassFreq, {70});
    fx.set(kBassGain, {0});
    const double flat = measureGainDbAt(fx, 40, 48000, 0.05f, false);
    CHECK(fabs(flat) < 0.4, "bass gain 0 lift %.2f dB, expected flat", flat);
    fx.set(kBassGain, {6});  // 0.6 dB: the fine control phone speakers need
    const double small = measureGainDbAt(fx, 40, 48000, 0.05f, false) - flat;
    CHECK(small > 0.25 && small < 0.95, "bass gain 0.6 dB lift %.2f dB", small);
    fx.set(kBassGain, {170});
    const double full = measureGainDbAt(fx, 40, 48000, 0.05f, false) - flat;
    CHECK(full > 12.0, "bass gain 17 dB lift %.2f dB", full);
    fx.set(kBassGain, {999});  // clamps to 170
    const double clamped = measureGainDbAt(fx, 40, 48000, 0.05f, false) - flat;
    CHECK(fabs(clamped - full) < 0.5, "bass gain clamps at max: %.2f vs %.2f dB", clamped, full);

    // Harmonics (Pure bass+): more harmonic energy with the slider up, at zero shelf gain.
    auto harmonicRms = [](int harmonics) {
        Fx f2;
        f2.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        f2.simple(EFFECT_CMD_ENABLE);
        f2.set(PARAM_GATE_OUTPUT_VOLUME, {100});
        f2.set(PARAM_GATE_LIMIT, {100});
        f2.set(PARAM_FIDELITY_BASS_ENABLE, {1});
        f2.set(kBassMode, {1});
        f2.set(kBassFreq, {70});
        f2.set(kBassGain, {0});
        f2.set(kBassHarmonics, {harmonics});
        std::vector<float> in(512), out(512);
        double sum = 0.0; int n = 0;
        for (int block = 0; block < 200; block++) {
            for (int i = 0; i < 256; i++) {
                const float s = 0.5f * sinf(2 * M_PI * 50 * (block * 256 + i) / 48000.0f);
                in[i * 2] = in[i * 2 + 1] = s;
            }
            audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
            f2.process(&ib, &ob);
            if (block >= 150) { for (float s : out) { sum += s * s; n++; } }
        }
        return sqrt(sum / n);
    };
    const double rmsOff = harmonicRms(0), rmsOn = harmonicRms(50);
    CHECK(rmsOn > rmsOff * 1.05, "harmonics 0.50 RMS %.4f vs 0.00 RMS %.4f, expected audible harmonics", rmsOn, rmsOff);
}

static void testPlaybackGain() {
    {
        // A quiet track (-31 LUFS) is lifted to the +12 dB limit within half a second.
        Fx fx;
        startAgc(fx);
        double g = sineGainDb(fx, 0.03f, 0.6, 0.1);
        CHECK(fabs(g - 12.0) < 0.5, "AGC after 0.6 s: %+.2f dB, expected +12", g);
        // Android disables, reconfigures and re-enables the effect around seeks: the learned gain
        // must survive that.
        fx.simple(EFFECT_CMD_DISABLE);
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.simple(EFFECT_CMD_ENABLE);
        g = sineGainDb(fx, 0.03f, 0.05, 0.05);
        CHECK(fabs(g - 12.0) < 0.5, "AGC right after disable/config/enable: %+.2f dB, expected +12", g);
        fx.simple(EFFECT_CMD_RESET);
        fx.set(PARAM_SET_RESET_STATUS, {1});
        g = sineGainDb(fx, 0.03f, 0.05, 0.05);
        CHECK(fabs(g - 12.0) < 0.5, "AGC right after reset commands: %+.2f dB, expected +12", g);
    }
    {
        // Loud programme is left alone by default, turned down only when allowed.
        Fx fx;
        startAgc(fx);
        double g = sineGainDb(fx, 0.5f, 1.0, 0.2);
        CHECK(fabs(g) < 0.3, "boost-only AGC changed a loud track by %+.2f dB", g);
        fx.set(kAgcMaxCut, {600});
        g = sineGainDb(fx, 0.5f, 8.0, 0.5);
        CHECK(fabs(g + 6.0) < 0.5, "AGC with 6 dB cut allowed: %+.2f dB, expected -6", g);
    }
    {
        // With the system volume applied before the engine (here -20 dB), a quiet input is
        // really a loud track played softly: no lift.
        Fx fx;
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.set(kInputLevelOffset, {-2000});
        startAgc(fx);
        double g = sineGainDb(fx, 0.03f, 1.0, 0.2);
        CHECK(fabs(g) < 0.3, "AGC lifted a track that is only quiet because of the volume: %+.2f dB", g);
    }
    {
        // The same offset makes the compressor judge the programme level: -26 dBFS peaks at -20 dB
        // volume are -6 dB programme, 12 dB over a -18 dB threshold at 8:1.
        Fx fx;
        fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.simple(EFFECT_CMD_ENABLE);
        fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
        fx.set(PARAM_GATE_LIMIT, {100});
        fx.set(kCompThreshold, {70});
        fx.set(kCompRatio, {100});
        fx.set(kCompAutoGain, {0});
        fx.set(kCompGain, {0});
        fx.set(kCompNoClip, {0});
        fx.set(kCompEnable, {1});
        double g = sineGainDb(fx, 0.05f, 1.0, 0.3);
        CHECK(fabs(g) < 0.5, "compressor acted below its threshold: %+.2f dB", g);
        fx.set(kInputLevelOffset, {-2000});
        g = sineGainDb(fx, 0.05f, 1.0, 0.3);
        CHECK(g < -8.0 && g > -12.0, "compressor with -20 dB volume offset: %+.2f dB, expected about -10.5", g);
    }
}

// Plays `seconds` of the stereo signal gen(i) (i = running sample index) and returns the
// output of the last block; *first receives the sample index that block started at.
template <class Gen>
static std::vector<float> playFor(Fx &fx, double seconds, Gen gen, long *first = nullptr) {
    const int frames = 480, blocks = std::max(1, (int) (seconds * 48000 / frames));
    static long index = 0;
    std::vector<float> in(frames * 2), out(frames * 2);
    for (int block = 0; block < blocks; block++) {
        if (first) *first = index;
        for (int i = 0; i < frames; i++, index++) { float v[2]; gen(index, v); in[i * 2] = v[0]; in[i * 2 + 1] = v[1]; }
        audio_buffer_t ib{(size_t) frames, {in.data()}}, ob{(size_t) frames, {out.data()}};
        fx.process(&ib, &ob);
    }
    return out;
}

static double rmsDb(const std::vector<float> &x) {
    double e = 0;
    for (float v : x) e += v * v;
    return 10 * log10(e / x.size() + 1e-30);
}

static void abxChainSetup(Fx &fx) {
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
    fx.set(PARAM_GATE_LIMIT, {100});
}

static void testListeningTest() {
    auto tone = [](long i, float *v) { v[0] = v[1] = 0.1f * (float) sin(2 * M_PI * 1000.0 * i / 48000); };
    {
        Fx fx;
        abxChainSetup(fx);
        uint32_t features = 0; fx.get(PARAM_GET_FEATURES, &features, 4);
        CHECK(features & kFeatureAbx, "features must advertise the listening test");
        CHECK(fx.getInt(kGetAbxDelta) == kAbxNotReady, "no loudness difference before a test");
        // The chain under test: +6 dB at 1 kHz.
        setPeq(fx, 0.0f, {{0, 1000, 6, 1.4f}});
        fx.set(PARAM_PARAMETRIC_EQ_ENABLE, {1});
        fx.set(kAbxMode, {1});
        fx.set(kAbxSelect, {1});
        playFor(fx, 4.0, tone);
        const int32_t delta = fx.getInt(kGetAbxDelta);
        CHECK(abs(delta - 600) < 30, "chain loudness difference %d (x100 dB), expected +6 dB", delta);
        // Both renderings now play at the same level.
        const double b = rmsDb(playFor(fx, 0.5, tone));
        fx.set(kAbxSelect, {0});
        const double a = rmsDb(playFor(fx, 0.5, tone));
        CHECK(fabs(a - b) < 0.2, "A %.2f dB vs B %.2f dB: not level matched", a, b);
        fx.set(kAbxMode, {0});
        CHECK(fx.getInt(kGetAbxDelta) == kAbxNotReady, "no loudness difference once the test ended");
    }
    {
        // Timing: the chain's latency (the compressor's clip guard, 31 samples, with nothing to
        // compress) must be matched in A, or switching would shift the music in time.
        auto noise = [](long i, float *v) {
            uint32_t s = (uint32_t) i * 2654435761u + 12345u;
            s ^= s >> 13; s *= 0x5bd1e995u; s ^= s >> 15;
            v[0] = v[1] = 0.05f * ((s >> 8) / 8388608.0f - 1.0f);
        };
        auto bestDelay = [&](const std::vector<float> &out, long first) {
            int best = -1;
            double bestErr = 1e30;
            for (int d = 0; d < 300; d++) {
                double err = 0;
                for (size_t i = 0; i < out.size() / 2; i++) {
                    float v[2]; noise(first + (long) i - d, v);
                    err += (out[i * 2] - v[0]) * (out[i * 2] - v[0]);
                }
                if (err < bestErr) { bestErr = err; best = d; }
            }
            return best;
        };
        Fx fx;
        abxChainSetup(fx);
        fx.set(kCompThreshold, {100});
        fx.set(kCompRatio, {0});
        fx.set(kCompAutoGain, {0});
        fx.set(kCompEnable, {1});
        fx.set(kAbxMode, {1});
        long first = 0;
        fx.set(kAbxSelect, {1});
        const std::vector<float> outB = playFor(fx, 0.5, noise, &first);
        const int dB = bestDelay(outB, first);
        fx.set(kAbxSelect, {0});
        const std::vector<float> outA = playFor(fx, 0.5, noise, &first);
        const int dA = bestDelay(outA, first);
        // The limiter (80 samples, in both) plus the clip guard's oversampling.
        CHECK(dB > 80 && dA == dB, "B is %d samples late, A %d: renderings not aligned", dB, dA);
    }
    {
        // Testing one effect: the reference keeps the rest of the chain.
        Fx fx;
        abxChainSetup(fx);
        setPeq(fx, 0.0f, {{0, 1000, 6, 1.4f}});
        fx.set(PARAM_PARAMETRIC_EQ_ENABLE, {1});
        fx.set(PARAM_GATE_OUTPUT_VOLUME, {50});  // the output gain stays in both: no difference
        fx.set(kAbxExclude, {PARAM_PARAMETRIC_EQ_ENABLE});
        fx.set(kAbxMode, {1});
        playFor(fx, 4.0, tone);
        const int32_t one = fx.getInt(kGetAbxDelta);
        CHECK(abs(one - 600) < 30, "excluding the parametric EQ: difference %d (x100 dB), expected +6 dB", one);
        // With the effect off in the chain too, A and B are the same.
        fx.set(PARAM_PARAMETRIC_EQ_ENABLE, {0});
        fx.set(kAbxMode, {0});
        fx.set(kAbxMode, {1});
        playFor(fx, 4.0, tone);
        const int32_t none = fx.getInt(kGetAbxDelta);
        CHECK(abs(none) < 10, "effect off in both: difference %d (x100 dB), expected 0", none);
    }
}

static void testConcurrentCommands() {
    // Binder thread hammers parameters/config while the audio thread processes: must never crash
    // or block (process() only try_locks).
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    fx.simple(EFFECT_CMD_ENABLE);
    std::atomic<bool> stop{false};
    std::thread control([&] {
        int i = 0;
        while (!stop) {
            fx.set(PARAM_FIDELITY_BASS_ENABLE, {i & 1});
            fx.set(PARAM_REVERBERATION_ENABLE, {(i >> 1) & 1});
            fx.set(PARAM_FIR_EQUALIZER_BAND_LEVEL, {i % 10, (i % 24 - 12) * 100});
            if (i % 50 == 0) fx.config(i % 100 == 0 ? 44100 : 48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
            i++;
        }
    });
    auto in = sine(256, 2, 0.5f);
    std::vector<float> out(in.size());
    for (int block = 0; block < 4000; block++) {
        audio_buffer_t ib{256, {in.data()}}, ob{256, {out.data()}};
        fx.process(&ib, &ob);
    }
    stop = true;
    control.join();
}

// ---- 3DoF head tracking -------------------------------------------------------------------------
static void testHrtfOrientation() {
    float az = 0, el = 0;
    // Identity leaves the direction unchanged.
    HrtfSpatializer::headDirection(30, 12, 1, 0, 0, 0, &az, &el);
    CHECK(fabsf(az - 30) < 1e-3f && fabsf(el - 12) < 1e-3f, "identity direction %.3f %.3f", az, el);

    // Head turned 30 degrees left (q = Rz(+30)): the world front appears 30 degrees right.
    float h = 30.0f * (float) M_PI / 180.0f / 2.0f;
    HrtfSpatializer::headDirection(0, 0, cosf(h), 0, 0, sinf(h), &az, &el);
    CHECK(fabsf(az - 30) < 1e-3f && fabsf(el) < 1e-3f, "yaw direction %.3f %.3f", az, el);

    // Head pitched 90 degrees up (q = Rx(+90)): the world front is directly below the head.
    h = 45.0f * (float) M_PI / 180.0f;
    HrtfSpatializer::headDirection(0, 0, cosf(h), sinf(h), 0, 0, &az, &el);
    CHECK(fabsf(el + 90.0f) < 1e-2f, "pitch direction elevation %.3f", el);

    // Right ear down 90 degrees (q = Ry(+90)): the world's right is now above the head.
    HrtfSpatializer::headDirection(90, 0, cosf(h), 0, sinf(h), 0, &az, &el);
    CHECK(fabsf(el - 90.0f) < 1e-2f, "roll direction elevation %.3f", el);

    HrtfSpatializer hrtf;
    hrtf.prepare(48000.0);
    HrtfSpatializer::Settings base;
    base.angleDeg = 30.0f;
    base.room = 0.0f;
    base.tracking = false;
    const int n = 4096;
    std::vector<float> l(n), r(n);
    hrtf.impulseResponse(base, l, r, n);
    double energy = 0;
    bool finite = true;
    for (int i = 0; i < n; ++i) {
        energy += static_cast<double>(l[i]) * l[i] + static_cast<double>(r[i]) * r[i];
        finite &= std::isfinite(l[i]) && std::isfinite(r[i]);
    }
    CHECK(finite && energy > 0.1, "HRTF impulse response invalid (energy %f)", energy);

    // The impulse must match what process() renders for the same settings: the compensation EQ
    // path relies on that equality.
    hrtf.set(base);
    hrtf.reset();
    std::vector<float> pl(n, 0.0f), pr(n, 0.0f);
    pl[0] = pr[0] = 1.0f;
    hrtf.process(pl.data(), pr.data(), n);
    double diff = 0;
    for (int i = 0; i < n; ++i) diff = std::max({diff, (double) fabsf(l[i] - pl[i]), (double) fabsf(r[i] - pr[i])});
    CHECK(diff < 1e-4, "impulseResponse differs from process by %g", diff);

    // Legacy yaw 20 and the equivalent quaternion must render identically.
    HrtfSpatializer::Settings yaw = base;
    yaw.tracking = true;
    yaw.yawDeg = 20.0f;
    HrtfSpatializer::Settings quat = base;
    quat.tracking = true;
    quat.orientation = true;
    const float hq = -20.0f * (float) M_PI / 180.0f / 2.0f;  // q = Rz(-yaw)
    quat.quatW = cosf(hq);
    quat.quatZ = sinf(hq);
    std::vector<float> yl(n), yr(n), ql(n), qr(n);
    hrtf.impulseResponse(yaw, yl, yr, n);
    hrtf.impulseResponse(quat, ql, qr, n);
    double ydiff = 0;
    for (int i = 0; i < n; ++i) ydiff = std::max({ydiff, (double) fabsf(yl[i] - ql[i]), (double) fabsf(yr[i] - qr[i])});
    CHECK(ydiff < 1e-5, "legacy yaw and quaternion differ by %g", ydiff);

    // Pitch must actually change the rendering (elevation is used, not just yaw).
    HrtfSpatializer::Settings up = base;
    up.tracking = true;
    up.orientation = true;
    const float hp = 30.0f * (float) M_PI / 180.0f / 2.0f;
    up.quatW = cosf(hp);
    up.quatX = sinf(hp);
    std::vector<float> ul(n), ur(n);
    hrtf.impulseResponse(up, ul, ur, n);
    double pdiff = 0;
    for (int i = 0; i < n; ++i) pdiff = std::max({pdiff, (double) fabsf(ul[i] - l[i]), (double) fabsf(ur[i] - r[i])});
    CHECK(pdiff > 1e-3, "pitch did not change the HRTF render");
}

static void testHeadOrientationParam() {
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    CHECK((fx.getInt(kGetFeatures) & kFeatureHeadOrientation) != 0,
          "features must advertise head orientation");
    // [count][w, x, y, z] (count is an integer, the quaternion floats follow it)
    struct {
        uint32_t count;
        float q[4];
    } payload{4, {1.0f, 0.0f, 0.0f, 0.0f}};
    CHECK(fx.setParam(kSpatialOrientation, &payload, sizeof(payload)) == 0,
          "orientation payload accepted");
    uint32_t bad[4] = {3, 0, 0, 0};
    CHECK(fx.setParam(kSpatialOrientation, bad, sizeof(bad)) != 0, "short orientation payload rejected");
}

// ---- Haas (precedence delay) and the mid/side widener -------------------------------------------

/// Runs an interleaved stereo buffer through the engine and returns the output.
static std::vector<float> runStereo(Fx &fx, std::vector<float> &in, size_t frames) {
    std::vector<float> out(in.size(), 0.0f);
    for (size_t at = 0; at < frames; at += 512) {
        size_t n = frames - at;
        if (n > 512) n = 512;
        audio_buffer_t ib{n, {in.data() + at * 2}};
        audio_buffer_t ob{n, {out.data() + at * 2}};
        CHECK(fx.process(&ib, &ob) == 0, "stereo process");
    }
    return out;
}

/// Mono-sum energy (dB) of an interleaved stereo buffer, from `fromFrames` on.
static double monoSumDb(const std::vector<float> &x, size_t fromFrames) {
    double sum = 0.0;
    size_t n = 0;
    for (size_t i = fromFrames; i * 2 + 1 < x.size(); i++) {
        const double m = 0.5 * ((double) x[i * 2] + (double) x[i * 2 + 1]);
        sum += m * m;
        n++;
    }
    if (n == 0 || sum <= 0.0) return -200.0;
    return 10.0 * std::log10(sum / (double) n);
}

/// Interleaved stereo sine with identical channels. The shared `sine()` offsets the second
/// channel by one radian, which is what a decorrelated test wants but not what a mono sum needs.
static std::vector<float> monoSine(size_t frames, float hz, float amp) {
    std::vector<float> v(frames * 2);
    for (size_t i = 0; i < frames; i++) {
        const float s = amp * sinf((float) (2.0 * M_PI * (double) hz * (double) i / 48000.0));
        v[i * 2] = v[i * 2 + 1] = s;
    }
    return v;
}

/// Runs an interleaved stereo buffer through the engine with the Haas delay on or off. Every test
/// below compares against the off run rather than the raw input: the engine buffers internally, so
/// its own latency would otherwise show up as a phase shift.
static std::vector<float> haasProcess(std::vector<float> in, size_t frames, int mixPercent, int delayX100,
                                      bool enabled = true) {
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
    fx.set(PARAM_GATE_LIMIT, {100});
    if (enabled) {
        fx.set(kDiffSurroundDelay, {delayX100});
        fx.set(kDiffSurroundMix, {mixPercent});
        fx.set(kDiffSurroundEnable, {1});
    }
    return runStereo(fx, in, frames);
}

/// Largest sample difference between two runs, in one channel (0 = left, 1 = right), from
/// `fromFrames` on.
static double maxDiff(const std::vector<float> &a, const std::vector<float> &b, size_t fromFrames, int channel) {
    double err = 0.0;
    for (size_t i = fromFrames; i * 2 + 1 < a.size(); i++)
        err = std::max(err, fabs((double) a[i * 2 + channel] - (double) b[i * 2 + channel]));
    return err;
}

static void testHaasMixZeroIsIdentity() {
    const size_t frames = 48000;
    std::vector<float> in(frames * 2);
    uint32_t seed = 12345;
    for (size_t i = 0; i < frames; i++) {
        seed = seed * 1664525u + 1013904223u;
        const float v = 0.25f * ((float) (seed >> 8) / 8388608.0f - 1.0f);
        in[i * 2] = v;
        in[i * 2 + 1] = 0.5f * v;
    }
    const std::vector<float> wet = haasProcess(in, frames, 0, 500, true);
    const std::vector<float> dry = haasProcess(in, frames, 0, 500, false);
    for (int ch = 0; ch < 2; ch++) {
        const double err = maxDiff(wet, dry, frames / 2, ch);
        CHECK(err < 1e-5, "haas at mix 0 changed channel %d by %g", ch, err);
    }
}

static void testHaasCombsTheMonoSumBounded() {
    // A fully wet delay cancels the bass outright in mono; a 70 % mix has to bound that.
    // Measured against the bypassed run, so the engine's own latency cancels out.
    const size_t frames = 24000;
    const float freqs[2] = {60.0f, 100.0f};
    for (float hz : freqs) {
        std::vector<float> tone = monoSine(frames, hz, 0.25f);
        const double off = monoSumDb(haasProcess(tone, frames, 70, 500, false), frames / 2);
        const double on = monoSumDb(haasProcess(tone, frames, 70, 500, true), frames / 2);
        const double delta = on - off;
        CHECK(delta < -2.0 && delta > -13.0, "haas mono at %.0f Hz changed by %.2f dB", hz, delta);
    }

    // Fully wet is the reference depth: 100 Hz is half a period at 5 ms, so it cancels.
    std::vector<float> tone = monoSine(frames, 100.0f, 0.25f);
    const double off = monoSumDb(haasProcess(tone, frames, 100, 500, false), frames / 2);
    const double on = monoSumDb(haasProcess(tone, frames, 100, 500, true), frames / 2);
    CHECK(on - off < -20.0, "fully wet haas should cancel 100 Hz in mono, got %.2f dB", on - off);
}

static void testHaasDelaysTheRightChannelOnly() {
    const size_t frames = 24000;
    std::vector<float> in(frames * 2);
    uint32_t seed = 12345;
    for (size_t i = 0; i < frames; i++) {
        seed = seed * 1664525u + 1013904223u;
        in[i * 2] = in[i * 2 + 1] = 0.25f * ((float) (seed >> 8) / 8388608.0f - 1.0f);
    }
    const std::vector<float> wet = haasProcess(in, frames, 100, 500, true);
    const std::vector<float> dry = haasProcess(in, frames, 100, 500, false);

    // The left channel is never delayed: it has to match the bypassed run sample for sample.
    const double leftErr = maxDiff(wet, dry, frames / 2, 0);
    CHECK(leftErr < 1e-5, "haas touched the left channel by %g", leftErr);

    // The right channel lags the left by 5 ms = 240 samples at 48 kHz.
    const size_t from = frames / 2;
    int bestLag = 0;
    double best = 0.0;
    for (int lag = 0; lag <= 480; lag++) {
        double c = 0.0;
        for (size_t i = from; i + lag < frames; i++) c += (double) wet[i * 2] * (double) wet[(i + lag) * 2 + 1];
        if (fabs(c) > fabs(best)) {
            best = c;
            bestLag = lag;
        }
    }
    // The 0.05 ms LFO wobbles the read position by up to 2.4 samples over the window.
    const int lagErr = bestLag > 240 ? bestLag - 240 : 240 - bestLag;
    CHECK(lagErr <= 3, "haas right-channel delay %d samples, expected 240", bestLag);
}

static void testStereoWidenerKeepsTheMonoSum() {
    const size_t frames = 24000;
    std::vector<float> in(frames * 2);
    uint32_t seed = 4242;
    for (size_t i = 0; i < frames; i++) {
        seed = seed * 1664525u + 1013904223u;
        in[i * 2] = 0.25f * ((float) (seed >> 8) / 8388608.0f - 1.0f);
        seed = seed * 1664525u + 1013904223u;
        in[i * 2 + 1] = 0.25f * ((float) (seed >> 8) / 8388608.0f - 1.0f);
    }
    Fx fx;
    fx.config(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
    fx.simple(EFFECT_CMD_ENABLE);
    fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
    fx.set(PARAM_GATE_LIMIT, {100});
    fx.set(kStereoWidenerDelay, {500});
    fx.set(kStereoWidenerWidth, {200});
    fx.set(kStereoWidenerEnable, {1});
    const std::vector<float> out = runStereo(fx, in, frames);
    const double delta = monoSumDb(out, frames / 2) - monoSumDb(in, frames / 2);
    CHECK(fabs(delta) < 0.01, "stereo widening changed the mono sum by %.3f dB", delta);
}

static void testHaasDelayScalesWithSampleRate() {
    // The delay is a time, not a sample count: one millisecond has to stay one millisecond at every
    // rate, and the read tap has to stay behind the write pointer at all of them.
    for (uint32_t rate : {44100u, 48000u, 96000u}) {
        Fx fx;
        fx.config(rate, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_FLOAT);
        fx.simple(EFFECT_CMD_ENABLE);
        fx.set(PARAM_GATE_OUTPUT_VOLUME, {100});
        fx.set(PARAM_GATE_LIMIT, {100});
        fx.set(kDiffSurroundDelay, {100});   // 1 ms: the shortest the UI can ask for
        fx.set(kDiffSurroundMix, {100});
        fx.set(kDiffSurroundEnable, {1});

        const size_t frames = rate / 2;
        std::vector<float> in(frames * 2);
        uint32_t seed = 12345;
        for (size_t i = 0; i < frames; i++) {
            seed = seed * 1664525u + 1013904223u;
            in[i * 2] = in[i * 2 + 1] = 0.25f * ((float) (seed >> 8) / 8388608.0f - 1.0f);
        }
        const std::vector<float> out = runStereo(fx, in, frames);

        const int expected = static_cast<int>(std::lround(rate / 1000.0));
        const size_t from = frames / 2;
        int bestLag = 0;
        double best = 0.0;
        for (int lag = 0; lag <= expected * 4; lag++) {
            double c = 0.0;
            for (size_t i = from; i + lag < frames; i++) c += (double) out[i * 2] * (double) out[(i + lag) * 2 + 1];
            if (fabs(c) > fabs(best)) {
                best = c;
                bestLag = lag;
            }
        }
        const int err = bestLag > expected ? bestLag - expected : expected - bestLag;
        CHECK(err <= 3, "haas 1 ms at %u Hz gave %d samples, expected %d", rate, bestLag, expected);
    }
}

int main() {
    testLibraryInfo();
    testLifecycleAndGetters();
    testRejectsBadPayloads();
    testStereoFloatProcessing();
    testClampingInt16();
    roundTrip<int16_t>(AUDIO_FORMAT_PCM_16_BIT, 32767.0, "pcm16");
    roundTrip<int32_t>(AUDIO_FORMAT_PCM_32_BIT, 2147483647.0, "pcm32");
    roundTrip<int32_t>(AUDIO_FORMAT_PCM_8_24_BIT, 8388607.0, "pcm8.24");
    testPacked24();
    testMultichannelPassthrough();
    testMono();
    testZeroFrameCountConfig();
    testAccumulate();
    testConvolverUpload();
    testHeadphoneSurround();
    testHrtfOrientation();
    testHeadOrientationParam();
    testHaasMixZeroIsIdentity();
    testHaasCombsTheMonoSumBounded();
    testHaasDelaysTheRightChannelOnly();
    testStereoWidenerKeepsTheMonoSum();
    testHaasDelayScalesWithSampleRate();
    testParametricEq();
    testSpeakerModules();
    testBassStepless();
    testPlaybackGain();
    testListeningTest();
    testConcurrentCommands();
    if (failures == 0) printf("All engine tests passed\n");
    else printf("%d engine test failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
