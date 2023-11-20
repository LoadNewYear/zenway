#pragma once

#include <wayland-client-protocol.h>
#include <wlr-layer-shell-unstable-v1.h>

#include <cstdint>
#include <list>
#include <memory>

#include "MainLoop.h"
#include "Output.h"
#include "Roots.h"
#include "Seat.h"
#include "ShellSurface.h"

class Registry : public IoHandler {
   public:
    static std::shared_ptr<Registry> Create(MainLoop &mainLoop);
    virtual ~Registry() {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }

    void Register(struct wl_registry *registry, uint32_t name, const char *interface,
                  uint32_t version);
    void Unregister(struct wl_registry *registry, uint32_t name);

    virtual void OnRead() override;

    // These are maintained by the registry
    std::shared_ptr<Roots> roots;
    std::shared_ptr<Outputs> outputs;
    std::shared_ptr<Seat> seat;

   private:
    Registry(wl_display *display, wl_registry *registry) : m_registry(registry) {
        roots = std::make_shared<Roots>();
        roots->display = display;
        outputs = Outputs::Create(roots);
    }

   private:
    wl_registry *m_registry;
};
