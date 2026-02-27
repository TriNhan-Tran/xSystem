#include <chrono>
#include <csignal>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <thread>

#include "Logger.h"
#include "ipc/BinderProtocol.h"
#include "ipc/BinderClientAdapter.h"

namespace {
volatile std::sig_atomic_t gRunning = 1;

void signalHandler(int) {
    gRunning = 0;
}

bool readCpu(xmonitor::CpuData& outData, std::uint64_t& previousTotal, std::uint64_t& previousIdle) {
    std::ifstream statFile("/proc/stat");
    if (!statFile.is_open()) {
        LOG_E("CPU read failed: cannot open /proc/stat");
        return false;
    }

    std::string line;
    if (!std::getline(statFile, line)) {
        LOG_E("CPU read failed: cannot read first line /proc/stat");
        return false;
    }

    std::istringstream lineStream(line);
    std::string cpuLabel;
    std::uint64_t user = 0;
    std::uint64_t nice = 0;
    std::uint64_t system = 0;
    std::uint64_t idle = 0;
    std::uint64_t iowait = 0;
    std::uint64_t irq = 0;
    std::uint64_t softirq = 0;
    std::uint64_t steal = 0;

    lineStream >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    if (cpuLabel != "cpu") {
        LOG_E("CPU read failed: invalid label '%s'", cpuLabel.c_str());
        return false;
    }

    const std::uint64_t idleAll = idle + iowait;
    const std::uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;

    double usage = 0.0;
    if (previousTotal != 0 && total >= previousTotal && idleAll >= previousIdle) {
        const std::uint64_t totalDelta = total - previousTotal;
        const std::uint64_t idleDelta = idleAll - previousIdle;
        if (totalDelta > 0) {
            usage = (static_cast<double>(totalDelta - idleDelta) * 100.0) / static_cast<double>(totalDelta);
        }
    }

    previousTotal = total;
    previousIdle = idleAll;

    outData.totalJiffies = total;
    outData.idleJiffies = idleAll;
    outData.usagePercent = usage;

    static int sampleCounter = 0;
    ++sampleCounter;
    if (sampleCounter % 10 == 0) {
        LOG_D("CPU read ok: usage=%.2f total=%llu idle=%llu",
              outData.usagePercent,
              static_cast<unsigned long long>(outData.totalJiffies),
              static_cast<unsigned long long>(outData.idleJiffies));
    }

    return true;
}
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    setLogFilePath("logs/xMonitor-cpu.log");
    LOG_I("CPU service start");

    xmonitor::BinderClientAdapter binder;

    xmonitor::CpuData lastPublished{};
    bool hasLastPublished = false;
    std::uint64_t previousTotal = 0;
    std::uint64_t previousIdle = 0;

    if (!binder.initialize()) {
        LOG_E("CPU service binder initialize failed");
        return 1;
    }

    LOG_I("CPU service binder initialize success");

    {
        xmonitor::BinderAck ack{};
        const std::uint32_t request = 1;
        std::size_t replySize = 0;
        if (!binder.transact(static_cast<std::uint32_t>(xmonitor::BinderTransactionCode::RegisterCpuService),
                     &request,
                     sizeof(request),
                     &ack,
                     sizeof(ack),
                     replySize) ||
            replySize != sizeof(ack) || ack.ok == 0) {
            LOG_E("CPU service register to lifecycle failed");
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
        LOG_I("CPU service stop");
        return 0;
    }

    LOG_I("CPU service start streaming");

    while (gRunning != 0) {
        xmonitor::CpuData current{};
        if (readCpu(current, previousTotal, previousIdle)) {
            if (!hasLastPublished || std::fabs(current.usagePercent - lastPublished.usagePercent) >= 0.01) {
                hasLastPublished = true;
                lastPublished = current;
                const bool sent = binder.send(static_cast<std::uint32_t>(xmonitor::BinderTransactionCode::CpuUpdated),
                                              &current,
                                              sizeof(current));
                if (!sent) {
                    LOG_E("CPU service binder send failed");
                    binder.shutdown();
                    return 1;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    binder.shutdown();
    LOG_I("CPU service stop");
    return 0;
}
