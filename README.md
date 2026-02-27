# xMonitor

Linux system monitor with layered architecture:

 App process (`xMonitor`): binder context manager + terminal rendering with `ncurses`
 Service processes: `xMonitorCpuService`, `xMonitorRamService`, `xMonitorMemoryService`
 Sampling policy: each service samples every 100ms and only sends when data changed
 IPC layer: real `linux_binder` transactions from services to app
 App event loop: app inherits `Processor` (MessageQueue) and handles binder events via queued messages

## Structure

 `app` - `MonitorApp` (`Processor`-based app message loop)
 `service` - independent service process entries
 `ipc` - binder protocol + linux_binder adapter
- `third_party/MessageQueue` - message queue + processor primitives used by callback thread pool
- `common` - shared monitor data structs
- `main.cpp` - app bootstrap
- `third_party/linux_binder` - vendored binder implementation

## Build

From workspace root:

```bash
make xMonitor
```

## Run

From workspace root:
 start app first (binder context manager):

```bash
make run-xMonitor
```

Press `Ctrl+C` to stop.
 From `xMonitor/build`, run services in separate terminals:

 ```bash
 ./xMonitorCpuService
 ./xMonitorRamService
 ./xMonitorMemoryService
 ```

 Press `Ctrl+C` to stop each process.
