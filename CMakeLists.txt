cmake_minimum_required(VERSION 3.10)

project(xMonitor VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Curses REQUIRED)

set(XMONITOR_COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/linux_binder
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/MessageQueue
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Logger/LogFile
)

set(XMONITOR_BINDER_SOURCES
    ipc/BinderClientAdapter.cpp
    ipc/BinderServerAdapter.cpp
    third_party/linux_binder/binder.c
)

set(XMONITOR_LOGGER_SOURCES
    third_party/Logger/LogFile/Logger.cpp
)

add_executable(${PROJECT_NAME}
    main.cpp
    app/MonitorApp.cpp
    ${XMONITOR_BINDER_SOURCES}
    ${XMONITOR_LOGGER_SOURCES}
    third_party/MessageQueue/Looper.cpp
    third_party/MessageQueue/Handler.cpp
    third_party/MessageQueue/Message.cpp
    third_party/MessageQueue/Processor.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE ${XMONITOR_COMMON_INCLUDE_DIRS} ${CURSES_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${CURSES_LIBRARIES}
)

add_executable(xMonitorCpuService
    service/CpuService.cpp
    ${XMONITOR_BINDER_SOURCES}
    ${XMONITOR_LOGGER_SOURCES}
)

target_include_directories(xMonitorCpuService
    PRIVATE ${XMONITOR_COMMON_INCLUDE_DIRS}
)

add_executable(xMonitorRamService
    service/RamService.cpp
    ${XMONITOR_BINDER_SOURCES}
    ${XMONITOR_LOGGER_SOURCES}
)

target_include_directories(xMonitorRamService
    PRIVATE ${XMONITOR_COMMON_INCLUDE_DIRS}
)

add_executable(xMonitorMemoryService
    service/MemoryService.cpp
    ${XMONITOR_BINDER_SOURCES}
    ${XMONITOR_LOGGER_SOURCES}
)

target_include_directories(xMonitorMemoryService
    PRIVATE ${XMONITOR_COMMON_INCLUDE_DIRS}
)

add_executable(xMonitorLifecycle
    lifecycle/LifecycleMain.cpp
    ${XMONITOR_BINDER_SOURCES}
    ${XMONITOR_LOGGER_SOURCES}
)

target_include_directories(xMonitorLifecycle
    PRIVATE ${XMONITOR_COMMON_INCLUDE_DIRS}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
    target_link_libraries(xMonitorCpuService PRIVATE pthread)
    target_link_libraries(xMonitorRamService PRIVATE pthread)
    target_link_libraries(xMonitorMemoryService PRIVATE pthread)
    target_link_libraries(xMonitorLifecycle PRIVATE pthread)
endif()
