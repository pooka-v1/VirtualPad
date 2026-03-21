#include "EightBitDoInputSource.h"
#include <algorithm>

#pragma comment(lib, "WinMM.lib")

// --- Button mapping for 8BitDo Pro 2 in D-mode ---
// Determined experimentally with PadScannerV3.
// JOY_BUTTON1 = bit 0x00000001, JOY_BUTTON2 = 0x00000002, etc.
static constexpr DWORD BTN_B      = JOY_BUTTON1;
static constexpr DWORD BTN_A      = JOY_BUTTON2;
static constexpr DWORD BTN_Y      = JOY_BUTTON4;
static constexpr DWORD BTN_X      = JOY_BUTTON5;
static constexpr DWORD BTN_LB     = JOY_BUTTON7;
static constexpr DWORD BTN_RB     = JOY_BUTTON8;
static constexpr DWORD BTN_SELECT = JOY_BUTTON11;
static constexpr DWORD BTN_START  = JOY_BUTTON12;
static constexpr DWORD BTN_HOME   = JOY_BUTTON13;
static constexpr DWORD BTN_L3     = JOY_BUTTON14;
static constexpr DWORD BTN_R3     = JOY_BUTTON15;

// Axis mapping in D-mode:
//   dwXpos = Left stick X
//   dwYpos = Left stick Y  (inverted: low = up)
//   dwZpos = Right stick X
//   dwRpos = Right stick Y (inverted: low = up)
//   dwUpos = L2 analog
//   dwVpos = R2 analog
//   dwPOV  = D-pad hat switch

EightBitDoInputSource::EightBitDoInputSource(UINT joyId)
    : m_joyId(joyId) {}

UINT EightBitDoInputSource::scan() {
    UINT numDevs = joyGetNumDevs();
    for (UINT id = 0; id < numDevs; ++id) {
        JOYINFOEX info = {};
        info.dwSize  = sizeof(JOYINFOEX);
        info.dwFlags = JOY_RETURNBUTTONS;
        if (joyGetPosEx(id, &info) == JOYERR_NOERROR)
            return id;
    }
    return UINT_MAX;
}

bool EightBitDoInputSource::isConnected() const {
    JOYINFOEX info;
    info.dwSize  = sizeof(JOYINFOEX);
    info.dwFlags = JOY_RETURNBUTTONS;
    return joyGetPosEx(m_joyId, &info) == JOYERR_NOERROR;
}

bool EightBitDoInputSource::read(GamepadState& state) {
    JOYINFOEX info;
    info.dwSize  = sizeof(JOYINFOEX);
    info.dwFlags = JOY_RETURNALL;  // Request all axes, buttons and POV

    if (joyGetPosEx(m_joyId, &info) != JOYERR_NOERROR)
        return false;

    // --- Face buttons ---
    state.btnA = (info.dwButtons & BTN_A) != 0;
    state.btnB = (info.dwButtons & BTN_B) != 0;
    state.btnX = (info.dwButtons & BTN_X) != 0;
    state.btnY = (info.dwButtons & BTN_Y) != 0;

    // --- Shoulder buttons ---
    state.btnLB = (info.dwButtons & BTN_LB) != 0;
    state.btnRB = (info.dwButtons & BTN_RB) != 0;

    // --- Analog triggers ---
    // In D-mode the 8BitDo Pro 2 exposes L2/R2 on the U and V axes.
    state.triggerL = normalizeTrigger(info.dwUpos);
    state.triggerR = normalizeTrigger(info.dwVpos);

    // --- Menu buttons ---
    state.btnBack  = (info.dwButtons & BTN_SELECT) != 0;
    state.btnStart = (info.dwButtons & BTN_START)  != 0;
    state.btnHome  = (info.dwButtons & BTN_HOME)   != 0;

    // --- Stick clicks ---
    state.btnL3 = (info.dwButtons & BTN_L3) != 0;
    state.btnR3 = (info.dwButtons & BTN_R3) != 0;

    // --- D-Pad ---
    parsePOV(info.dwPOV, state.dpadUp, state.dpadDown, state.dpadLeft, state.dpadRight);

    // --- Sticks ---
    // The X axis is natural: low = left, high = right.
    // The Y axis is inverted in WinMM: low = up, high = down.
    // We negate Y so that +1.0 means "up", matching Xbox convention.
    state.leftX  =  normalizeAxis(info.dwXpos);
    state.leftY  = -normalizeAxis(info.dwYpos);  // inverted
    state.rightX =  normalizeAxis(info.dwZpos);
    state.rightY = -normalizeAxis(info.dwRpos);  // inverted

    return true;
}

float EightBitDoInputSource::normalizeAxis(DWORD value) {
    // Input:  0 to 65535 (center ≈ 32767.5)
    // Output: -1.0 to +1.0
    float normalized = (static_cast<float>(value) - 32767.5f) / 32767.5f;
    return std::clamp(normalized, -1.0f, 1.0f);
}

float EightBitDoInputSource::normalizeTrigger(DWORD value) {
    // Input:  0 to 65535
    // Output: 0.0 to 1.0
    float normalized = static_cast<float>(value) / 65535.0f;
    return std::clamp(normalized, 0.0f, 1.0f);
}

void EightBitDoInputSource::parsePOV(DWORD pov, bool& up, bool& down, bool& left, bool& right) {
    // POV angles are in hundredths of a degree, clockwise from North:
    //   0     = N (up)
    //   4500  = NE  →  up + right
    //   9000  = E (right)
    //   13500 = SE  →  right + down
    //   18000 = S (down)
    //   22500 = SW  →  down + left
    //   27000 = W (left)
    //   31500 = NW  →  left + up
    //   65535 = centered (JOY_POVCENTERED)
    //
    // We use inclusive bounds on both sides of each range so that diagonal
    // angles activate exactly two directions.

    if (pov == JOY_POVCENTERED) {
        up = down = left = right = false;
        return;
    }

    up    = (pov >= 31500 || pov <= 4500);
    right = (pov >= 4500  && pov <= 13500);
    down  = (pov >= 13500 && pov <= 22500);
    left  = (pov >= 22500 && pov <= 31500);
}
