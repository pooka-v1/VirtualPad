#pragma once
#include <d3d11.h>
#include <unordered_map>
#include <string>
#include "../GamepadState.h"
#include "PadLayout.h"

// RAII wrapper for a single D3D11 shader resource view loaded from a PNG.
struct PadTexture {
    ID3D11ShaderResourceView* srv = nullptr;
    int w = 0, h = 0;

    bool valid() const { return srv != nullptr; }
    void release() { if (srv) { srv->Release(); srv = nullptr; } w = h = 0; }

    PadTexture() = default;
    PadTexture(const PadTexture&)            = delete;
    PadTexture& operator=(const PadTexture&) = delete;
    PadTexture(PadTexture&& o) noexcept
        : srv(o.srv), w(o.w), h(o.h) { o.srv = nullptr; o.w = o.h = 0; }
    PadTexture& operator=(PadTexture&& o) noexcept {
        if (this != &o) { release(); srv = o.srv; w = o.w; h = o.h; o.srv = nullptr; o.w = o.h = 0; }
        return *this;
    }
};

// Renders a visual representation of the active gamepad state.
// Fully data-driven: the component list in PadLayout describes every element to draw.
class PadView {
public:
    // Initialise D3D device reference. Textures are loaded on the first setLayout() call.
    bool load(ID3D11Device* device);

    // Switch to a new layout: loads any new textures, releases unused ones.
    // Ignored if layout.id is unchanged.
    void setLayout(const PadLayout& layout);

    // Render the pad view inside the current ImGui window at the current cursor position.
    void render(const GamepadState& state);

    // Release all D3D11 resources. Call before releasing the D3D device.
    void unload();

private:
    static bool loadPng(ID3D11Device* device, const char* path, PadTexture& out);
    const PadTexture* getTex(const std::string& name) const;

    ID3D11Device* m_device = nullptr;
    bool          m_loaded = false;
    PadLayout     m_layout;

    std::unordered_map<std::string, PadTexture> m_textures;
};
