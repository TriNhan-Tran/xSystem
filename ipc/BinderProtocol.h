#pragma once

#include <cstdint>

namespace xmonitor {

struct CpuData {
    std::uint64_t totalJiffies{0};
    std::uint64_t idleJiffies{0};
    double usagePercent{0.0};
};

struct RamData {
    std::uint64_t totalBytes{0};
    std::uint64_t usedBytes{0};
    std::uint64_t availableBytes{0};
    double usagePercent{0.0};
};

struct MemoryData {
    std::uint64_t virtualBytes{0};
    std::uint64_t residentBytes{0};
};

enum class BinderTransactionCode : std::uint32_t {
    CpuUpdated = 1,
    RamUpdated = 2,
    MemoryUpdated = 3,
    RegisterApp = 100,
    RegisterCpuService = 101,
    RegisterRamService = 102,
    RegisterMemoryService = 103,
    WaitStart = 104,
    QuerySnapshot = 105
};

struct BinderAck {
    std::uint32_t ok;
    std::uint32_t startGranted;
};

struct BinderSnapshot {
    CpuData cpu;
    RamData ram;
    MemoryData memory;
};

enum MonitorMessageId : int {
    CPU_UPDATE = 1,
    RAM_UPDATE = 2,
    MEMORY_UPDATE = 3
};

inline int binderCodeToMessageId(std::uint32_t code) {
    switch (static_cast<BinderTransactionCode>(code)) {
        case BinderTransactionCode::CpuUpdated:
            return CPU_UPDATE;
        case BinderTransactionCode::RamUpdated:
            return RAM_UPDATE;
        case BinderTransactionCode::MemoryUpdated:
            return MEMORY_UPDATE;
        default:
            return -1;
    }
}

inline std::uint32_t messageIdToBinderCode(int messageId) {
    switch (messageId) {
        case CPU_UPDATE:
            return static_cast<std::uint32_t>(BinderTransactionCode::CpuUpdated);
        case RAM_UPDATE:
            return static_cast<std::uint32_t>(BinderTransactionCode::RamUpdated);
        case MEMORY_UPDATE:
            return static_cast<std::uint32_t>(BinderTransactionCode::MemoryUpdated);
        default:
            return 0;
    }
}

} // namespace xmonitor
