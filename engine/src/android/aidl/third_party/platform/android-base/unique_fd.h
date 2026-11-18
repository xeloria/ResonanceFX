// SPDX-License-Identifier: Apache-2.0
// Minimal android-base/unique_fd.h for the out-of-tree AIDL effect build.
//
// libbase is not in the NDK. The FMQ headers only need a move-only owning fd.
#pragma once

#include <unistd.h>

namespace android::base {

class unique_fd {
  public:
    unique_fd() = default;
    explicit unique_fd(int fd) : fd_(fd) {}
    unique_fd(unique_fd&& other) noexcept : fd_(other.release()) {}
    unique_fd& operator=(unique_fd&& other) noexcept {
        reset(other.release());
        return *this;
    }
    unique_fd(const unique_fd&) = delete;
    unique_fd& operator=(const unique_fd&) = delete;

    ~unique_fd() { reset(); }

    int get() const { return fd_; }
    bool operator==(int fd) const { return fd_ == fd; }
    bool operator!=(int fd) const { return fd_ != fd; }
    bool operator<(int fd) const { return fd_ < fd; }
    bool operator>(int fd) const { return fd_ > fd; }
    bool ok() const { return fd_ >= 0; }
    explicit operator bool() const { return ok(); }
    int operator*() const { return fd_; }

    int release() {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void reset(int fd = -1) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = fd;
    }

  private:
    int fd_ = -1;
};

}  // namespace android::base

