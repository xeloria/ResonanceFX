// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria
#pragma once
// Names the host tests use for protocol ids, mapped onto engine/src/Protocol.h.

#include "Protocol.h"

using namespace rfx::proto;

#define PARAM_GET_CONFIGURE kGetConfigured
#define PARAM_GET_SAMPLING_RATE kGetSampleRate
#define PARAM_GET_CONVOLUTION_KERNEL_ID kGetKernelId
#define PARAM_GET_DRIVER_VERSION_NAME kGetVersionName
#define PARAM_GET_ARCHITECTURE kGetArchitecture
#define PARAM_GET_CHANNEL_COUNT kGetChannelCount
#define PARAM_GET_FEATURES kGetFeatures
#define RFX_FEATURE_PARAMETRIC_EQ kFeatureParametricEq

#define PARAM_CONVOLUTION_ENABLE kConvEnable
#define PARAM_CONVOLUTION_PREPARE_BUFFER kConvPrepare
#define PARAM_CONVOLUTION_SET_BUFFER kConvSetBuffer
#define PARAM_CONVOLUTION_COMMIT_BUFFER kConvCommit
#define PARAM_DDC_COEFFICIENTS kDdcCoefficients
#define PARAM_FIDELITY_BASS_ENABLE kBassEnable
#define PARAM_FIDELITY_BASS_GAIN kBassGain
#define PARAM_FIDELITY_CLARITY_ENABLE kClarityEnable
#define PARAM_FIR_EQUALIZER_BAND_LEVEL kEqBand
#define PARAM_GATE_LIMIT kLimiterCeiling
#define PARAM_GATE_OUTPUT_VOLUME kOutputVolume
#define PARAM_HEADPHONE_SURROUND_ENABLE kHeadphoneSurroundEnable
#define PARAM_HEADPHONE_SURROUND_STRENGTH kHeadphoneSurroundLevel
#define PARAM_PARAMETRIC_EQ_ENABLE kAutoEqEnable
#define PARAM_PARAMETRIC_EQ_FILTERS kAutoEqFilters
#define PARAM_REVERBERATION_ENABLE kReverbEnable
#define PARAM_SET_RESET_STATUS kSetResetStatus
