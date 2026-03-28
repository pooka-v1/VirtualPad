#include "PadView.h"

#include <wincodec.h>
#include <unordered_set>
#include <vector>
#include <cstdio>

#include "../imgui/imgui.h"

#pragma comment(lib, "windowscodecs.lib")

// ---------------------------------------------------------------------------
// PNG loader — WIC (no extra dependencies)
// ---------------------------------------------------------------------------

bool PadView::loadPng(ID3D11Device* device, const char* path, PadTexture& out) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);  // safe to call multiple times

    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
        return false;

    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);

    IWICBitmapDecoder* decoder = nullptr;
    if (FAILED(factory->CreateDecoderFromFilename(wpath, nullptr,
            GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) {
        factory->Release();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        decoder->Release(); factory->Release();
        return false;
    }

    IWICFormatConverter* conv = nullptr;
    factory->CreateFormatConverter(&conv);
    conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT w, h;
    conv->GetSize(&w, &h);

    std::vector<uint8_t> pixels(w * h * 4);
    conv->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data());

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = w;
    td.Height           = h;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem     = pixels.data();
    sd.SysMemPitch = w * 4;

    bool ok = false;
    ID3D11Texture2D* tex = nullptr;
    if (SUCCEEDED(device->CreateTexture2D(&td, &sd, &tex))) {
        ok = SUCCEEDED(device->CreateShaderResourceView(tex, nullptr, &out.srv));
        tex->Release();
        if (ok) { out.w = (int)w; out.h = (int)h; }
    }

    conv->Release();
    frame->Release();
    decoder->Release();
    factory->Release();
    return ok;
}

// ---------------------------------------------------------------------------
// load / setLayout / unload
// ---------------------------------------------------------------------------

bool PadView::load(ID3D11Device* device) {
    m_device = device;
    m_loaded = true;
    return true;
}

void PadView::setLayout(const PadLayout& layout) {
    if (!m_device || !m_loaded) { m_layout = layout; return; }
    if (layout.id == m_layout.id) return;

    // Collect all image names referenced by the new layout
    std::unordered_set<std::string> needed;
    for (const auto& c : layout.components) {
        if (!c.image.empty())       needed.insert(c.image);
        if (!c.overlay.empty())     needed.insert(c.overlay);
        if (!c.imageUp.empty())    { needed.insert(c.imageUp);
                                     needed.insert(c.imageDown);
                                     needed.insert(c.imageLeft);
                                     needed.insert(c.imageRight); }
    }

    // Release textures no longer needed
    for (auto it = m_textures.begin(); it != m_textures.end(); ) {
        if (needed.find(it->first) == needed.end()) {
            it->second.release();
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }

    // Load any texture not yet present
    for (const auto& name : needed) {
        if (m_textures.find(name) != m_textures.end()) continue;
        PadTexture tex;
        char path[256];
        snprintf(path, sizeof(path), "images/%s", name.c_str());
        loadPng(m_device, path, tex);           // non-fatal if file missing
        m_textures.emplace(name, std::move(tex));
    }

    m_layout = layout;
}

void PadView::unload() {
    for (auto& [name, tex] : m_textures)
        tex.release();
    m_textures.clear();
    m_device = nullptr;
    m_loaded = false;
}

// ---------------------------------------------------------------------------
// State resolvers
// ---------------------------------------------------------------------------

static bool resolveState(const GamepadState& s, const std::string& name, float threshold) {
    if (name == "btnA")      return s.btnA;
    if (name == "btnB")      return s.btnB;
    if (name == "btnX")      return s.btnX;
    if (name == "btnY")      return s.btnY;
    if (name == "btnLB")     return s.btnLB;
    if (name == "btnRB")     return s.btnRB;
    if (name == "btnL3")     return s.btnL3;
    if (name == "btnR3")     return s.btnR3;
    if (name == "btnL4")     return s.btnL4;
    if (name == "btnR4")     return s.btnR4;
    if (name == "btnLP")     return s.btnLP;
    if (name == "btnRP")     return s.btnRP;
    if (name == "btnBack")   return s.btnBack;
    if (name == "btnStart")  return s.btnStart;
    if (name == "btnHome")   return s.btnHome;
    if (name == "dpadUp")    return s.dpadUp;
    if (name == "dpadDown")  return s.dpadDown;
    if (name == "dpadLeft")  return s.dpadLeft;
    if (name == "dpadRight") return s.dpadRight;
    if (name == "triggerL")  return s.triggerL > threshold;
    if (name == "triggerR")  return s.triggerR > threshold;
    return false;
}

static float resolveFloat(const GamepadState& s, const std::string& name) {
    if (name == "leftX")    return s.leftX;
    if (name == "leftY")    return s.leftY;
    if (name == "rightX")   return s.rightX;
    if (name == "rightY")   return s.rightY;
    if (name == "triggerL") return s.triggerL;
    if (name == "triggerR") return s.triggerR;
    return 0.0f;
}

// ---------------------------------------------------------------------------
// getTex
// ---------------------------------------------------------------------------

const PadTexture* PadView::getTex(const std::string& name) const {
    if (name.empty()) return nullptr;
    auto it = m_textures.find(name);
    return (it != m_textures.end() && it->second.valid()) ? &it->second : nullptr;
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------

void PadView::render(const GamepadState& state) {
    if (!m_loaded) {
        ImGui::TextDisabled("Assets not loaded.");
        return;
    }

    const PadLayout& L  = m_layout;
    ImVec2      origin  = ImGui::GetCursorScreenPos();
    ImDrawList* dl      = ImGui::GetWindowDrawList();

    // Draw a texture centered at (cx, cy).
    auto img = [&](const PadTexture& t,
                   float cx, float cy, float w, float h,
                   ImVec4 tint) {
        if (!t.valid()) return;
        ImVec2 p0 = { origin.x + cx - w * 0.5f, origin.y + cy - h * 0.5f };
        ImVec2 p1 = { p0.x + w, p0.y + h };
        dl->AddImage((ImTextureID)(intptr_t)t.srv, p0, p1,
                     { 0, 0 }, { 1, 1 }, ImGui::ColorConvertFloat4ToU32(tint));
    };

    for (const auto& c : L.components) {
        const ImVec4 col         = { c.colorR,         c.colorG,         c.colorB,         c.colorA         };
        const ImVec4 activeCol   = { c.activeColorR,   c.activeColorG,   c.activeColorB,   c.activeColorA   };
        const ImVec4 ovCol       = { c.ovColorR,       c.ovColorG,       c.ovColorB,       c.ovColorA       };
        const ImVec4 activeOvCol = { c.activeOvColorR, c.activeOvColorG, c.activeOvColorB, c.activeOvColorA };

        if (c.type == "template") {
            const PadTexture* t = getTex(c.image);
            if (t) img(*t, c.cx, c.cy, c.w, c.h, col);
        }
        else if (c.type == "button") {
            bool pressed = resolveState(state, c.state, c.threshold);
            const PadTexture* t  = getTex(c.image);
            const PadTexture* ov = getTex(c.overlay);
            if (t)  img(*t,  c.cx, c.cy, c.w, c.h,
                        pressed ? activeCol : col);
            if (ov) img(*ov, c.cx, c.cy, c.w * c.overlayScaleX, c.h * c.overlayScaleY,
                        pressed ? activeOvCol : ovCol);
        }
        else if (c.type == "stick") {
            float dx    = resolveFloat(state, c.stateX);
            float dy    = resolveFloat(state, c.stateY);
            bool  click = resolveState(state, c.stateClick, 0.5f);
            const PadTexture* t = getTex(c.image);
            if (t) img(*t, c.cx, c.cy, c.size, c.size, click ? activeCol : col);
            float hx = c.cx + dx * c.maxOffset;
            float hy = c.cy - dy * c.maxOffset;
            ImVec2 hc = { origin.x + hx, origin.y + hy };
            // Dot color: brightened inactive color when released, active color when clicked
            ImVec4 dotCol = click ? activeCol
                                  : ImVec4{ c.colorR + 0.35f, c.colorG + 0.35f, c.colorB + 0.35f, 1.0f };
            dl->AddCircleFilled(hc, 5.5f, ImGui::ColorConvertFloat4ToU32(dotCol));
            dl->AddCircle(hc, 5.5f, ImGui::ColorConvertFloat4ToU32({ 0.15f, 0.15f, 0.15f, 1.0f }), 12, 1.5f);
        }
        else if (c.type == "decoration") {
            // Non-interactive visual element (USB port, logo, label, etc.).
            // Uses natural image size if w/h are not specified in the layout.
            const PadTexture* t = getTex(c.image);
            if (t) {
                float dw = c.w > 0.0f ? c.w : (float)t->w;
                float dh = c.h > 0.0f ? c.h : (float)t->h;
                img(*t, c.cx, c.cy, dw, dh, col);
            }
        }
        else if (c.type == "dpad") {
            // Each arm is sized to its texture's natural dimensions, offset from the dpad center.
            auto drawArm = [&](const std::string& imgName, const std::string& stName,
                               float acx, float acy) {
                const PadTexture* t = getTex(imgName);
                if (!t) return;
                bool pressed = resolveState(state, stName, 0.5f);
                img(*t, acx, acy, (float)t->w, (float)t->h, pressed ? activeCol : col);
            };
            const PadTexture* tUp    = getTex(c.imageUp);
            const PadTexture* tDown  = getTex(c.imageDown);
            const PadTexture* tLeft  = getTex(c.imageLeft);
            const PadTexture* tRight = getTex(c.imageRight);
            if (tUp)    drawArm(c.imageUp,    c.stateUp,    c.cx,                          c.cy - tUp->h    * 0.5f + 2.0f);
            if (tDown)  drawArm(c.imageDown,  c.stateDown,  c.cx,                          c.cy + tDown->h  * 0.5f);
            if (tLeft)  drawArm(c.imageLeft,  c.stateLeft,  c.cx - tLeft->w  * 0.5f,       c.cy);
            if (tRight) drawArm(c.imageRight, c.stateRight, c.cx + tRight->w * 0.5f,       c.cy);
        }
    }

    // Advance ImGui layout cursor past the drawn area.
    ImGui::Dummy({ L.W, L.FrontH + L.TopH });
}
