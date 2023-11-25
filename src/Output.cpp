#include "Output.h"

#include "Panel.h"
#include "Roots.h"
#include "spdlog/spdlog.h"

class Output {
    using OnNamedCallback = std::function<void(Output *output, const std::string &name)>;

   public:
    static void Create(const std::shared_ptr<Roots> roots, wl_output *wloutput,
                       const wl_output_listener *listener, std::shared_ptr<Configuration> config,
                       OnNamedCallback onNamed) {
        // This will be in lingo until name is received
        auto output = new Output(wloutput, config, onNamed);
        wl_output_add_listener(wloutput, listener, output);
    }

    void OnName(const char *name) {
        // This will only be invoked once per lifetime of output
        m_name = name;
        spdlog::trace("Event wl_output::name {}", name);
        m_onNamed(this, m_name);
        m_onNamed = nullptr;
    }

    void OnDescription(const char *description) {
        // Might change over lifetime
        spdlog::trace("Event wl_output::description {}", description);
    }

    virtual ~Output() {
        wl_output_destroy(m_wloutput);
        m_wloutput = nullptr;
    }

    void Draw(const Configuration::Panel &panelConfig, std::shared_ptr<Roots> roots,
              BufferPool &bufferPool) {
        auto drawn = Panel::Draw(panelConfig, m_name, bufferPool);
        if (!drawn.buffer) {
            // Nothing drawn for this output
            return;
        }
        // Ensure that there is a surface for this panel
        if (!m_surfaces.contains(panelConfig.index)) {
            auto surface = ShellSurface::Create(roots, m_wloutput);
            if (!surface) {
                spdlog::error("Failed to create surface");
                return;
            }
            m_surfaces[panelConfig.index] = std::move(surface);
        }
        m_surfaces[panelConfig.index]->Draw(panelConfig.anchor, *drawn.buffer, drawn.size);
    }
    void Hide() {
        for (const auto &kv : m_surfaces) {
            kv.second->Hide();
        }
    }

   private:
    Output(wl_output *wloutput, std::shared_ptr<Configuration> config, OnNamedCallback onNamed)
        : m_wloutput(wloutput), m_config(config), m_onNamed(onNamed) {}

    std::map<int, std::unique_ptr<ShellSurface>> m_surfaces;  // Surface per panel index
    wl_output *m_wloutput;
    const std::shared_ptr<Configuration> m_config;
    // Temporary callback until named, registers amoung the other outputs when name received
    OnNamedCallback m_onNamed;
    std::string m_name;
};

static void on_name(void *data, struct wl_output *wl_output, const char *name) {
    ((Output *)data)->OnName(name);
}

void on_description(void *data, struct wl_output *wl_output, const char *description) {
    ((Output *)data)->OnDescription(description);
}

void on_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y,
                 int32_t physical_width, int32_t physical_height, int32_t subpixel,
                 const char *make, const char *model, int32_t transform) {}

void on_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height,
             int32_t refresh) {}

void on_done(void *data, struct wl_output *wl_output) {}

void on_scale(void *data, struct wl_output *wl_output, int32_t factor) {}

const struct wl_output_listener listener = {
    .geometry = on_geometry,
    .mode = on_mode,
    .done = on_done,
    .scale = on_scale,
    .name = on_name,
    .description = on_description,
};

std::unique_ptr<Outputs> Outputs::Create(std::shared_ptr<Configuration> config) {
    return std::unique_ptr<Outputs>(new Outputs(config));
}

bool Outputs::Initialize(const std::shared_ptr<Roots> roots) {
    m_bufferPool = BufferPool::Create(roots, m_config->numBuffers, m_config->bufferWidth,
                                      m_config->bufferHeight);
    if (!m_bufferPool) {
        spdlog::error("Failed to initialize buffer pool");
        return false;
    }
    m_roots = roots;
    return true;
}

void Outputs::Add(wl_output *wloutput) {
    Output::Create(m_roots, wloutput, &listener, m_config, [this](auto output, auto name) {
        spdlog::debug("Found output {}", name);
        m_map[name] = std::shared_ptr<Output>(output);
    });
}

void Outputs::Draw(const Sources &sources) {
    for (const auto &panelConfig : m_config->panels) {
        for (const auto &widgetConfig : panelConfig.widgets) {
            if (sources.IsDirty(widgetConfig.sources)) {
                // This panel is dirty, redraw it on every output
                for (const auto &keyValue : m_map) {
                    keyValue.second->Draw(panelConfig, m_roots, *m_bufferPool);
                }
                continue;
            }
        }
    }
}

void Outputs::Hide() {
    for (auto &keyValue : m_map) {
        keyValue.second->Hide();
    }
}

std::shared_ptr<Output> Outputs::Get(const std::string &name) const {
    auto keyValue = m_map.find(name);
    if (keyValue == m_map.end()) return nullptr;
    return keyValue->second;
}
