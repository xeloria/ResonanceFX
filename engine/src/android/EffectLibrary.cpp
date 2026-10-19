// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
//
// Entry points the Android effect factory loads (the exported AELI symbol).

#include <cerrno>
#include <cstring>
#include <new>

#include "../Protocol.h"
#include "EffectContext.h"
#include "android_effect_abi.h"

namespace {

using rfx::android::EffectContext;

constexpr effect_uuid_t kTypeNull = {0xec7178ec, 0xe5e1, 0x4432, 0xa3f4, {0x46, 0x57, 0xe6, 0x79, 0x52, 0x10}};
constexpr effect_uuid_t kUuid = {rfx::proto::kUuidTimeLow, rfx::proto::kUuidTimeMid, rfx::proto::kUuidTimeHi,
                                 rfx::proto::kUuidClockSeq,
                                 {rfx::proto::kUuidNode[0], rfx::proto::kUuidNode[1], rfx::proto::kUuidNode[2],
                                  rfx::proto::kUuidNode[3], rfx::proto::kUuidNode[4], rfx::proto::kUuidNode[5]}};

const effect_descriptor_t& descriptor() {
    static const effect_descriptor_t d = [] {
        effect_descriptor_t x{};
        x.type = kTypeNull;
        x.uuid = kUuid;
        x.apiVersion = EFFECT_CONTROL_API_VERSION;
        x.flags = EFFECT_FLAG_TYPE_INSERT | EFFECT_FLAG_INSERT_LAST | EFFECT_FLAG_INPUT_DIRECT | EFFECT_FLAG_OUTPUT_DIRECT;
        x.cpuLoad = 8;
        x.memoryUsage = 1;
        std::strncpy(x.name, "ResonanceFX", sizeof(x.name) - 1);
        std::strncpy(x.implementor, "Xeloria", sizeof(x.implementor) - 1);
        return x;
    }();
    return d;
}

struct Handle {
    const effect_interface_s* itfe;  // must stay the first member: the framework calls through it
    EffectContext* context;
};

bool isOurs(const effect_uuid_t* uuid) { return uuid && std::memcmp(uuid, &kUuid, sizeof(kUuid)) == 0; }

int32_t effectProcess(effect_handle_t self, audio_buffer_t* in, audio_buffer_t* out) {
    auto* h = reinterpret_cast<Handle*>(self);
    return h ? h->context->process(in, out) : -EINVAL;
}

int32_t effectCommand(effect_handle_t self, uint32_t code, uint32_t size, void* data, uint32_t* replySize, void* reply) {
    auto* h = reinterpret_cast<Handle*>(self);
    return h ? h->context->command(code, size, data, replySize, reply) : -EINVAL;
}

int32_t effectGetDescriptor(effect_handle_t self, effect_descriptor_t* out) {
    if (!self || !out) return -EINVAL;
    *out = descriptor();
    return 0;
}

const effect_interface_s kInterface = {effectProcess, effectCommand, effectGetDescriptor, nullptr};

int32_t libraryCreate(const effect_uuid_t* uuid, int32_t, int32_t, effect_handle_t* out) {
    if (!uuid || !out) return -EINVAL;
    if (!isOurs(uuid)) return -ENOENT;
    auto* h = new (std::nothrow) Handle{&kInterface, nullptr};
    if (!h) return -ENOMEM;
    h->context = new (std::nothrow) EffectContext();
    if (!h->context) {
        delete h;
        return -ENOMEM;
    }
    *out = reinterpret_cast<effect_handle_t>(h);
    return 0;
}

int32_t libraryRelease(effect_handle_t handle) {
    auto* h = reinterpret_cast<Handle*>(handle);
    if (!h) return -EINVAL;
    delete h->context;
    delete h;
    return 0;
}

int32_t libraryGetDescriptor(const effect_uuid_t* uuid, effect_descriptor_t* out) {
    if (!uuid || !out) return -EINVAL;
    if (!isOurs(uuid)) return -ENOENT;
    *out = descriptor();
    return 0;
}

}  // namespace

extern "C" {
__attribute__((visibility("default"))) audio_effect_library_t AELI = {
    AUDIO_EFFECT_LIBRARY_TAG,
    EFFECT_LIBRARY_API_VERSION,
    "ResonanceFX",
    "Xeloria",
    libraryCreate,
    libraryRelease,
    libraryGetDescriptor,
};
}
