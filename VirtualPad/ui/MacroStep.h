#pragma once
#include <string>
#include <vector>

struct TokenDef {
    const char* img;
    const char* token;
    const char* tip;
};

struct AnalogDirDef {
    const char* img;
    float       x, y;
    const char* tip;
};

extern const TokenDef     kButtons[];
extern const TokenDef     kDpad[];
extern const AnalogDirDef kAnalog[];

constexpr int kButtonsCount = 13;
constexpr int kDpadCount    = 8;
constexpr int kAnalogCount  = 8;

std::string analogToken(bool rightStick, float x, float y);
std::string tokenToImageName(const std::string& token);

struct MacroStepItem {
    enum class Kind { Press, Wait };
    Kind kind = Kind::Press;

    // Press: DSL token strings, e.g. {"A","CD"} -> "A+CD"
    // Analog compound tokens stored as a single string, e.g. "LAX0.71+LAY0.71"
    std::vector<std::string> tokens;
    int holdMs = 0;   // 0 = default (parser uses DEFAULT_PRESS_MS)

    // Wait
    int waitMs = 200;

    std::string toDsl() const;
};

// Returns empty vector if the DSL contains complex elements (groups, repeat specs).
std::vector<MacroStepItem> tryParseSimpleDsl(const std::string& rawDsl);
