#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <thread>

#include <unistd.h>

#include "Logger.h"
#include "ipc/BinderProtocol.h"
#include "ipc/BinderClientAdapter.h"

namespace {
volatile std::sig_atomic_t gRunning = 1;

void signalHandler(int) {
    gRunning = 0;
}

bool readMemory(xmonitor::MemoryData& outData) {
    std::ifstream statm("/proc/self/statm");
    if (!statm.is_open()) {
        LOG_E("Memory read failed: cannot open /proc/self/statm");
        return false;
    }

    std::uint64_t sizePages = 0;
    std::uint64_t residentPages = 0;
    statm >> sizePages >> residentPages;

    if (sizePages == 0 && residentPages == 0) {
        LOG_E("Memory read failed: invalid size/resident pages");
        return false;
    }

    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        LOG_E("Memory read failed: invalid page size");
        return false;
    }

    outData.virtualBytes = sizePages * static_cast<std::uint64_t>(pageSize);
    outData.residentBytes = residentPages * static_cast<std::uint64_t>(pageSize);

    static int sampleCounter = 0;
    ++sampleCounter;
    if (sampleCounter % 10 == 0) {
        LOG_D("Memory read ok: rss=%llu virt=%llu",
              static_cast<unsigned long long>(outData.residentBytes),
              static_cast<unsigned long long>(outData.virtualBytes));
    }

    return true;
}
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    setLogFilePath("logs/xMonitor-memory.log");
    LOG_I("Memory service start");

    xmonitor::BinderClientAdapter binder;

    xmonitor::MemoryData lastPublished{};
    bool hasLastPublished = false;

    if (!binder.initialize()) {
        LOG_E("Memory service binder initialize failed");
        return 1;
    }

    LOG_I("Memory service binder initialize success");

    {
        xmonitor::BinderAck ack{};
        const std::uint32_t request = 1;
        std::size_t replySize = 0;
        if (!binder.transact(static_cast<std::uint32_t>(xmonitor::BinderTransactionCode::RegisterMemoryService),
                     &request,
                     sizeof(request),
                     &ack,
                     sizeof(ack),
                     replySize) ||
            replySize != sizeof(ack) || ack.ok == 0) {
            LOG_E("Memory service register to lifecycle failed");
            binder.shutdown();
            return 1;
        }
    }

    while (gRunning != 0) {
        xmonitor::BinderAck ack{};
        const std::uint32_t request = 1;
        std::size_t replySize = 0;
        if (binder.transact(static_cast<std::uint32_t>(xmonitor::BinderTransactionCode::WaitStart),
                            &request,
                            sizeof(request),
                            &ack,
                            sizeof(ack),
                            replySize) &&
            replySize == sizeof(ack) && ack.ok != 0 && ack.startGranted != 0) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (gRunning == 0) {
        binder.shutdown();
        LOG_I("Memory service stop");
        return 0;
    }

    LOG_I("Memory service start streaming");

    while (gRunning != 0) {
        xmonitor::MemoryData current{};
        if (readMemory(current)) {
            if (!hasLastPublished ||
                current.virtualBytes != lastPublished.virtualBytes ||
                current.residentBytes != lastPublished.residentBytes) {
                hasLastPublished = true;
                lastPublished = current;
                const bool sent = binder.send(static_cast<std::uint32_t>(xmonitor::BinderTransactionCode::MemoryUpdated),
                                              &current,
                                              sizeof(current));
                if (!sent) {
                    LOG_E("Memory service binder send failed");
                    binder.shutdown();
                    return 1;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    binder.shutdown();
    LOG_I("Memory service stop");
    return 0;
}
