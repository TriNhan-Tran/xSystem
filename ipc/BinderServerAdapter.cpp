#include "ipc/BinderServerAdapter.h"

#include <cerrno>
#include <cstring>
#include <utility>
#include <vector>

#include "Logger.h"

extern "C" {
#include "binder.h"
}

namespace xmonitor {

BinderServerAdapter* BinderServerAdapter::sLoopOwner = nullptr;

BinderServerAdapter::BinderServerAdapter()
    : mBinderState(nullptr) {}

BinderServerAdapter::~BinderServerAdapter() {
    shutdown();
}

bool BinderServerAdapter::initializeContextManager() {
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

        if (binder_become_context_manager(state) != 0) {
            LOG_E("binder_become_context_manager failed on %s errno=%d msg=%s",
                  candidate,
                  errno,
                  std::strerror(errno));
            binder_close(state);
            continue;
        }

        mBinderState = state;
        LOG_I("binder server init success: path=%s", candidate);
        return true;
    }

    LOG_E("binder server init failed");
    return false;
}

void BinderServerAdapter::shutdown() {
    if (mBinderState != nullptr) {
        binder_close(mBinderState);
        mBinderState = nullptr;
    }

    if (sLoopOwner == this) {
        sLoopOwner = nullptr;
    }
}

bool BinderServerAdapter::isEnabled() const {
    return mBinderState != nullptr;
}

bool BinderServerAdapter::reply(std::uint32_t code, const void* payload, std::size_t payloadSize) {
    if (mBinderState == nullptr || payload == nullptr || payloadSize == 0) {
        LOG_E("binder reply rejected: state=%p payload=%p size=%zu",
              static_cast<void*>(mBinderState),
              payload,
              payloadSize);
        return false;
    }

    const int rc = binder_send_reply(
        mBinderState,
        code,
        const_cast<void*>(payload),
        payloadSize);

    if (rc != 0) {
        LOG_E("binder_send_reply failed: code=%u size=%zu errno=%d msg=%s",
              code,
              payloadSize,
              errno,
              std::strerror(errno));
        return false;
    }

    return true;
}

void BinderServerAdapter::setTransactionCallback(TransactionCallback callback) {
    mTransactionCallback = std::move(callback);
}

void BinderServerAdapter::loop() {
    if (mBinderState == nullptr) {
        return;
    }

    sLoopOwner = this;
    binder_loop(mBinderState, &BinderServerAdapter::transactionHandlerThunk);
}

void BinderServerAdapter::transactionHandlerThunk(struct binder_state*, struct binder_transaction_data* txn) {
    if (sLoopOwner != nullptr) {
        sLoopOwner->onTransaction(txn);
    }
}

void BinderServerAdapter::onTransaction(struct binder_transaction_data* txn) {
    if (txn == nullptr || txn->data_size == 0 || txn->data.ptr.buffer == 0) {
        return;
    }

    if (!mTransactionCallback) {
        return;
    }

    std::vector<std::uint8_t> payload(txn->data_size);
    std::memcpy(payload.data(), reinterpret_cast<void*>(txn->data.ptr.buffer), txn->data_size);
    mTransactionCallback(txn->code, payload.data(), payload.size());
}

} // namespace xmonitor
