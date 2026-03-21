#include "PadEngine.h"

#include <iostream>
#include <cmath>
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <unordered_map>
#include <string>

#include "input/EightBitDoInputSource.h"
#include "output/ViGEmOutputAdapter.h"
#include "config/ConfigLoader.h"
#include "bots/LightningBot.h"
#include "macros/Macro.h"
#include "macros/MacroParser.h"

#pragma comment(lib, "ViGEmClient.lib")
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// ---------------------------------------------------------------------------
// Internal helpers (not exposed in the header)
// ---------------------------------------------------------------------------

struct JoyEntry {
    UINT    id;
    UINT    axes;
    UINT    buttons;
    WORD    wMid;
    WORD    wPid;
    wchar_t name[MAXPNAMELEN];
};

static std::vector<JoyEntry> scanPorts() {
    std::vector<JoyEntry> result;
    UINT numDevs = joyGetNumDevs();
    for (UINT id = 0; id < numDevs; ++id) {
        JOYINFOEX info = {};
        info.dwSize  = sizeof(JOYINFOEX);
        info.dwFlags = JOY_RETURNBUTTONS;
        if (joyGetPosEx(id, &info) != JOYERR_NOERROR) continue;

        JoyEntry e = {};
        e.id = id;
        JOYCAPS caps = {};
        if (joyGetDevCaps(id, &caps, sizeof(caps)) == JOYERR_NOERROR) {
            e.axes    = caps.wNumAxes;
            e.buttons = caps.wNumButtons;
            e.wMid    = caps.wMid;
            e.wPid    = caps.wPid;
            wcsncpy_s(e.name, caps.szPname, MAXPNAMELEN);
        } else {
            wcscpy_s(e.name, L"(unknown)");
        }
        result.push_back(e);
    }
    return result;
}

static int findBotBit(const ControllerConfig& cfg, const std::string& botName) {
    for (const auto& [bit, action] : cfg.buttons)
        if (action.type == ButtonActionType::Bot && action.name == botName)
            return bit;
    return 0;
}

// ---------------------------------------------------------------------------
// PadEngine
// ---------------------------------------------------------------------------

PadEngine::PadEngine()  = default;
PadEngine::~PadEngine() { stop(); }

void PadEngine::start() {
    if (m_running.exchange(true)) return;  // already running
    m_thread = std::thread(&PadEngine::threadFunc, this);
}

void PadEngine::stop() {
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
    m_connected = false;
}

std::string PadEngine::getDevice() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_device;
}

std::string PadEngine::getStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

void PadEngine::setDevice(const std::string& s) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_device = s;
}

void PadEngine::setStatus(const std::string& s) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_status = s;
}

// ---------------------------------------------------------------------------
// Background thread: mirrors the original VirtualPad.cpp main() logic.
// ---------------------------------------------------------------------------

void PadEngine::threadFunc() {
    setStatus("Scanning for devices...");
    std::cout << "\n=== VirtualPad — device init ===\n";
    std::cout << "Scanning for joystick devices...\n";

    // --- Step 1: Find the real controller BEFORE creating the virtual one ---
    UINT     joyPort = UINT_MAX;
    JoyEntry selected = {};

    while (m_running && joyPort == UINT_MAX) {
        auto entries = scanPorts();

        if (entries.empty()) {
            std::cout << "\rNo joystick found. Connect the controller in D-mode.    ";
            setStatus("No device found — connect controller");
            Sleep(500);
            continue;
        }

        if (entries.size() == 1) {
            selected = entries[0];
            joyPort  = selected.id;
            printf("\nAuto-selected port %u: %ls (%u axes, %u buttons) [VID=%04X PID=%04X]\n",
                joyPort, selected.name, selected.axes, selected.buttons,
                selected.wMid, selected.wPid);
        } else {
            std::cout << "\nMultiple joystick devices found:\n";
            for (auto& e : entries)
                printf("  Port %u: %ls (%u axes, %u buttons) [VID=%04X PID=%04X]\n",
                    e.id, e.name, e.axes, e.buttons, e.wMid, e.wPid);
            std::cout << "Enter port number: ";
            std::cin >> joyPort;
            for (auto& e : entries)
                if (e.id == joyPort) { selected = e; break; }
        }
    }

    if (!m_running) return;

    // --- Step 2: Load controller config ---
    setStatus("Loading config...");
    std::vector<ControllerConfig> configs;
    try {
        configs = loadControllerConfigs("data/controllers.json");
    } catch (const std::exception& ex) {
        std::cerr << "Error loading config: " << ex.what() << "\n";
        setStatus(std::string("Config error: ") + ex.what());
        m_running = false;
        return;
    }

    std::unordered_map<std::string, std::string> macroLibrary;
    try {
        macroLibrary = loadMacroLibrary("data/macros.json");
        if (!macroLibrary.empty())
            printf("Macro library loaded: %zu macros.\n", macroLibrary.size());
    } catch (const std::exception& ex) {
        fprintf(stderr, "Warning: could not load macro library: %s\n", ex.what());
    }

    const ControllerConfig* cfg = findConfig(configs, selected.wMid, selected.wPid);
    if (!cfg) {
        fprintf(stderr,
            "No config found for VID=%04X PID=%04X (%ls).\n"
            "Add an entry to configs/controllers.json and restart.\n",
            selected.wMid, selected.wPid, selected.name);
        setStatus("No config for this device");
        m_running = false;
        return;
    }
    printf("Config loaded: %s\n", cfg->source_name.c_str());
    setDevice(cfg->source_name);

    EightBitDoInputSource input(joyPort, *cfg);

    // --- Step 3: Initialize ViGEm after the real port is secured ---
    setStatus("Connecting to ViGEm...");
    ViGEmOutputAdapter output;
    LightningBot       bot;
    if (!output.isReady()) {
        std::cerr << "Aborting: could not create virtual pad.\n";
        setStatus("ViGEm error — is the driver installed?");
        m_running = false;
        return;
    }

    int lightningBotBit = findBotBit(*cfg, "LightningBot");
    if (lightningBotBit > 0)
        printf("LightningBot assigned to button %d.\n", lightningBotBit);

    // --- Step 4: Parse macros ---
    std::unordered_map<int, Macro>       macros;
    std::unordered_map<int, bool>        macroPrevBtn;
    std::unordered_map<int, std::string> macroNames;
    std::unordered_map<int, int>         macroRotCount;
    std::unordered_map<int, float>       macroLastRX;
    std::unordered_map<int, float>       macroLastRY;

    for (const auto& [bit, action] : cfg->buttons) {
        if (action.type != ButtonActionType::Macro) continue;
        std::string execution = action.execution;
        if (execution.empty()) {
            auto it = macroLibrary.find(action.name);
            if (it == macroLibrary.end()) {
                fprintf(stderr, "Warning: macro '%s' (button %d) not found in library — skipped.\n",
                        action.name.c_str(), bit);
                continue;
            }
            execution = it->second;
        }
        try {
            Macro m;
            MacroParser::parse(execution, m);
            macros[bit]        = std::move(m);
            macroPrevBtn[bit]  = false;
            macroNames[bit]    = action.name;
            macroRotCount[bit] = 0;
            macroLastRX[bit]   = 0.0f;
            macroLastRY[bit]   = 0.0f;
            printf("Macro '%s' assigned to button %d.\n", action.name.c_str(), bit);
        } catch (const std::exception& ex) {
            fprintf(stderr, "Error parsing macro '%s': %s\n", action.name.c_str(), ex.what());
        }
    }

    // --- Main loop ---
    setStatus("Running");
    m_connected = true;
    std::cout << "Forwarding input. Close the window to exit.\n\n";

    GamepadState state;
    bool         botBtnPrev = false;
    bool         btnAPrev   = false;

    while (m_running) {
        if (!input.isConnected()) {
            if (m_connected) {
                printf("\r[%s] disconnected. Waiting...    ", cfg->source_name.c_str());
                m_connected = false;
                setStatus("Device disconnected — waiting...");
            }
            Sleep(500);
            continue;
        }

        if (!m_connected) {
            m_connected = true;
            setStatus("Running");
        }

        // Read raw buttons for special actions (bots, macros)
        {
            JOYINFOEX raw = {};
            raw.dwSize  = sizeof(JOYINFOEX);
            raw.dwFlags = JOY_RETURNBUTTONS;
            DWORD btns  = 0;
            if (joyGetPosEx(joyPort, &raw) == JOYERR_NOERROR)
                btns = raw.dwButtons;

            if (lightningBotBit > 0) {
                bool pressed = (btns & (1u << (lightningBotBit - 1))) != 0;
                if (pressed && !botBtnPrev) {
                    bot.toggle();
                    printf("\n[BOT] Lightning bot %s\n", bot.isActive() ? "ON" : "OFF");
                }
                botBtnPrev = pressed;
            }

            for (auto& [bit, macro] : macros) {
                bool pressed = (btns & (1u << (bit - 1))) != 0;
                bool& prev   = macroPrevBtn[bit];

                if (pressed && !prev) {
                    if (macro.getMode() == MacroRepeatMode::UntilRelease)
                        macro.start();
                    else
                        macro.toggle();
                    if (macro.isActive()) {
                        macroRotCount[bit] = 0;
                        macroLastRX[bit]   = 0.0f;
                        macroLastRY[bit]   = 0.0f;
                    }
                    printf("\n[MACRO][%llu] '%s' %s\n",
                           GetTickCount64(), macroNames[bit].c_str(),
                           macro.isActive() ? "ON" : "OFF");
                }
                if (!pressed && prev)
                    if (macro.getMode() == MacroRepeatMode::UntilRelease)
                        macro.stop();
                prev = pressed;
            }
        }

        if (input.read(state)) {
            if (state.btnA && !btnAPrev)
                printf("\n[MAN][%llu] Manual A press\n", GetTickCount64());
            btnAPrev = state.btnA;

            bool botPressed = bot.consumePressA();
            if (botPressed) state.btnA = true;

            for (auto& [bit, macro] : macros) {
                bool wasActive = macro.isActive();
                macro.tick(state);

                if (macro.isActive()
                    && (state.rightX != macroLastRX[bit] || state.rightY != macroLastRY[bit])) {
                    bool atNorth    = (fabsf(state.rightX) < 0.1f && state.rightY > 0.9f);
                    bool wasAtNorth = (fabsf(macroLastRX[bit]) < 0.1f && macroLastRY[bit] > 0.9f);
                    if (atNorth && !wasAtNorth) {
                        macroRotCount[bit]++;
                        printf("[MACRO][%llu] '%s' lap=%d\n",
                               GetTickCount64(), macroNames[bit].c_str(), macroRotCount[bit]);
                    }
                    macroLastRX[bit] = state.rightX;
                    macroLastRY[bit] = state.rightY;
                }

                if (wasActive && !macro.isActive())
                    printf("\n[MACRO][%llu] '%s' AUTO-OFF (laps: %d)\n",
                           GetTickCount64(), macroNames[bit].c_str(), macroRotCount[bit]);
            }

            output.update(state);
        }

        Sleep(8);
    }

    m_connected = false;
    setStatus("Stopped");
    std::cout << "\n[PadEngine] thread stopped.\n";
}
