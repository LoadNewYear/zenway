#include "PowerSource.h"

#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/timerfd.h>

#include <filesystem>
#include <optional>
#include <string>

std::shared_ptr<PowerSource> PowerSource::Create(std::string_view name, MainLoop& mainLoop,
                                                 std::shared_ptr<ScriptContext> scriptContext) {
    // Use non blocking to make sure we never hang on read
    auto fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (fd == -1) {
        spdlog::error("Failed to create timer: {}", strerror(errno));
        return nullptr;
    }
    itimerspec timer = {.it_interval = {.tv_sec = 30, .tv_nsec = 0},
                        .it_value = {.tv_sec = 1, .tv_nsec = 0}};
    auto ret = timerfd_settime(fd, 0, &timer, nullptr);
    if (ret == -1) {
        spdlog::error("Failed to set timer: {}", strerror(errno));
        return nullptr;
    }
    auto source = std::shared_ptr<PowerSource>(new PowerSource(name, fd, scriptContext));
    mainLoop.Register(fd, name, source);
    return source;
}

bool PowerSource::Initialize() {
    // Publish initial value to make sure that something is published
    m_scriptContext->Publish(m_name,
                             PowerState{.IsPluggedIn = false, .IsCharging = false, .Capacity = 0});
    // TODO: Probe that these exists
    m_ac = "/sys/class/power_supply/AC/online";
    // TODO: Always BAT0?
    // TODO: More than one battery?
    m_batteryCapacity = "/sys/class/power_supply/BAT0/capacity";
    m_batteryStatus = "/sys/class/power_supply/BAT0/status";
    ReadState();
    return true;
}

static std::optional<std::string> ReadAll(int fd) {
    constexpr size_t size = 512;
    char buf[size];
    auto n = read(fd, buf, size);
    if (n == -1) {
        // Probably blocking..
        spdlog::warn("Power source blocked read");
        return {};
    }
    if (n == size) {
        // Read exact amount, might be more, not sure
        spdlog::warn("Power source too much data");
        return {};
    }
    return std::string(buf, n);
}

static std::optional<std::string> Read(const char* path, const char* kind) {
    auto fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        spdlog::error("Power source failed to open {} for {}: {}", path, kind, strerror(errno));
        return {};
    }
    auto s = ReadAll(fd);
    close(fd);
    if (!s) {
        return {};
    }
    return s;
}

void PowerSource::ReadState() {
    auto state = PowerState{};
    // Use non-blocking to avoid sporadic hanging
    // Current capacity
    auto maybeString = Read(m_batteryCapacity.c_str(), "capacity");
    if (maybeString) {
        state.Capacity = std::stoi(*maybeString);
    }
    // Battery status
    maybeString = Read(m_batteryStatus.c_str(), "status");
    if (maybeString) {
        state.IsCharging = *maybeString == "Charging";
    }
    // AC status
    maybeString = Read(m_ac.c_str(), "AC");
    if (maybeString) {
        state.IsPluggedIn = std::stoi(*maybeString) != 0;
    }
    m_sourceDirtyFlag = state != m_sourceState;
    if (m_sourceDirtyFlag) {
        m_sourceState = state;
        m_scriptContext->Publish(m_name, m_sourceState);
        spdlog::info("Power status changed, capacity {}, charging {}, plugged in {}",
                     state.Capacity, state.IsCharging, state.IsPluggedIn);
    }
}

PowerSource::~PowerSource() { close(m_timerfd); }

bool PowerSource::OnRead() {
    spdlog::debug("Polling power status");
    uint64_t ignore;
    auto n = read(m_timerfd, &ignore, sizeof(ignore));
    if (n <= 0) {
        // Either block or no events
        return false;
    }
    ReadState();
    return m_sourceDirtyFlag;
}
