#include "PowerSource.h"

#include <spdlog/spdlog.h>
#include <sys/timerfd.h>

#include <filesystem>
#include <fstream>

std::shared_ptr<PowerSource> PowerSource::Create(MainLoop& mainLoop,
                                                 std::shared_ptr<ScriptContext> scriptContext) {
    auto fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (fd == -1) {
        spdlog::error("Failed to create timer: {}", strerror(errno));
        return nullptr;
    }
    itimerspec timer = {.it_interval = {.tv_sec = 30}, .it_value = {.tv_sec = 1}};
    auto ret = timerfd_settime(fd, 0, &timer, nullptr);
    if (ret == -1) {
        spdlog::error("Failed to set timer: {}", strerror(errno));
        return nullptr;
    }
    auto source = std::shared_ptr<PowerSource>(new PowerSource(fd, scriptContext));
    mainLoop.Register(fd, "PowerSource", source);
    return source;
}

bool PowerSource::Initialize() {
    // TODO: Probe that these exists
    m_ac = "/sys/class/power_supply/AC";
    m_battery = "/sys/class/power_supply/BAT0";
    ReadState();
    return true;
}

void PowerSource::ReadState() {
    auto state = PowerState{};
    // Read current charge
    {
        std::ifstream f(m_battery / "capacity");
        int capacity;
        f >> capacity;
        state.Capacity = (uint8_t)capacity;
    }
    // Read battery status
    {
        std::ifstream f(m_battery / "status");
        std::string status;
        f >> status;
        state.IsCharging = status == "Charging";
    }
    // Read AC status
    {
        std::ifstream f(m_ac / "online");
        int online;
        f >> online;
        state.IsPluggedIn = online != 0;
    }
    m_sourceDirtyFlag = state != m_sourceState;
    if (m_sourceDirtyFlag) {
        m_sourceState = state;
        m_scriptContext->Publish(m_sourceState);
        spdlog::info("Power status changed, capacity {}, charging {}, plugged in {}",
                     state.Capacity, state.IsCharging, state.IsPluggedIn);
    }
}

PowerSource::~PowerSource() { close(m_timerfd); }

bool PowerSource::OnRead() {
    spdlog::debug("Polling power status");
    uint64_t ignore;
    read(m_timerfd, &ignore, sizeof(ignore));
    ReadState();
    return m_sourceDirtyFlag;
}
