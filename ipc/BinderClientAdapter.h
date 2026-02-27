#pragma once

#include <cstddef>
#include <cstdint>

struct binder_state;

namespace xmonitor {

class BinderClientAdapter {
public:
    BinderClientAdapter();
    ~BinderClientAdapter();

    BinderClientAdapter(const BinderClientAdapter&) = delete;
    BinderClientAdapter& operator=(const BinderClientAdapter&) = delete;

    bool initialize();
    void shutdown();
    bool isEnabled() const;

    bool send(std::uint32_t code, const void* payload, std::size_t payloadSize);
    bool transact(std::uint32_t code,
                  const void* payload,
                  std::size_t payloadSize,
                  void* replyBuffer,
                  std::size_t replyCapacity,
                  std::size_t& replySize);

private:
    binder_state* mBinderState;
};

} // namespace xmonitor
