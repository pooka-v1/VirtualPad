#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

enum class ButtonActionType  { VirtualButton, Trigger, TriggerPassthrough, Bot, Macro, Keyboard, MouseClick };

// Returns true if s is a stick half-axis slot direction (e.g. "right_x_neg").
inline bool isStickSlotDir(const std::string& s) {
    return s == "left_x_pos"  || s == "left_x_neg"  ||
           s == "left_y_pos"  || s == "left_y_neg"  ||
           s == "right_x_pos" || s == "right_x_neg" ||
           s == "right_y_pos" || s == "right_y_neg";
}
enum class HalfAxisActionType { Analog, VirtualButton, Dpad, Macro, Keyboard, Mouse };

struct ButtonAction {
    ButtonActionType     type      = ButtonActionType::VirtualButton;
    std::string          name;        // virtual button ("a","b",...), bot/macro name
    std::string          physical;    // nombre del botón en el mando físico ("a","l4","rp"...)
                                     // independiente de la acción — nunca sobreescrito por perfiles
    std::string          axis;        // trigger only: WinMM source ("dwUpos", "dwVpos")
    std::string          target;      // trigger only: "l2" or "r2"
    std::string          execution;   // macro only: compact execution string
    std::vector<std::string> keys;    // keyboard only: e.g. ["alt","tab"]
    std::string          mouseButton; // mouse_click only: "left","right","middle"
};

// One action bound to a single half-axis direction ("source_pos" / "source_neg").
// The key stored in ControllerConfig::axis_actions is "<axis_source>_pos" or "<axis_source>_neg".
struct HalfAxisAction {
    HalfAxisActionType   type        = HalfAxisActionType::Analog;
    // Analog:        target stick axis  "left_x"|"left_y"|"right_x"|"right_y"
    // VirtualButton: button name        "a"|"b"|"x"|"y"|"l1"|"r1"|"select"|"start"|"home"|"l3"|"r3"|"l4"|"r4"|"lp"|"rp"
    // Dpad:          direction          "up"|"down"|"left"|"right"
    // Macro:         macro name         (target field)
    // Keyboard:      unused             (keys field holds combo)
    // Mouse:         unused             (mouseButton field)
    std::string          target;
    std::string          outDir;     // Analog only: "pos"|"neg" — which virtual half to drive
    float                threshold  = 0.5f;   // digital targets: activation threshold
    float                scale      = 1.0f;   // Analog: output multiplier (1.0 = proportional 1:1)
    std::vector<std::string> keys;             // Keyboard only
    std::string          mouseButton;          // Mouse only: "left"|"right"|"middle"
    std::string          execution;            // Macro only: optional compact execution string
};

struct AxisMapping {
    std::string target;
    bool        invert    = false;
    float       speed     = 15.0f;    // mouse_x/mouse_y only: pixels per tick at full deflection
    std::string stickId;              // permanent physical axis ID: "left_x"|"left_y"|"right_x"|"right_y"
    std::string btnNeg;               // btn_dir: virtual button when v < -threshold  (e.g. "l1")
    std::string btnPos;               // btn_dir: virtual button when v > +threshold  (e.g. "r1")
    float       threshold = 0.5f;     // dpad_x/dpad_y/btn_dir activation threshold
};

struct TouchpadConfig {
    bool enabled      = false;
    int  dataOffset   = 34;    // byte index of finger-1 data in raw HID report (DS4 USB: 34)
    int  maxX         = 1919;  // DS4 touchpad horizontal resolution
    int  maxY         = 942;   // DS4 touchpad vertical resolution
    bool mouseEnabled = false; // route surface movement → mouse (delta-based)
};

struct ImuConfig {
    bool  enabled     = false;
    int   gyroOffset  = 13;    // byte index of gyro X in raw HID report (DS4 USB: 13)
    float gyroScale   = 1.0f / 32768.0f;  // int16 raw → normalized [-1..1]
};

// One action bound to a specific range of a physical trigger's value.
struct TriggerRange {
    float        from      = 0.0f;  // inclusive lower bound [0, 1]
    float        to        = 1.0f;  // inclusive upper bound [0, 1]
    ButtonAction action;
    bool         hasAction = false; // true only if an action was explicitly set
};

struct ControllerConfig {
    uint16_t    vid = 0;
    uint16_t    pid = 0;
    std::string source_name;
    std::string mode;
    std::string connection;    // "usb" / "bt" / "" = match any

    std::unordered_map<int, ButtonAction>           buttons;       // physical bit (1-indexed) -> action
    std::unordered_map<std::string, AxisMapping>    axes;          // WinMM/HID source name -> whole-axis mapping
    std::unordered_map<std::string, HalfAxisAction> axis_actions;  // "source_pos"/"source_neg" -> per-direction action
    std::unordered_map<std::string, std::string>    dpadRemap;     // "up"/"down"/"left"/"right" -> virtual short name
    std::unordered_map<std::string, ButtonAction>   dpadActions;   // "up"/"down"/"left"/"right" -> keyboard/mouse/macro action
    std::string    dpad;
    std::string    layout_id;  // references an entry in data/pad_layouts.json; empty = use defaults
    TouchpadConfig touchpad;
    ImuConfig      imu;

    // Physical trigger → action mapping (physical trigger as source)
    ButtonAction   triggerLAction;
    ButtonAction   triggerRAction;
    bool           triggerLHasAction = false;
    bool           triggerRHasAction = false;
    // Physical trigger → ranged actions (if non-empty, overrides the simple action above)
    std::vector<TriggerRange> triggerLRanges;
    std::vector<TriggerRange> triggerRRanges;

    // Stick slot assignments: slot key → source name.
    // Slot keys: "left_x_pos", "left_x_neg", "left_y_pos", "left_y_neg",
    //            "right_x_pos", "right_x_neg", "right_y_pos", "right_y_neg".
    // Sources: physShort ("a","b",...), "dpad_up/down/left/right", "l2", "r2".
    // Undefined slots fall through to the axes mapping for that axis.
    // Trigger sources without ranges: analog (0..1). With ranges: digital (0 or 1).
    std::unordered_map<std::string, std::vector<std::string>> stickSlots; // slot → [sources] (OR: any active drives the slot)
};
