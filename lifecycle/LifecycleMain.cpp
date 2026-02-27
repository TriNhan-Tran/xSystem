#include <csignal>
#include <cstdint>
#include <cstring>
#include <thread>

#include "Logger.h"
#include "ipc/BinderProtocol.h"
#include "ipc/BinderServerAdapter.h"

namespace {
volatile std::sig_atomic_t gRunning = 1;

void signalHandler(int) {
    gRunning = 0;
}
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    setLogFilePath("logs/xMonitor-lifecycle.log");
    LOG_I("Lifecycle start");

    xmonitor::BinderServerAdapter binder;
    if (!binder.initializeContextManager()) {
        LOG_E("Lifecycle binder initialize(context manager) failed");
        return 1;
    }

    struct State {
        xmonitor::BinderSnapshot snapshot{};
        bool hasApp{false};
        bool hasCpuService{false};
        bool hasRamService{false};
        bool hasMemoryService{false};
        bool startGranted{false};
    } state;

    binder.setTransactionCallback([&](std::uint32_t code, const void* payload, std::size_t payloadSize) {
        const auto txnCode = static_cast<xmonitor::BinderTransactionCode>(code);

        switch (txnCode) {
            case xmonitor::BinderTransactionCode::RegisterApp:
            case xmonitor::BinderTransactionCode::RegisterCpuService:
            case xmonitor::BinderTransactionCode::RegisterRamService:
            case xmonitor::BinderTransactionCode::RegisterMemoryService:
            case xmonitor::BinderTransactionCode::WaitStart: {
                xmonitor::BinderAck ack{};
                ack.ok = 1;

                if (txnCode == xmonitor::BinderTransactionCode::RegisterApp) {
                    state.hasApp = true;
                    LOG_I("Lifecycle: app registered");
                } else if (txnCode == xmonitor::BinderTransactionCode::RegisterCpuService) {
                    state.hasCpuService = true;
                    LOG_I("Lifecycle: CPU service registered");
                } else if (txnCode == xmonitor::BinderTransactionCode::RegisterRamService) {
                    state.hasRamService = true;
                    LOG_I("Lifecycle: RAM service registered");
                } else if (txnCode == xmonitor::BinderTransactionCode::RegisterMemoryService) {
                    state.hasMemoryService = true;
                    LOG_I("Lifecycle: Memory service registered");
                }

                state.startGranted = state.hasApp && state.hasCpuService && state.hasRamService && state.hasMemoryService;
                ack.startGranted = state.startGranted ? 1u : 0u;

                if (!binder.reply(code, &ack, sizeof(ack))) {
                    LOG_E("Lifecycle: reply failed for code=%u", code);
                }
                break;
            }
            case xmonitor::BinderTransactionCode::QuerySnapshot: {
                if (!binder.reply(code, &state.snapshot, sizeof(state.snapshot))) {
                    LOG_E("Lifecycle: snapshot reply failed");
                }
                break;
            }
            case xmonitor::BinderTransactionCode::CpuUpdated: {
                if (state.startGranted && payload != nullptr && payloadSize == sizeof(xmonitor::CpuData)) {
                    state.snapshot.cpu = *reinterpret_cast<const xmonitor::CpuData*>(payload);
                }
                break;
            }
            case xmonitor::BinderTransactionCode::RamUpdated: {
                if (state.startGranted && payload != nullptr && payloadSize == sizeof(xmonitor::RamData)) {
                    state.snapshot.ram = *reinterpret_cast<const xmonitor::RamData*>(payload);
                }
                break;
            }
            case xmonitor::BinderTransactionCode::MemoryUpdated: {
                if (state.startGranted && payload != nullptr && payloadSize == sizeof(xmonitor::MemoryData)) {
                    state.snapshot.memory = *reinterpret_cast<const xmonitor::MemoryData*>(payload);
                }
                break;
            }
            default:
                break;
        }
    });

    std::thread loopThread([&]() {
        binder.loop();
    });

    while (gRunning != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    binder.shutdown();
    if (loopThread.joinable()) {
        loopThread.join();
    }

    LOG_I("Lifecycle stop");
    return 0;
}
