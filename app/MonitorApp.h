#pragma once

#include <mutex>

#include "ipc/BinderProtocol.h"
#include "ipc/BinderClientAdapter.h"
#include "Processor.h"

namespace xmonitor {

class MonitorApp : public Processor {
public:
    MonitorApp();
    ~MonitorApp() override;

    void handleMessage(const Message& message) override;

    void run();
    void requestStop();

private:
    bool registerToLifecycle();
    bool querySnapshot(BinderSnapshot& snapshot);
    void publishSnapshot(const BinderSnapshot& snapshot);
    void redrawUnlocked() const;

    mutable std::mutex mDataMutex;
    CpuData mCpuData{};
    RamData mRamData{};
    MemoryData mMemoryData{};

    BinderClientAdapter mBinderAdapter;
};

} // namespace xmonitor
