#include "ipc/BinderClientAdapter.h"

#include <cerrno>
#include <cstring>

#include "Logger.h"

extern "C" {
#include "binder.h"
}

namespace xmonitor {

BinderClientAdapter::BinderClientAdapter()
    : mBinderState(nullptr) {}

BinderClientAdapter::~BinderClientAdapter() {
    shutdown();
}

bool BinderClientAdapter::initialize() {
    if (mBinderState != nullptr) {
        return true;
    }

    static const char* kCandidates[] = {
        "/dev/binderfs/binder",
    };

    for (const char* candidate : kCandidates) {
        binder_state* state = binder_open(candidate);
        if (state == nullptr) {
            LOG_E("binder_open failed on %s", candidate);
            continue;
        }

        mBinderState = state;
        LOG_I("binder client init success: path=%s", candidate);
        return true;
    }

    LOG_E("binder client init failed");
    return false;
}

void BinderClientAdapter::shutdown() {
    if (mBinderState != nullptr) {
        binder_close(mBinderState);
        mBinderState = nullptr;
    }
}

bool BinderClientAdapter::isEnabled() const {
    return mBinderState != nullptr;
}

bool BinderClientAdapter::send(std::uint32_t code, const void* payload, std::size_t payloadSize) {
    if (mBinderState == nullptr || payload == nullptr || payloadSize == 0) {
        LOG_E("binder send rejected: state=%p payload=%p size=%zu",
              static_cast<void*>(mBinderState),
              payload,
              payloadSize);
        return false;
    }

    const int rc = binder_call(
        mBinderState,
        0,
        code,
        const_cast<void*>(payload),
        payloadSize);

    if (rc != 0) {
        LOG_E("binder_call failed: code=%u size=%zu errno=%d msg=%s",
              code,
              payloadSize,
              errno,
              std::strerror(errno));
        return false;
    }

    return true;
}

bool BinderClientAdapter::transact(std::uint32_t code,
                                   const void* payload,
                                   std::size_t payloadSize,
                                   void* replyBuffer,
                                   std::size_t replyCapacity,
                                   std::size_t& replySize) {
    replySize = 0;

    if (mBinderState == nullptr || payload == nullptr || payloadSize == 0 ||
        replyBuffer == nullptr || replyCapacity == 0) {
        LOG_E("binder transact rejected: state=%p payload=%p size=%zu reply=%p cap=%zu",
              static_cast<void*>(mBinderState),
              payload,
              payloadSize,
              replyBuffer,
              replyCapacity);
        return false;
    }

    size_t nativeReplySize = 0;
    const int rc = binder_transact(
        mBinderState,
        0,
        code,
        const_cast<void*>(payload),
        payloadSize,
        replyBuffer,
        replyCapacity,
        &nativeReplySize);

    if (rc != 0) {
        LOG_E("binder_transact failed: code=%u size=%zu errno=%d msg=%s",
              code,
              payloadSize,
              errno,
              std::strerror(errno));
        return false;
    }

    replySize = nativeReplySize;
    return true;
}

} // namespace xmonitor
