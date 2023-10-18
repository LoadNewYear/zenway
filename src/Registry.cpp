#include "Registry.h"

#include <spdlog/spdlog.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wlr-layer-shell-unstable-v1.h>

#include <ostream>

#include "ShellSurface.h"

void Registry::Register(struct wl_registry *registry, uint32_t name, const char *interface,
                        uint32_t version) {
    if (interface == std::string_view(zwlr_layer_shell_v1_interface.name)) {
        roots->shell = (zwlr_layer_shell_v1 *)wl_registry_bind(
            registry, name, &zwlr_layer_shell_v1_interface, zwlr_layer_shell_v1_interface.version);
        return;
    }
    if (interface == std::string_view(wl_shm_interface.name)) {
        roots->shm =
            (wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface, wl_shm_interface.version);
        return;
    }
    if (interface == std::string_view(wl_compositor_interface.name)) {
        roots->compositor = (wl_compositor *)wl_registry_bind(
            registry, name, &wl_compositor_interface, 5 /*wl_compositor_interface.version*/);
        return;
    }
    if (interface == std::string_view(wl_output_interface.name)) {
        auto output = (wl_output *)wl_registry_bind(registry, name, &wl_output_interface,
                                                    wl_output_interface.version);
        outputs->Add(output);
        return;
    }
    spdlog::trace("Event wl_registry::register {} {}", interface, version);
}

void Registry::Unregister(struct wl_registry *registry, uint32_t name) {
    // TODO: Outputs will be removed here
}

static void on_register(void *data, struct wl_registry *wlregistry, uint32_t name,
                        const char *interface, uint32_t version) {
    auto registry = (Registry *)(data);
    registry->Register(wlregistry, name, interface, version);
}

static void on_unregister(void *data, struct wl_registry *wlregistry, uint32_t name) {
    auto registry = (Registry *)(data);
    registry->Unregister(wlregistry, name);
}

static const wl_registry_listener listener = {.global = on_register,
                                              .global_remove = on_unregister};

std::shared_ptr<Registry> Registry::Create(MainLoop &mainLoop) {
    // Connect
    auto display = wl_display_connect(std::getenv("WAYLAND_DISPLAY"));
    if (display == nullptr) {
        spdlog::error("Failed to connect to wayland");
        return nullptr;
    }
    auto wlregistry = wl_display_get_registry(display);
    auto registry = std::shared_ptr<Registry>(new Registry(display, wlregistry));
    // Will fill in roots and this instance with needed interfaces
    wl_registry_add_listener(wlregistry, &listener, registry.get());
    // Two roundtrips, first to trigger registration, second to process binding requests.
    if (wl_display_roundtrip(display) < 0 || wl_display_roundtrip(display) < 0) return nullptr;
    // Register in mainloop
    mainLoop.Register(wl_display_get_fd(display), "wayland", registry);
    return registry;
}

void Registry::OnRead() {
    /*
      while (wl_display_prepare_read(roots->display) != 0) {
          wl_display_dispatch(roots->display);
      }
      */
    wl_display_prepare_read(roots->display);
    wl_display_read_events(roots->display);
    wl_display_dispatch_pending(roots->display);
    wl_display_flush(roots->display);
    // roots->FlushAndDispatchCommands();
}