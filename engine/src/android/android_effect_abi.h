// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Minimal, ABI-compatible declarations of the Android audio effect HAL interface
// (system/audio_effect.h + hardware/audio_effect.h, effect library API 3.0).
// The NDK does not ship these headers; the layouts below must match AOSP exactly.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct effect_uuid_s {
    uint32_t timeLow;
    uint16_t timeMid;
    uint16_t timeHiAndVersion;
    uint16_t clockSeq;
    uint8_t node[6];
} effect_uuid_t;

#define EFFECT_STRING_LEN_MAX 64

typedef struct effect_descriptor_s {
    effect_uuid_t type;
    effect_uuid_t uuid;
    uint32_t apiVersion;
    uint32_t flags;
    uint16_t cpuLoad;
    uint16_t memoryUsage;
    char name[EFFECT_STRING_LEN_MAX];
    char implementor[EFFECT_STRING_LEN_MAX];
} effect_descriptor_t;

// Effect flags (bit layout per system/audio_effect.h).
#define EFFECT_FLAG_TYPE_SHIFT 0
#define EFFECT_FLAG_TYPE_SIZE 3
#define EFFECT_FLAG_TYPE_INSERT (0u << EFFECT_FLAG_TYPE_SHIFT)
#define EFFECT_FLAG_TYPE_AUXILIARY (1u << EFFECT_FLAG_TYPE_SHIFT)
#define EFFECT_FLAG_INSERT_SHIFT (EFFECT_FLAG_TYPE_SHIFT + EFFECT_FLAG_TYPE_SIZE)
#define EFFECT_FLAG_INSERT_SIZE 3
#define EFFECT_FLAG_INSERT_ANY (0u << EFFECT_FLAG_INSERT_SHIFT)
#define EFFECT_FLAG_INSERT_FIRST (1u << EFFECT_FLAG_INSERT_SHIFT)
#define EFFECT_FLAG_INSERT_LAST (2u << EFFECT_FLAG_INSERT_SHIFT)
#define EFFECT_FLAG_VOLUME_SHIFT (EFFECT_FLAG_INSERT_SHIFT + EFFECT_FLAG_INSERT_SIZE)
#define EFFECT_FLAG_VOLUME_SIZE 3
#define EFFECT_FLAG_VOLUME_CTRL (1u << EFFECT_FLAG_VOLUME_SHIFT)
#define EFFECT_FLAG_VOLUME_IND (2u << EFFECT_FLAG_VOLUME_SHIFT)
#define EFFECT_FLAG_DEVICE_SHIFT (EFFECT_FLAG_VOLUME_SHIFT + EFFECT_FLAG_VOLUME_SIZE)
#define EFFECT_FLAG_DEVICE_SIZE 3
#define EFFECT_FLAG_DEVICE_IND (1u << EFFECT_FLAG_DEVICE_SHIFT)
#define EFFECT_FLAG_INPUT_SHIFT (EFFECT_FLAG_DEVICE_SHIFT + EFFECT_FLAG_DEVICE_SIZE)
#define EFFECT_FLAG_INPUT_SIZE 2
#define EFFECT_FLAG_INPUT_DIRECT (1u << EFFECT_FLAG_INPUT_SHIFT)
#define EFFECT_FLAG_OUTPUT_SHIFT (EFFECT_FLAG_INPUT_SHIFT + EFFECT_FLAG_INPUT_SIZE)
#define EFFECT_FLAG_OUTPUT_SIZE 2
#define EFFECT_FLAG_OUTPUT_DIRECT (1u << EFFECT_FLAG_OUTPUT_SHIFT)

#define EFFECT_MAKE_API_VERSION(M, m) (((M) << 16) | ((m) & 0xFFFF))
#define EFFECT_API_VERSION_MAJOR(v) ((v) >> 16)
#define EFFECT_CONTROL_API_VERSION EFFECT_MAKE_API_VERSION(2, 0)
#define EFFECT_LIBRARY_API_VERSION EFFECT_MAKE_API_VERSION(3, 0)

typedef struct audio_buffer_s {
    size_t frameCount;
    union {
        void* raw;
        float* f32;
        int32_t* s32;
        int16_t* s16;
        uint8_t* u8;
    };
} audio_buffer_t;

typedef int32_t (*buffer_function_t)(void* cookie, audio_buffer_t* buffer);

typedef struct buffer_provider_s {
    buffer_function_t getBuffer;
    buffer_function_t releaseBuffer;
    void* cookie;
} buffer_provider_t;

typedef struct buffer_config_s {
    audio_buffer_t buffer;
    uint32_t samplingRate;
    uint32_t channels;
    buffer_provider_t bufferProvider;
    uint8_t format;
    uint8_t accessMode;
    uint16_t mask;
} buffer_config_t;

enum effect_buffer_access_e {
    EFFECT_BUFFER_ACCESS_WRITE,
    EFFECT_BUFFER_ACCESS_READ,
    EFFECT_BUFFER_ACCESS_ACCUMULATE
};

#define EFFECT_CONFIG_BUFFER 0x0001
#define EFFECT_CONFIG_SMP_RATE 0x0002
#define EFFECT_CONFIG_CHANNELS 0x0004
#define EFFECT_CONFIG_FORMAT 0x0008
#define EFFECT_CONFIG_ACC_MODE 0x0010
#define EFFECT_CONFIG_PROVIDER 0x0020
#define EFFECT_CONFIG_ALL 0x003F

typedef struct effect_config_s {
    buffer_config_t inputCfg;
    buffer_config_t outputCfg;
} effect_config_t;

typedef struct effect_param_s {
    int32_t status;
    uint32_t psize;
    uint32_t vsize;
    char data[];
} effect_param_t;

enum effect_command_e {
    EFFECT_CMD_INIT,
    EFFECT_CMD_SET_CONFIG,
    EFFECT_CMD_RESET,
    EFFECT_CMD_ENABLE,
    EFFECT_CMD_DISABLE,
    EFFECT_CMD_SET_PARAM,
    EFFECT_CMD_SET_PARAM_DEFERRED,
    EFFECT_CMD_SET_PARAM_COMMIT,
    EFFECT_CMD_GET_PARAM,
    EFFECT_CMD_SET_DEVICE,
    EFFECT_CMD_SET_VOLUME,
    EFFECT_CMD_SET_AUDIO_MODE,
    EFFECT_CMD_SET_CONFIG_REVERSE,
    EFFECT_CMD_SET_INPUT_DEVICE,
    EFFECT_CMD_GET_CONFIG,
    EFFECT_CMD_GET_CONFIG_REVERSE,
    EFFECT_CMD_GET_FEATURE_SUPPORTED_CONFIGS,
    EFFECT_CMD_GET_FEATURE_CONFIG,
    EFFECT_CMD_SET_FEATURE_CONFIG,
    EFFECT_CMD_SET_AUDIO_SOURCE,
    EFFECT_CMD_OFFLOAD,
    EFFECT_CMD_DUMP,
    EFFECT_CMD_FIRST_PROPRIETARY = 0x10000
};

// audio_format_t values used by effects.
#define AUDIO_FORMAT_PCM_16_BIT 0x1u
#define AUDIO_FORMAT_PCM_32_BIT 0x3u
#define AUDIO_FORMAT_PCM_8_24_BIT 0x4u
#define AUDIO_FORMAT_PCM_FLOAT 0x5u
#define AUDIO_FORMAT_PCM_24_BIT_PACKED 0x6u

// audio_channel_mask_t helpers.
#define AUDIO_CHANNEL_OUT_MONO 0x1u
#define AUDIO_CHANNEL_OUT_STEREO 0x3u

struct effect_interface_s;
typedef struct effect_interface_s** effect_handle_t;

struct effect_interface_s {
    int32_t (*process)(effect_handle_t self, audio_buffer_t* inBuffer, audio_buffer_t* outBuffer);
    int32_t (*command)(effect_handle_t self, uint32_t cmdCode, uint32_t cmdSize, void* pCmdData,
                       uint32_t* replySize, void* pReplyData);
    int32_t (*get_descriptor)(effect_handle_t self, effect_descriptor_t* pDescriptor);
    int32_t (*process_reverse)(effect_handle_t self, audio_buffer_t* inBuffer, audio_buffer_t* outBuffer);
};

#define AUDIO_EFFECT_LIBRARY_TAG ((('A') << 24) | (('E') << 16) | (('L') << 8) | ('T'))

typedef struct audio_effect_library_s {
    uint32_t tag;
    uint32_t version;
    const char* name;
    const char* implementor;
    int32_t (*create_effect)(const effect_uuid_t* uuid, int32_t sessionId, int32_t ioId, effect_handle_t* pHandle);
    int32_t (*release_effect)(effect_handle_t handle);
    int32_t (*get_descriptor)(const effect_uuid_t* uuid, effect_descriptor_t* pDescriptor);
} audio_effect_library_t;

#define AUDIO_EFFECT_LIBRARY_INFO_SYM AELI
#define AUDIO_EFFECT_LIBRARY_INFO_SYM_AS_STR "AELI"

#ifdef __cplusplus
}
#endif
