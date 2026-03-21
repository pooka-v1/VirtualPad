#include "AppWindow.h"

#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Forward-declare the ImGui Win32 message handler.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AppWindow::AppWindow(PadEngine& engine)
    : m_engine(engine) {}

AppWindow::~AppWindow() {
    cleanup();
}

// ---------------------------------------------------------------------------
// run() — entry point called from main()
// ---------------------------------------------------------------------------

int AppWindow::run() {
    ImGui_ImplWin32_EnableDpiAwareness();

    if (!initWindow()) return 1;
    if (!initD3D())    { DestroyWindow(m_hwnd); return 1; }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename  = nullptr;  // don't save layout to disk

    ImGui::StyleColorsDark();

    // Tweak style for a slightly cleaner look
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.FramePadding      = { 6.0f, 4.0f };
    style.ItemSpacing       = { 8.0f, 6.0f };

    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_device, m_context);

    // Start the gamepad engine on a background thread
    m_engine.start();

    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hwnd);

    // --- Message + render loop ---
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Skip rendering when the window is occluded (e.g. minimised to tray)
        if (m_swapChainOccluded && m_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            continue;
        }
        m_swapChainOccluded = false;

        // Build the ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        renderFrame();

        ImGui::Render();

        // Clear the back buffer and render ImGui draw data
        const float clearColor[4] = { 0.10f, 0.10f, 0.11f, 1.00f };
        m_context->OMSetRenderTargets(1, &m_renderTarget, nullptr);
        m_context->ClearRenderTargetView(m_renderTarget, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = m_swapChain->Present(1, 0);  // vsync (1)
        if (hr == DXGI_STATUS_OCCLUDED) m_swapChainOccluded = true;
    }

    // Stop the engine before tearing down rendering resources
    m_engine.stop();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanup();
    return 0;
}

// ---------------------------------------------------------------------------
// Per-frame UI
// ---------------------------------------------------------------------------

void AppWindow::renderFrame() {
    // One full-screen window with no decorations — acts as the app canvas
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ 0.0f, 0.0f });
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##canvas", nullptr,
        ImGuiWindowFlags_NoTitleBar  |
        ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove      |
        ImGuiWindowFlags_NoCollapse  |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // --- Title ---
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
    ImGui::Text("VirtualPad");
    ImGui::PopStyleColor();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120.0f);
    ImGui::TextDisabled("A1 — base GUI");
    ImGui::Separator();
    ImGui::Spacing();

    // --- Device status ---
    bool connected = m_engine.isConnected();
    bool running   = m_engine.isRunning();

    if (connected) {
        ImGui::TextColored({ 0.3f, 1.0f, 0.3f, 1.0f }, "●");
        ImGui::SameLine();
        ImGui::Text("Connected — %s", m_engine.getDevice().c_str());
    } else if (running) {
        ImGui::TextColored({ 1.0f, 0.8f, 0.0f, 1.0f }, "●");
        ImGui::SameLine();
        ImGui::Text("Waiting for device...");
    } else {
        ImGui::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "●");
        ImGui::SameLine();
        ImGui::Text("Engine stopped");
    }

    ImGui::Spacing();

    // --- Status line ---
    ImGui::TextDisabled("Status:");
    ImGui::SameLine();
    ImGui::Text("%s", m_engine.getStatus().c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- Info footer ---
    ImGui::TextDisabled("Console window shows full log output.");
    ImGui::TextDisabled("Close this window to exit.");

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Window / D3D11 initialisation
// ---------------------------------------------------------------------------

bool AppWindow::initWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"VirtualPadWindow";
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        0, L"VirtualPadWindow", L"VirtualPad",
        WS_OVERLAPPEDWINDOW,
        100, 100, 900, 600,
        nullptr, nullptr, wc.hInstance, this);  // pass 'this' for WndProc lookup

    return m_hwnd != nullptr;
}

bool AppWindow::initD3D() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = m_hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL       featureLevel;
    UINT                    flags = 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        featureLevels, 2, D3D11_SDK_VERSION,
        &sd, &m_swapChain, &m_device, &featureLevel, &m_context);

    // Fallback to WARP (software) renderer if hardware is unavailable
    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
            featureLevels, 2, D3D11_SDK_VERSION,
            &sd, &m_swapChain, &m_device, &featureLevel, &m_context);

    if (FAILED(hr)) return false;

    createRenderTarget();
    return true;
}

void AppWindow::createRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTarget);
        backBuffer->Release();
    }
}

void AppWindow::cleanupRenderTarget() {
    if (m_renderTarget) { m_renderTarget->Release(); m_renderTarget = nullptr; }
}

void AppWindow::cleanup() {
    cleanupRenderTarget();
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_context)   { m_context->Release();   m_context   = nullptr; }
    if (m_device)    { m_device->Release();    m_device    = nullptr; }
    if (m_hwnd)      { DestroyWindow(m_hwnd);  m_hwnd      = nullptr; }
    UnregisterClassW(L"VirtualPadWindow", GetModuleHandle(nullptr));
}

// ---------------------------------------------------------------------------
// Win32 window procedure
// ---------------------------------------------------------------------------

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Let ImGui handle its own input first
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    // Retrieve our AppWindow instance stored at window creation
    AppWindow* self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        // Store the AppWindow pointer so later messages can reach it
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    case WM_SIZE:
        if (self && self->m_device && wParam != SIZE_MINIMIZED) {
            self->cleanupRenderTarget();
            self->m_swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam),
                                             DXGI_FORMAT_UNKNOWN, 0);
            self->createRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;  // suppress Alt menu flicker
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
