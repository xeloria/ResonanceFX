// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Xeloria

#include "rfx/dsp/Convolver.h"

#include <algorithm>
#include <cstring>

namespace rfx {

std::unique_ptr<ConvKernel> ConvKernel::build(const float* const* irs, int paths, int length, int blockSize) {
    auto k = std::make_unique<ConvKernel>();
    k->blockSize = blockSize;
    k->paths = paths;
    k->length = length;
    k->bins = blockSize + 1;
    k->partitions = std::max(1, (length + blockSize - 1) / blockSize);
    const size_t total = static_cast<size_t>(paths) * k->partitions * k->bins;
    k->re.assign(total, 0.0f);
    k->im.assign(total, 0.0f);

    Fft<float> fft(2 * blockSize);
    std::vector<float> seg(2 * blockSize);
    for (int p = 0; p < paths; ++p) {
        for (int part = 0; part < k->partitions; ++part) {
            std::fill(seg.begin(), seg.end(), 0.0f);
            const int start = part * blockSize;
            const int count = std::min(blockSize, length - start);
            if (count > 0) std::memcpy(seg.data(), irs[p] + start, sizeof(float) * count);
            fft.forward(seg.data(), const_cast<float*>(k->pathRe(p, part)), const_cast<float*>(k->pathIm(p, part)));
        }
    }
    return k;
}

void PartitionedConvolver::prepare(int blockSize, int maxPartitions) {
    block_ = blockSize;
    fftSize_ = 2 * blockSize;
    bins_ = blockSize + 1;
    maxParts_ = std::max(1, maxPartitions);
    fft_.init(fftSize_);
    inL_.assign(fftSize_, 0.0f);
    inR_.assign(fftSize_, 0.0f);
    outL_.assign(block_, 0.0f);
    outR_.assign(block_, 0.0f);
    const size_t fdl = static_cast<size_t>(maxParts_) * bins_;
    fdlLr_.assign(fdl, 0.0f);
    fdlLi_.assign(fdl, 0.0f);
    fdlRr_.assign(fdl, 0.0f);
    fdlRi_.assign(fdl, 0.0f);
    for (auto* v : {&accLr_, &accLi_, &accRr_, &accRi_, &acc2Lr_, &acc2Li_, &acc2Rr_, &acc2Ri_})
        v->assign(bins_, 0.0f);
    tmp_.assign(fftSize_, 0.0f);
    tmp2_.assign(fftSize_, 0.0f);
    pos_ = 0;
    head_ = 0;
}

void PartitionedConvolver::setKernel(const ConvKernel* k) {
    if (k == current_) return;
    previous_ = current_;
    current_ = k;
}

void PartitionedConvolver::reset() {
    std::fill(inL_.begin(), inL_.end(), 0.0f);
    std::fill(inR_.begin(), inR_.end(), 0.0f);
    std::fill(outL_.begin(), outL_.end(), 0.0f);
    std::fill(outR_.begin(), outR_.end(), 0.0f);
    std::fill(fdlLr_.begin(), fdlLr_.end(), 0.0f);
    std::fill(fdlLi_.begin(), fdlLi_.end(), 0.0f);
    std::fill(fdlRr_.begin(), fdlRr_.end(), 0.0f);
    std::fill(fdlRi_.begin(), fdlRi_.end(), 0.0f);
    pos_ = 0;
    head_ = 0;
    previous_ = nullptr;
}

void PartitionedConvolver::process(float* left, float* right, int n, float wetGain, float dryGain) {
    if (block_ == 0) return;
    for (int i = 0; i < n; ++i) {
        inL_[block_ + pos_] = left[i];
        inR_[block_ + pos_] = right[i];
        const float dl = inL_[pos_], dr = inR_[pos_];  // input delayed by one block
        left[i] = outL_[pos_] * wetGain + dl * dryGain;
        right[i] = outR_[pos_] * wetGain + dr * dryGain;
        if (++pos_ == block_) {
            runBlock();
            pos_ = 0;
        }
    }
}

void PartitionedConvolver::accumulate(const ConvKernel* k, float* yLr, float* yLi, float* yRr, float* yRi) {
    std::fill(yLr, yLr + bins_, 0.0f);
    std::fill(yLi, yLi + bins_, 0.0f);
    std::fill(yRr, yRr + bins_, 0.0f);
    std::fill(yRi, yRi + bins_, 0.0f);
    if (!k || k->bins != bins_) return;
    const int parts = std::min(k->partitions, maxParts_);
    auto mac = [this](const float* xr, const float* xi, const float* hr, const float* hi, float* yr, float* yi) {
        for (int b = 0; b < bins_; ++b) {
            yr[b] += xr[b] * hr[b] - xi[b] * hi[b];
            yi[b] += xr[b] * hi[b] + xi[b] * hr[b];
        }
    };
    for (int p = 0; p < parts; ++p) {
        int slot = head_ - p;
        if (slot < 0) slot += maxParts_;
        const size_t off = static_cast<size_t>(slot) * bins_;
        const float* xLr = fdlLr_.data() + off;
        const float* xLi = fdlLi_.data() + off;
        const float* xRr = fdlRr_.data() + off;
        const float* xRi = fdlRi_.data() + off;
        switch (k->paths) {
            case 1:
                mac(xLr, xLi, k->pathRe(0, p), k->pathIm(0, p), yLr, yLi);
                mac(xRr, xRi, k->pathRe(0, p), k->pathIm(0, p), yRr, yRi);
                break;
            case 2:
                mac(xLr, xLi, k->pathRe(0, p), k->pathIm(0, p), yLr, yLi);
                mac(xRr, xRi, k->pathRe(1, p), k->pathIm(1, p), yRr, yRi);
                break;
            default:  // 4: LL, LR, RL, RR
                mac(xLr, xLi, k->pathRe(0, p), k->pathIm(0, p), yLr, yLi);
                mac(xLr, xLi, k->pathRe(1, p), k->pathIm(1, p), yRr, yRi);
                mac(xRr, xRi, k->pathRe(2, p), k->pathIm(2, p), yLr, yLi);
                mac(xRr, xRi, k->pathRe(3, p), k->pathIm(3, p), yRr, yRi);
                break;
        }
    }
}

void PartitionedConvolver::runBlock() {
    const size_t off = static_cast<size_t>(head_) * bins_;
    fft_.forward(inL_.data(), fdlLr_.data() + off, fdlLi_.data() + off);
    fft_.forward(inR_.data(), fdlRr_.data() + off, fdlRi_.data() + off);

    accumulate(current_, accLr_.data(), accLi_.data(), accRr_.data(), accRi_.data());
    fft_.inverse(accLr_.data(), accLi_.data(), tmp_.data());
    std::memcpy(outL_.data(), tmp_.data() + block_, sizeof(float) * block_);
    fft_.inverse(accRr_.data(), accRi_.data(), tmp_.data());
    std::memcpy(outR_.data(), tmp_.data() + block_, sizeof(float) * block_);

    if (previous_) {
        accumulate(previous_, acc2Lr_.data(), acc2Li_.data(), acc2Rr_.data(), acc2Ri_.data());
        const float inc = 1.0f / static_cast<float>(block_);
        fft_.inverse(acc2Lr_.data(), acc2Li_.data(), tmp2_.data());
        for (int i = 0; i < block_; ++i) {
            const float t = (i + 1) * inc;
            outL_[i] = tmp2_[block_ + i] + (outL_[i] - tmp2_[block_ + i]) * t;
        }
        fft_.inverse(acc2Rr_.data(), acc2Ri_.data(), tmp2_.data());
        for (int i = 0; i < block_; ++i) {
            const float t = (i + 1) * inc;
            outR_[i] = tmp2_[block_ + i] + (outR_[i] - tmp2_[block_ + i]) * t;
        }
        previous_ = nullptr;
    }

    std::memcpy(inL_.data(), inL_.data() + block_, sizeof(float) * block_);
    std::memcpy(inR_.data(), inR_.data() + block_, sizeof(float) * block_);
    head_ = (head_ + 1) % maxParts_;
}

}  // namespace rfx
