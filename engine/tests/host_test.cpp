// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

// Host-side tests for the ResonanceFX engine. Drives the library exactly like Android's effect
// framework: through the exported AELI entry point and the effect_interface_s command/process
// calls. Build with ASan/UBSan (see tests/run-engine-tests.sh).

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <zlib.h>

#include "android/android_effect_abi.h"
#include "test_protocol.h"

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
    fx.set(PARAM_FIDELITY_BASS_GAIN, {600});
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
    fx.set(PARAM_FIDELITY_BASS_GAIN, {600});
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
    testParametricEq();
    testSpeakerModules();
    testPlaybackGain();
    testConcurrentCommands();
    if (failures == 0) printf("All engine tests passed\n");
    else printf("%d engine test failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
