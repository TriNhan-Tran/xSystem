#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "Logger.h"
#include "ipc/BinderProtocol.h"
#include "ipc/BinderClientAdapter.h"

namespace {
volatile std::sig_atomic_t gRunning = 1;

void signalHandler(int) {
    gRunning = 0;
}

bool readRam(xmonitor::RamData& outData) {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        LOG_E("RAM read failed: cannot open /proc/meminfo");
        return false;
    }

    std::string line;
    std::uint64_t memTotalKb = 0;
    std::uint64_t memAvailableKb = 0;

    while (std::getline(meminfo, line)) {
        std::istringstream stream(line);
        std::string key;
        std::uint64_t value = 0;
        std::string unit;
        stream >> key >> value >> unit;

        if (key == "MemTotal:") {
            memTotalKb = value;
        } else if (key == "MemAvailable:") {
            memAvailableKb = value;
        }

        if (memTotalKb > 0 && memAvailableKb > 0) {
            break;
        }
    }

    if (memTotalKb == 0) {
        LOG_E("RAM read failed: MemTotal missing in /proc/meminfo");
        return false;
    }

    const std::uint64_t totalBytes = memTotalKb * 1024;
    const std::uint64_t availableBytes = memAvailableKb * 1024;
    const std::uint64_t usedBytes = totalBytes >= availableBytes ? totalBytes - availableBytes : 0;
    const double usagePercent = totalBytes > 0
        ? static_cast<double>(usedBytes) * 100.0 / static_cast<double>(totalBytes)
        : 0.0;

    outData.totalBytes = totalBytes;
    outData.usedBytes = usedBytes;
    outData.availableBytes = availableBytes;
    outData.usagePercent = usagePercent;

    static int sampleCounter = 0;
    ++sampleCounter;
    if (sampleCounter % 10 == 0) {
        LOG_D("RAM read ok: usage=%.2f used=%llu total=%llu available=%llu",
              outData.usagePercent,
              static_cast<unsigned long long>(outData.usedBytes),
              static_cast<unsigned long long>(outData.totalBytes),
              static_cast<unsigned long long>(outData.availableBytes));
    }

    return true;
}
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    setLogFilePath("logs/xMonitor-ram.log");
    LOG_I("RAM service start");

    xmonitor::BinderClientAdapter binder;

    xmonitor::RamData lastPublished{};
    bool hasLastPublished = false;

    if (!binder.initialize()) {
        LOG_E("RAM service binder initialize failed");
        return 1;
    }

    LOG_I("RAM service binder initialize success");

    {
        xmonitor::BinderAck ack{};
        const std::uint32_t request = 1;
        std::size_t replySize = 0;
        if (!binder.transact(static_cast<std::uint32_t>(xmonitor::BinderTransactionCode::RegisterRamService),
                     &request,
                     sizeof(request),
                     &ack,
                     sizeof(ack),
                     replySize) ||
            replySize != sizeof(ack) || ack.ok == 0) {
            LOG_E("RAM service register to lifecycle failed");
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
        LOG_I("RAM service stop");
        return 0;
    }

    LOG_I("RAM service start streaming");

    while (gRunning != 0) {
        xmonitor::RamData current{};
        if (readRam(current)) {
            if (!hasLastPublished ||
                current.totalBytes != lastPublished.totalBytes ||
                current.usedBytes != lastPublished.usedBytes ||
                current.availableBytes != lastPublished.availableBytes) {
                hasLastPublished = true;
                lastPublished = current;
                const bool sent = binder.send(static_cast<std::uint32_t>(xmonitor::BinderTransactionCode::RamUpdated),
                                              &current,
                                              sizeof(current));
                if (!sent) {
                    LOG_E("RAM service binder send failed");
                    binder.shutdown();
                    return 1;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    binder.shutdown();
    LOG_I("RAM service stop");
    return 0;
}
