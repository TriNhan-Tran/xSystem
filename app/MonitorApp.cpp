#include <any>
#include <csignal>
#include <cstdio>
#include <iomanip>
#include <ncurses.h>
#include <sstream>
#include <string>

#include "app/MonitorApp.h"
#include "Logger.h"
#include "ipc/BinderProtocol.h"

namespace xmonitor {
namespace {
volatile std::sig_atomic_t gStopRequested = 0;

std::string formatBytes(std::uint64_t bytes) {
    static const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    std::size_t unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        ++unitIndex;
    }

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(unitIndex == 0 ? 0 : 2) << value << " " << kUnits[unitIndex];
    return stream.str();
}

void signalHandler(int) {
    gStopRequested = 1;
}
} // namespace

MonitorApp::MonitorApp() = default;

MonitorApp::~MonitorApp() {
    requestStop();
    mBinderAdapter.shutdown();
}

void MonitorApp::handleMessage(const Message& message) {
    {
        std::lock_guard<std::mutex> lock(mDataMutex);

        switch (message.what) {
            case CPU_UPDATE:
                if (message.obj.type() == typeid(CpuData)) {
                    mCpuData = std::any_cast<CpuData>(message.obj);
                }
                break;
            case RAM_UPDATE:
                if (message.obj.type() == typeid(RamData)) {
                    mRamData = std::any_cast<RamData>(message.obj);
                }
                break;
            case MEMORY_UPDATE:
                if (message.obj.type() == typeid(MemoryData)) {
                    mMemoryData = std::any_cast<MemoryData>(message.obj);
                }
                break;
            default:
                break;
        }
    }
}

void MonitorApp::run() {
    setLogFilePath("logs/xMonitor-app.log");
    LOG_I("MonitorApp start");

    gStopRequested = 0;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    if (!mBinderAdapter.initialize()) {
        LOG_E("MonitorApp binder initialize(client) failed");
        return;
    }

    if (!registerToLifecycle()) {
        LOG_E("MonitorApp register to lifecycle failed");
        mBinderAdapter.shutdown();
        return;
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    while (gStopRequested == 0) {
        BinderSnapshot snapshot{};
        if (querySnapshot(snapshot)) {
            publishSnapshot(snapshot);
        }

        {
            std::lock_guard<std::mutex> lock(mDataMutex);
            redrawUnlocked();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    LOG_W("Stop requested by signal");

    endwin();

    mBinderAdapter.shutdown();
    LOG_I("MonitorApp stop");
}

void MonitorApp::requestStop() {
    gStopRequested = 1;
}

bool MonitorApp::registerToLifecycle() {
    BinderAck ack{};
    const std::uint32_t request = 1;
    std::size_t replySize = 0;

    const bool ok = mBinderAdapter.transact(
        static_cast<std::uint32_t>(BinderTransactionCode::RegisterApp),
        &request,
        sizeof(request),
        &ack,
        sizeof(ack),
        replySize);

    if (!ok || replySize != sizeof(ack)) {
        return false;
    }

    return ack.ok != 0;
}

bool MonitorApp::querySnapshot(BinderSnapshot& snapshot) {
    const std::uint32_t request = 1;
    std::size_t replySize = 0;
    const bool ok = mBinderAdapter.transact(
        static_cast<std::uint32_t>(BinderTransactionCode::QuerySnapshot),
        &request,
        sizeof(request),
        &snapshot,
        sizeof(snapshot),
        replySize);

    return ok && replySize == sizeof(snapshot);
}

void MonitorApp::publishSnapshot(const BinderSnapshot& snapshot) {
    Message cpuMessage;
    cpuMessage.what = CPU_UPDATE;
    cpuMessage.obj = snapshot.cpu;
    postMessage(cpuMessage);

    Message ramMessage;
    ramMessage.what = RAM_UPDATE;
    ramMessage.obj = snapshot.ram;
    postMessage(ramMessage);

    Message memoryMessage;
    memoryMessage.what = MEMORY_UPDATE;
    memoryMessage.obj = snapshot.memory;
    postMessage(memoryMessage);
}

void MonitorApp::redrawUnlocked() const {
    erase();

    mvprintw(0, 0, "xMonitor - Linux System Monitor (ncurses)");
    mvprintw(1, 0, "=======================================");

    mvprintw(3, 0, "CPU Usage      : %.2f%%", mCpuData.usagePercent);

    const std::string used = formatBytes(mRamData.usedBytes);
    const std::string total = formatBytes(mRamData.totalBytes);
    mvprintw(4, 0, "RAM Usage      : %.2f%% (Used %s / Total %s)",
             mRamData.usagePercent,
             used.c_str(),
             total.c_str());

    const std::string rss = formatBytes(mMemoryData.residentBytes);
    const std::string virt = formatBytes(mMemoryData.virtualBytes);
    mvprintw(5, 0, "Process Memory : RSS %s, VIRT %s", rss.c_str(), virt.c_str());

    mvprintw(7, 0, "Press Ctrl+C to exit.");
    refresh();
}

} // namespace xmonitor
