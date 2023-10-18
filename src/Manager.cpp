#include "Manager.h"

std::shared_ptr<Manager> Manager::Create(MainLoop& mainLoop, std::shared_ptr<Outputs> outputs,
                                         std::vector<std::unique_ptr<Panel>>&& panels) {
    auto manager = std::shared_ptr<Manager>(new Manager(outputs, std::move(panels)));
    mainLoop.RegisterBatchHandler(manager);
    return manager;
}

void Manager::DirtyWorkspace() {
    // No need to redraw when not visible
    if (!m_isVisible) return;

    // Redraw dirty panels
    for (const auto& panel : m_panels) {
        if (!panel->IsDirty()) continue;
        auto p = panel.get();
        m_outputs->ForEach([p](std::shared_ptr<Output> output) { p->Draw(*output); });
    }
}
void Manager::OnBatchProcessed() { DirtyWorkspace(); }

void Manager::Hide() {
    m_outputs->ForEach([](std::shared_ptr<Output> output) {
        for (const auto& surface : output->surfaces) {
            surface->Hide();
        }
    });
    m_isVisible = false;
}

void Manager::Show() {
    m_isVisible = true;
    for (const auto& panel : m_panels) {
        auto p = panel.get();
        m_outputs->ForEach([p](std::shared_ptr<Output> output) { p->Draw(*output); });
    }
}