#pragma once

#include "src/MainLoop.h"
#include "src/Outputs.h"
#include "src/Panel.h"

class Manager : public IoBatchHandler {
   public:
    static std::shared_ptr<Manager> Create(MainLoop& mainLoop, std::shared_ptr<Outputs> outputs,
                                           std::vector<std::unique_ptr<Panel>>&& panels);

    void Show();
    void Hide();
    void OnBatchProcessed();
    void DirtyWorkspace();

   private:
    Manager(std::shared_ptr<Outputs> outputs, std::vector<std::unique_ptr<Panel>>&& panels)
        : m_outputs(outputs), m_panels(std::move(panels)) {}

    bool m_isVisible;
    std::shared_ptr<Outputs> m_outputs;
    std::vector<std::unique_ptr<Panel>> m_panels;
};