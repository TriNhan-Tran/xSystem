#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

struct binder_state;
struct binder_transaction_data;

namespace xmonitor {

class BinderServerAdapter {
public:
    using TransactionCallback = std::function<void(std::uint32_t, const void*, std::size_t)>;

    BinderServerAdapter();
    ~BinderServerAdapter();

    BinderServerAdapter(const BinderServerAdapter&) = delete;
    BinderServerAdapter& operator=(const BinderServerAdapter&) = delete;

    bool initializeContextManager();
    void shutdown();
    bool isEnabled() const;

    bool reply(std::uint32_t code, const void* payload, std::size_t payloadSize);
    void setTransactionCallback(TransactionCallback callback);
    void loop();

private:
    static void transactionHandlerThunk(struct binder_state* bs, struct binder_transaction_data* txn);
    void onTransaction(struct binder_transaction_data* txn);

    binder_state* mBinderState;
    TransactionCallback mTransactionCallback;

    static BinderServerAdapter* sLoopOwner;
};

} // namespace xmonitor
