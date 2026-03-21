#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <vector>

#include "EightBitDoInputSource.h"
#include "ViGEmOutputAdapter.h"

#pragma comment(lib, "ViGEmClient.lib")
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

struct JoyEntry {
    UINT    id;
    UINT    axes;
    UINT    buttons;
    wchar_t name[MAXPNAMELEN];
};

// Returns all WinMM ports that respond to joyGetPosEx.
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
            wcsncpy_s(e.name, caps.szPname, MAXPNAMELEN);
        } else {
            wcscpy_s(e.name, L"(unknown)");
        }
        result.push_back(e);
    }
    return result;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "=== VirtualPad ===\n";
    std::cout << "Press ESC to exit.\n\n";

    // --- Step 1: Find the real controller BEFORE creating the virtual one.
    // ViGEm's virtual Xbox 360 competes with the real controller for WinMM slots;
    // scanning first lets the real device claim its port before ViGEm occupies one.
    std::cout << "Scanning for joystick devices...\n";

    UINT joyPort = UINT_MAX;
    while (joyPort == UINT_MAX) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) return 0;

        auto entries = scanPorts();

        if (entries.empty()) {
            std::cout << "\rNo joystick found. Connect the Pro 2 in D-mode.    ";
            Sleep(500);
            continue;
        }

        if (entries.size() == 1) {
            joyPort = entries[0].id;
            printf("\nAuto-selected port %u: %ls (%u axes, %u buttons)\n",
                joyPort, entries[0].name, entries[0].axes, entries[0].buttons);
        } else {
            // Multiple devices found: show list and let the user pick.
            std::cout << "\nMultiple joystick devices found:\n";
            for (auto& e : entries)
                printf("  Port %u: %ls (%u axes, %u buttons)\n",
                    e.id, e.name, e.axes, e.buttons);
            std::cout << "Enter port number of the 8BitDo Pro 2: ";
            std::cin >> joyPort;
        }
    }

    EightBitDoInputSource input(joyPort);

    // --- Step 2: Initialize ViGEm (virtual Xbox 360) after the real port is secured. ---
    ViGEmOutputAdapter output;
    if (!output.isReady()) {
        std::cerr << "Aborting: could not create virtual pad.\n";
        return 1;
    }

    std::cout << "Forwarding input. Press ESC to exit.\n\n";

    // --- Main loop ---
    GamepadState state;

    while (true) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            break;

        if (!input.isConnected()) {
            std::cout << "\r[8BitDo Pro 2 (D-mode)] disconnected. Waiting...    ";
            Sleep(500);
            continue;
        }

        if (input.read(state)) {
            output.update(state);
            // Debug: print what is being read from the controller.
            printf("\r  A:%d B:%d X:%d Y:%d LB:%d RB:%d Bk:%d St:%d | LX:%+.2f LY:%+.2f | RX:%+.2f RY:%+.2f | L2:%.2f R2:%.2f",
                state.btnA, state.btnB, state.btnX, state.btnY,
                state.btnLB, state.btnRB, state.btnBack, state.btnStart,
                state.leftX, state.leftY,
                state.rightX, state.rightY,
                state.triggerL, state.triggerR);
        }

        Sleep(8);  // ~120 Hz polling rate
    }

    std::cout << "\nGoodbye.\n";
    return 0;
}
