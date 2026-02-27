.PHONY: all build run clean stop xMonitor

all: build

xMonitor: build

build:
	@echo "Building xMonitor..."
	@mkdir -p build
	@cd ./build && cmake .. && cmake --build .

stop:
	@echo "Stopping xMonitor processes..."
	@pkill -f '(^|/)xMonitorLifecycle$$' 2>/dev/null || true
	@pkill -f '(^|/)xMonitorCpuService$$' 2>/dev/null || true
	@pkill -f '(^|/)xMonitorRamService$$' 2>/dev/null || true
	@pkill -f '(^|/)xMonitorMemoryService$$' 2>/dev/null || true
	@pkill -f '(^|/)xMonitor$$' 2>/dev/null || true
	@sleep 0.2

run: stop clean build
	@echo "Running xMonitor (app foreground, services background)..."; \
	cd ./build && \
	mkdir -p logs && \
	mount_opts=$$(findmnt -no OPTIONS . 2>/dev/null || true); \
	case ",$$mount_opts," in \
		*,noexec,*) \
			echo "Current filesystem is mounted with noexec: $$mount_opts"; \
			echo "Move project to exec-enabled path or remount without noexec."; \
			exit 1 ;; \
	esac; \
	chmod +x ./xMonitor ./xMonitorLifecycle ./xMonitorCpuService ./xMonitorRamService ./xMonitorMemoryService 2>/dev/null || true; \
	if [ ! -x ./xMonitor ] || [ ! -x ./xMonitorLifecycle ] || [ ! -x ./xMonitorCpuService ] || [ ! -x ./xMonitorRamService ] || [ ! -x ./xMonitorMemoryService ]; then \
		echo "Binary is not executable. Current permissions:"; \
		ls -l ./xMonitor ./xMonitorLifecycle ./xMonitorCpuService ./xMonitorRamService ./xMonitorMemoryService; \
		exit 1; \
	fi; \
	./xMonitorLifecycle & lifecycle_pid=$$!; \
	sleep 0.3; \
	./xMonitorCpuService & cpu_pid=$$!; \
	./xMonitorRamService & ram_pid=$$!; \
	./xMonitorMemoryService & mem_pid=$$!; \
	cleanup() { \
		pkill -P $$lifecycle_pid 2>/dev/null || true; \
		pkill -P $$cpu_pid 2>/dev/null || true; \
		pkill -P $$ram_pid 2>/dev/null || true; \
		pkill -P $$mem_pid 2>/dev/null || true; \
		kill $$cpu_pid 2>/dev/null || true; \
		kill $$ram_pid 2>/dev/null || true; \
		kill $$mem_pid 2>/dev/null || true; \
		kill $$lifecycle_pid 2>/dev/null || true; \
		pkill -f '(^|/)xMonitorLifecycle$$' 2>/dev/null || true; \
		pkill -f '(^|/)xMonitorCpuService$$' 2>/dev/null || true; \
		pkill -f '(^|/)xMonitorRamService$$' 2>/dev/null || true; \
		pkill -f '(^|/)xMonitorMemoryService$$' 2>/dev/null || true; \
		pkill -f '(^|/)xMonitor$$' 2>/dev/null || true; \
	}; \
	trap cleanup INT TERM EXIT; \
	./xMonitor; \
	cleanup

clean:
	@echo "Cleaning xMonitor..."
	@rm -rf xMonitor/build
