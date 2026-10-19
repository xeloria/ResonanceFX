// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#pragma once
// Uniformly partitioned overlap-save FFT convolution with glitch-free kernel swaps.
//
// Kernels are built on a control thread (allocation, FFTs) and handed to the audio
// thread, which crossfades from the old kernel to the new one over one block.
// Latency is exactly one block.

#include <memory>
#include <vector>

#include "rfx/dsp/Fft.h"

namespace rfx {

/// Frequency-domain representation of a 1-, 2- or 4-path impulse response.
///   1 path : L->L and R->R share the same IR
///   2 paths: L->L, R->R
///   4 paths: L->L, L->R, R->L, R->R (true stereo / BRIR speaker virtualisation)
struct ConvKernel {
    int blockSize = 0;
    int partitions = 0;
    int paths = 0;
    int bins = 0;
    int length = 0;  // IR length in samples
    std::vector<float> re, im;  // [path][partition][bin]

    const float* pathRe(int p, int part) const { return re.data() + (static_cast<size_t>(p) * partitions + part) * bins; }
    const float* pathIm(int p, int part) const { return im.data() + (static_cast<size_t>(p) * partitions + part) * bins; }

    /// irs[path] -> pointer to `length` samples.  Not real-time safe.
    static std::unique_ptr<ConvKernel> build(const float* const* irs, int paths, int length, int blockSize);
};

class PartitionedConvolver {
public:
    /// Allocates the frequency-domain delay line for kernels up to maxPartitions.  Not RT safe.
    void prepare(int blockSize, int maxPartitions);
    int blockSize() const { return block_; }
    int maxPartitions() const { return maxParts_; }
    int latency() const { return block_; }

    /// Real-time safe: switches to `k` (may be nullptr = silence) with a one-block crossfade.
    /// The previous kernel pointer must stay valid until `retired()` no longer returns it.
    void setKernel(const ConvKernel* k);
    const ConvKernel* kernel() const { return current_; }
    /// Kernel no longer referenced by the audio path after the last crossfade finished.
    bool fading() const { return previous_ != nullptr; }

    void reset();

    /// In-place stereo processing.  dryGain/wetGain mix the (latency-aligned) dry signal.
    void process(float* left, float* right, int n, float wetGain = 1.0f, float dryGain = 0.0f);

private:
    void runBlock();
    void accumulate(const ConvKernel* k, float* yLr, float* yLi, float* yRr, float* yRi);

    int block_ = 0, fftSize_ = 0, bins_ = 0, maxParts_ = 0;
    int pos_ = 0, head_ = 0;
    const ConvKernel* current_ = nullptr;
    const ConvKernel* previous_ = nullptr;
    Fft<float> fft_;
    std::vector<float> inL_, inR_;        // 2*block time-domain history
    std::vector<float> outL_, outR_;      // block of output
    std::vector<float> fdlLr_, fdlLi_, fdlRr_, fdlRi_;  // [partition][bin]
    std::vector<float> accLr_, accLi_, accRr_, accRi_;
    std::vector<float> acc2Lr_, acc2Li_, acc2Rr_, acc2Ri_;
    std::vector<float> tmp_, tmp2_;
};

}  // namespace rfx
