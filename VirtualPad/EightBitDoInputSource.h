#pragma once

#include <windows.h>
#include <mmsystem.h>
#include "IInputSource.h"

// Reads the 8BitDo Pro 2 controller in D-mode via the WinMM joystick API.
//
// IMPORTANT: the controller MUST be switched to D-mode before connecting.
// In D-mode it appears as a generic DirectInput device, which WinMM can read
// with joyGetPosEx. In X-mode it would expose itself as an XInput pad instead.
class EightBitDoInputSource : public IInputSource {
public:
    // joyId: JOYSTICKID1 (0) for the first connected joystick, JOYSTICKID2 (1) for the second.
    explicit EightBitDoInputSource(UINT joyId = JOYSTICKID1);

    // Scans all WinMM ports and returns the id of the first responsive device.
    // Returns UINT_MAX if no device is found.
    static UINT scan();

    bool        isConnected() const override;
    bool        read(GamepadState& state) override;
    const char* getName()      const override { return "8BitDo Pro 2 (D-mode)"; }

private:
    UINT m_joyId;

    // Normalizes a WinMM axis value (0-65535, center ~32767) to [-1.0, 1.0].
    static float normalizeAxis(DWORD value);

    // Normalizes a WinMM trigger value (0-65535) to [0.0, 1.0].
    static float normalizeTrigger(DWORD value);

    // Converts a WinMM POV hat angle to individual D-pad booleans.
    // Diagonals (e.g. NE = 4500) set two directions simultaneously.
    static void parsePOV(DWORD pov, bool& up, bool& down, bool& left, bool& right);
};
