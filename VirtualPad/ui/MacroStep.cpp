#define NOMINMAX
#include "MacroStep.h"
#include <cstdio>
#include <cctype>

// ---------------------------------------------------------------------------
// Token tables
// ---------------------------------------------------------------------------
const TokenDef kButtons[] = {
    {"PressA",      "A",  "A"},
    {"PressB",      "B",  "B"},
    {"PressX",      "X",  "X"},
    {"PressY",      "Y",  "Y"},
    {"PressL1",     "L1", "LB"},
    {"PressR1",     "R1", "RB"},
    {"PressL2",     "L2", "LT"},
    {"PressR2",     "R2", "RT"},
    {"PressL3",     "L3", "LS"},
    {"PressR3",     "R3", "RS"},
    {"PressStart",  "ST", "Start"},
    {"PressSelect", "SE", "Select"},
    {"PressHome",   "HO", "Home"},
};

const TokenDef kDpad[] = {
    {"CrossMoveUp",        "CU",  "Up"},
    {"CrossMoveUpRight",   "CUR", "Up-Right"},
    {"CrossMoveRight",     "CR",  "Right"},
    {"CrossMoveDownRight", "CDR", "Down-Right"},
    {"CrossMoveDown",      "CD",  "Down"},
    {"CrossMoveDownLeft",  "CDL", "Down-Left"},
    {"CrossMoveLeft",      "CL",  "Left"},
    {"CrossMoveUpLeft",    "CUL", "Up-Left"},
};

const AnalogDirDef kAnalog[] = {
    {"AnalogicMoveUp",        0.0f,   1.0f,  "Up"},
    {"AnalogicMoveUpRight",   0.71f,  0.71f, "Up-Right"},
    {"AnalogicMoveRight",     1.0f,   0.0f,  "Right"},
    {"AnalogicMoveDownRight",  0.71f, -0.71f, "Down-Right"},
    {"AnalogicMoveDown",       0.0f,  -1.0f,  "Down"},
    {"AnalogicMoveDownLeft",  -0.71f, -0.71f, "Down-Left"},
    {"AnalogicMoveLeft",      -1.0f,   0.0f,  "Left"},
    {"AnalogicMoveUpLeft",    -0.71f,  0.71f, "Up-Left"},
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string fmtFloat(float v) {
    if (v ==  0.0f) return "0";
    if (v ==  1.0f) return "1";
    if (v == -1.0f) return "-1";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.2g", v);
    return buf;
}

std::string analogToken(bool rightStick, float x, float y) {
    const char* prefix = rightStick ? "RA" : "LA";
    return std::string(prefix) + "X" + fmtFloat(x)
         + "+" + prefix + "Y" + fmtFloat(y);
}

std::string tokenToImageName(const std::string& token) {
    for (int i = 0; i < kButtonsCount; ++i)
        if (token == kButtons[i].token) return kButtons[i].img;
    for (int i = 0; i < kDpadCount; ++i)
        if (token == kDpad[i].token) return kDpad[i].img;
    for (int i = 0; i < kAnalogCount; ++i) {
        if (token == analogToken(false, kAnalog[i].x, kAnalog[i].y)) return kAnalog[i].img;
        if (token == analogToken(true,  kAnalog[i].x, kAnalog[i].y)) return kAnalog[i].img;
    }
    return {};
}

// ---------------------------------------------------------------------------
// MacroStepItem::toDsl
// ---------------------------------------------------------------------------
std::string MacroStepItem::toDsl() const {
    if (kind == Kind::Wait)
        return std::to_string(waitMs);

    std::string result;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) result += "+";
        result += tokens[i];
    }
    if (holdMs > 0)
        result += "=" + std::to_string(holdMs);
    return result;
}

// ---------------------------------------------------------------------------
// tryParseSimpleDsl
// ---------------------------------------------------------------------------
std::vector<MacroStepItem> tryParseSimpleDsl(const std::string& rawDsl) {
    std::vector<MacroStepItem> result;
    if (rawDsl.empty()) return result;

    // Split by top-level commas (skip content inside parentheses)
    std::vector<std::string> parts;
    int depth = 0;
    std::string cur;
    for (char c : rawDsl) {
        if      (c == '(') ++depth;
        else if (c == ')') --depth;
        if (c == ',' && depth == 0) { if (!cur.empty()) parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);

    for (auto& part : parts) {
        size_t s = part.find_first_not_of(" \t");
        size_t e = part.find_last_not_of(" \t");
        if (s == std::string::npos) continue;
        part = part.substr(s, e - s + 1);

        // Complex element (group/repeat) -> bail
        if (part.find('*') != std::string::npos || part.find('(') != std::string::npos)
            return {};

        // Pure number -> Wait step
        bool isNum = !part.empty();
        for (char c : part) if (!std::isdigit((unsigned char)c)) { isNum = false; break; }
        if (isNum) {
            MacroStepItem step;
            step.kind   = MacroStepItem::Kind::Wait;
            step.waitMs = std::stoi(part);
            result.push_back(step);
            continue;
        }

        // Press step: optional "=N" suffix for hold time
        MacroStepItem step;
        step.kind = MacroStepItem::Kind::Press;
        std::string combo = part;
        size_t eqPos = part.rfind('=');
        if (eqPos != std::string::npos) {
            std::string after = part.substr(eqPos + 1);
            bool allDig = !after.empty();
            for (char c : after) if (!std::isdigit((unsigned char)c)) { allDig = false; break; }
            if (allDig) { step.holdMs = std::stoi(after); combo = part.substr(0, eqPos); }
        }

        // Split by '+', trim whitespace from each token, then merge LA*/RA* axis pairs
        auto trimmed = [](const std::string& s) -> std::string {
            size_t a = s.find_first_not_of(" \t");
            size_t b = s.find_last_not_of(" \t");
            return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
        };
        std::vector<std::string> toks;
        std::string tok;
        for (char c : combo) {
            if (c == '+') { auto t = trimmed(tok); if (!t.empty()) toks.push_back(t); tok.clear(); }
            else tok += c;
        }
        { auto t = trimmed(tok); if (!t.empty()) toks.push_back(t); }

        std::vector<std::string> merged;
        for (int i = 0; i < (int)toks.size(); ++i) {
            const std::string& t = toks[i];
            bool isAxis = t.size() >= 3 && (t[0] == 'L' || t[0] == 'R') && t[1] == 'A'
                          && (t[2] == 'X' || t[2] == 'Y');
            if (isAxis && !merged.empty()) {
                const std::string& prev = merged.back();
                bool prevAxis = prev.size() >= 3 && prev[0] == t[0] && prev[1] == 'A'
                                && (prev[2] == 'X' || prev[2] == 'Y') && prev[2] != t[2];
                if (prevAxis) { merged.back() += "+" + t; continue; }
            }
            merged.push_back(t);
        }
        step.tokens = merged;
        if (!step.tokens.empty()) result.push_back(step);
    }
    return result;
}
