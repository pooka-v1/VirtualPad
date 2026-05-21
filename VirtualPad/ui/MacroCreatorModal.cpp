#define NOMINMAX
#include "MacroCreatorModal.h"
#include "../config/Strings.h"
#include "../macros/Macro.h"
#include "../macros/MacroParser.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <stdexcept>

// ---------------------------------------------------------------------------
// init / loadIcons
// ---------------------------------------------------------------------------

static const char* kAllImages[] = {
    "PressA","PressB","PressX","PressY",
    "PressL1","PressR1","PressL2","PressR2",
    "PressL3","PressR3","PressStart","PressSelect","PressHome",
    "CrossMoveUp","CrossMoveUpRight","CrossMoveRight","CrossMoveDownRight",
    "CrossMoveDown","CrossMoveDownLeft","CrossMoveLeft","CrossMoveUpLeft",
    "CrossSpinLeft","CrossSpinRight",
    "AnalogicMoveUp","AnalogicMoveUpRight","AnalogicMoveRight","AnalogicMoveDownRight",
    "AnalogicMoveDown","AnalogicMoveDownLeft","AnalogicMoveLeft","AnalogicMoveUpLeft",
    "AnalogicSpinLeft","AnalogicSpinRight",
    "Wait",
};

void MacroCreatorModal::init(ID3D11Device* device) {
    if (m_initialized) return;
    loadIcons(device);
    m_initialized = true;
}

void MacroCreatorModal::loadIcons(ID3D11Device* device) {
    for (const char* name : kAllImages) {
        std::string path = std::string("images/input_tokens/") + name + ".png";
        PadTexture tex;
        if (PadView::loadPng(device, path.c_str(), tex))
            m_icons[name] = std::move(tex);
    }
}

// ---------------------------------------------------------------------------
// open
// ---------------------------------------------------------------------------
void MacroCreatorModal::open(Mode mode, const std::string& name, const std::string& execution) {
    m_mode        = mode;
    m_waitMsBuf  = 200;
    m_validOk    = false;
    m_validMsg    = {};
    m_activeStep  = -1;

    strncpy_s(m_nameBuffer, name.c_str(),      sizeof(m_nameBuffer) - 1);
    strncpy_s(m_dslBuffer,  execution.c_str(), sizeof(m_dslBuffer)  - 1);

    m_steps   = tryParseSimpleDsl(execution);
    m_dslOnly = !execution.empty() && m_steps.empty();

    validate();
    m_open = true;
}

// ---------------------------------------------------------------------------
// DSL sync & validation
// ---------------------------------------------------------------------------
void MacroCreatorModal::syncDslFromSteps() {
    std::string dsl;
    for (size_t i = 0; i < m_steps.size(); ++i) {
        if (i > 0) dsl += ", ";
        dsl += m_steps[i].toDsl();
    }
    strncpy_s(m_dslBuffer, dsl.c_str(), sizeof(m_dslBuffer) - 1);
    validate();
}

void MacroCreatorModal::validate() {
    if (m_dslBuffer[0] == '\0') {
        m_validOk  = false;
        m_validMsg = tr("macros.err_empty");
        return;
    }
    try {
        Macro tmp;
        MacroParser::parse(std::string(m_dslBuffer), tmp);
        m_validOk  = true;
        m_validMsg = tr("macros.valid");
    } catch (const std::exception& e) {
        m_validOk  = false;
        m_validMsg = std::string(tr("macros.invalid")) + " " + e.what();
    }
}

// ---------------------------------------------------------------------------
// onTokenClick — creates a new Press step or toggles token in the active step
// ---------------------------------------------------------------------------
void MacroCreatorModal::onTokenClick(const std::string& token) {
    if (m_dslOnly) {
        // Complex DSL: append a one-token Press step directly
        MacroStepItem item;
        item.kind   = MacroStepItem::Kind::Press;
        item.tokens = { token };
        std::string cur(m_dslBuffer);
        if (!cur.empty()) cur += ", ";
        cur += item.toDsl();
        strncpy_s(m_dslBuffer, cur.c_str(), sizeof(m_dslBuffer) - 1);
        validate();
        return;
    }

    bool hasActivePress = m_activeStep >= 0
                         && m_activeStep < (int)m_steps.size()
                         && m_steps[m_activeStep].kind == MacroStepItem::Kind::Press;

    if (!hasActivePress) {
        // Create a new Press step at the end — no focus by default
        MacroStepItem item;
        item.kind   = MacroStepItem::Kind::Press;
        item.tokens = { token };
        m_steps.push_back(std::move(item));
    } else {
        // Toggle token in the active Press step
        auto& tokens = m_steps[m_activeStep].tokens;
        auto it = std::find(tokens.begin(), tokens.end(), token);
        if (it != tokens.end()) {
            tokens.erase(it);
            if (tokens.empty()) {
                // Removing the last token deletes the step
                m_steps.erase(m_steps.begin() + m_activeStep);
                m_activeStep = -1;
            }
        } else {
            tokens.push_back(token);
        }
    }
    syncDslFromSteps();
}

bool MacroCreatorModal::isTokenInActiveStep(const std::string& token) const {
    if (m_activeStep < 0 || m_activeStep >= (int)m_steps.size()) return false;
    const auto& step = m_steps[m_activeStep];
    if (step.kind != MacroStepItem::Kind::Press) return false;
    return std::find(step.tokens.begin(), step.tokens.end(), token) != step.tokens.end();
}

// ---------------------------------------------------------------------------
// addSpinSteps — inserts 8 directional Press steps in circular order
// ---------------------------------------------------------------------------
void MacroCreatorModal::addSpinSteps(bool clockwise, bool analog, bool rightStick) {
    constexpr int kSpinMs   = 30;
    constexpr int kRightIdx = 2;  // CR / AnalogicMoveRight in both arrays
    constexpr int kLeftIdx  = 6;  // CL / AnalogicMoveLeft  in both arrays

    int total    = analog ? kAnalogCount : kDpadCount;
    int startIdx = clockwise ? kRightIdx : kLeftIdx;

    std::vector<MacroStepItem> spin;
    spin.reserve(total);
    for (int i = 0; i < total; ++i) {
        int idx = clockwise
            ? (startIdx + i) % total
            : (startIdx - i + total) % total;
        MacroStepItem step;
        step.kind   = MacroStepItem::Kind::Press;
        step.holdMs = kSpinMs;
        step.tokens = { analog ? analogToken(rightStick, kAnalog[idx].x, kAnalog[idx].y)
                               : std::string(kDpad[idx].token) };
        spin.push_back(std::move(step));
    }

    if (m_dslOnly) {
        std::string cur(m_dslBuffer);
        for (auto& s : spin) { if (!cur.empty()) cur += ", "; cur += s.toDsl(); }
        strncpy_s(m_dslBuffer, cur.c_str(), sizeof(m_dslBuffer) - 1);
        validate();
    } else {
        for (auto& s : spin) m_steps.push_back(std::move(s));
        m_activeStep = -1;
        syncDslFromSteps();
    }
}

// ---------------------------------------------------------------------------
// addWaitStep
// ---------------------------------------------------------------------------
void MacroCreatorModal::addWaitStep() {
    MacroStepItem item;
    item.kind   = MacroStepItem::Kind::Wait;
    item.waitMs = m_waitMsBuf;

    if (m_dslOnly) {
        std::string cur(m_dslBuffer);
        if (!cur.empty()) cur += ", ";
        cur += item.toDsl();
        strncpy_s(m_dslBuffer, cur.c_str(), sizeof(m_dslBuffer) - 1);
        validate();
    } else {
        m_steps.push_back(std::move(item));
        m_activeStep = (int)m_steps.size() - 1;
        syncDslFromSteps();
    }
}

// ---------------------------------------------------------------------------
// Icon helpers
// ---------------------------------------------------------------------------
ImTextureID MacroCreatorModal::iconSrv(const std::string& name) const {
    auto it = m_icons.find(name);
    if (it != m_icons.end() && it->second.valid())
        return (ImTextureID)(uintptr_t)it->second.srv;
    return (ImTextureID)0;
}

bool MacroCreatorModal::renderIconToggle(const std::string& imgName,
                                          const std::string& token,
                                          const char*        tooltip) {
    constexpr float kImgSz = 24.0f;

    bool selected = isTokenInActiveStep(token);
    ImVec4 bgColor = selected
        ? ImVec4(0.25f, 0.55f, 1.0f, 1.0f)
        : ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

    bool clicked = false;
    ImGui::PushID((imgName + "_" + token).c_str());
    ImGui::PushStyleColor(ImGuiCol_Button, bgColor);

    auto srv = iconSrv(imgName);
    if (srv) {
        clicked = ImGui::ImageButton("##icon", srv, ImVec2(kImgSz, kImgSz));
    } else {
        // Match ImageButton total size so the row height never shifts
        const ImVec2& pad = ImGui::GetStyle().FramePadding;
        ImVec2 sz(kImgSz + 2.0f * pad.x, kImgSz + 2.0f * pad.y);
        clicked = ImGui::Button(token.c_str(), sz);
    }

    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered() && tooltip)
        ImGui::SetTooltip("%s", tooltip);

    ImGui::PopID();

    if (clicked)
        onTokenClick(token);

    return clicked;
}

// ---------------------------------------------------------------------------
// renderSpinButton
// ---------------------------------------------------------------------------
void MacroCreatorModal::renderSpinButton(const std::string& imgName,
                                          const char*        tooltip,
                                          bool               clockwise,
                                          bool               analog,
                                          bool               rightStick) {
    constexpr float kImgSz = 24.0f;
    ImGui::PushID((imgName + (rightStick ? "_r" : "_l")).c_str());
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.25f, 1.0f));

    auto srv = iconSrv(imgName);
    bool clicked;
    if (srv) {
        clicked = ImGui::ImageButton("##icon", srv, ImVec2(kImgSz, kImgSz));
    } else {
        const ImVec2& pad = ImGui::GetStyle().FramePadding;
        clicked = ImGui::Button(tooltip, ImVec2(kImgSz + 2.0f * pad.x, kImgSz + 2.0f * pad.y));
    }

    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered() && tooltip) ImGui::SetTooltip("%s", tooltip);
    ImGui::PopID();

    if (clicked) addSpinSteps(clockwise, analog, rightStick);
}

// ---------------------------------------------------------------------------
// renderTokenPicker
// ---------------------------------------------------------------------------
void MacroCreatorModal::renderTokenPicker() {
    ImGui::SeparatorText("Buttons");
    for (int i = 0; i < kButtonsCount; ++i) {
        renderIconToggle(kButtons[i].img, kButtons[i].token, kButtons[i].tip);
        if (i < kButtonsCount - 1) ImGui::SameLine(0.0f, 4.0f);
    }

    ImGui::SeparatorText("D-pad");
    for (int i = 0; i < kDpadCount; ++i) {
        renderIconToggle(kDpad[i].img, kDpad[i].token, kDpad[i].tip);
        ImGui::SameLine(0.0f, 4.0f);
    }
    renderSpinButton("CrossSpinLeft",  "Spin CCW", false, false, false);
    ImGui::SameLine(0.0f, 4.0f);
    renderSpinButton("CrossSpinRight", "Spin CW",  true,  false, false);

    ImGui::SeparatorText("Analog");

    // L stick row
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("L");
    for (int i = 0; i < kAnalogCount; ++i) {
        ImGui::SameLine(0.0f, 3.0f);
        renderIconToggle(kAnalog[i].img, analogToken(false, kAnalog[i].x, kAnalog[i].y), kAnalog[i].tip);
    }
    ImGui::SameLine(0.0f, 6.0f);
    renderSpinButton("AnalogicSpinLeft",  "Spin CCW", false, true, false);
    ImGui::SameLine(0.0f, 3.0f);
    renderSpinButton("AnalogicSpinRight", "Spin CW",  true,  true, false);

    // R stick row
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("R");
    for (int i = 0; i < kAnalogCount; ++i) {
        ImGui::SameLine(0.0f, 3.0f);
        renderIconToggle(kAnalog[i].img, analogToken(true, kAnalog[i].x, kAnalog[i].y), kAnalog[i].tip);
    }
    ImGui::SameLine(0.0f, 6.0f);
    renderSpinButton("AnalogicSpinLeft",  "Spin CCW", false, true, true);
    ImGui::SameLine(0.0f, 3.0f);
    renderSpinButton("AnalogicSpinRight", "Spin CW",  true,  true, true);
}

// ---------------------------------------------------------------------------
// renderActiveStepControls — shown below the step sequence
// ---------------------------------------------------------------------------
void MacroCreatorModal::renderActiveStepControls() {
    ImGui::Spacing();

    bool hasActive      = m_activeStep >= 0 && m_activeStep < (int)m_steps.size();
    bool hasActivePress = hasActive && m_steps[m_activeStep].kind == MacroStepItem::Kind::Press;

    // Hold ms — always visible, disabled when no active Press step
    if (!hasActivePress) ImGui::BeginDisabled();
    int hold = hasActivePress ? m_steps[m_activeStep].holdMs : 0;
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputInt("Hold ms##hold", &hold) && hasActivePress) {
        if (hold < 0) hold = 0;
        m_steps[m_activeStep].holdMs = hold;
        syncDslFromSteps();
    }
    if (!hasActivePress) ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 12.0f);

    // ms input — edits active Wait step if one is selected, otherwise the new-step buffer
    bool hasActiveWait = hasActive && m_steps[m_activeStep].kind == MacroStepItem::Kind::Wait;
    int& waitVal = hasActiveWait ? m_steps[m_activeStep].waitMs : m_waitMsBuf;
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputInt("ms##wms", &waitVal)) {
        if (waitVal < 1) waitVal = 1;
        if (hasActiveWait) syncDslFromSteps();
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (ImGui::Button("Add Wait##addw")) addWaitStep();

    // Delete step — always visible, disabled when no step is active
    ImGui::SameLine(0.0f, 16.0f);
    if (!hasActive) ImGui::BeginDisabled();
    if (hasActive) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.18f, 0.18f, 1.0f));
    }
    if (ImGui::Button("Delete step##del") && hasActive) {
        m_steps.erase(m_steps.begin() + m_activeStep);
        m_activeStep = -1;
        m_dslOnly    = false;
        syncDslFromSteps();
    }
    if (hasActive) ImGui::PopStyleColor(2);
    if (!hasActive) ImGui::EndDisabled();

    // Clear all — always visible, disabled when sequence is empty
    ImGui::SameLine(0.0f, 8.0f);
    bool hasSteps = !m_steps.empty();
    if (!hasSteps) ImGui::BeginDisabled();
    if (ImGui::Button("Clear all##clr") && hasSteps) {
        m_steps.clear();
        m_activeStep = -1;
        m_dslOnly    = false;
        syncDslFromSteps();
    }
    if (!hasSteps) ImGui::EndDisabled();
}

// ---------------------------------------------------------------------------
// renderDslField
// ---------------------------------------------------------------------------
void MacroCreatorModal::renderDslField() {
    ImGui::SeparatorText("DSL");

    float avail = ImGui::GetContentRegionAvail().x;
    if (ImGui::InputTextMultiline("##dsl", m_dslBuffer, sizeof(m_dslBuffer),
                                  ImVec2(avail, 60.0f))) {
        validate();
        auto parsed = tryParseSimpleDsl(std::string(m_dslBuffer));
        if (!parsed.empty() || m_dslBuffer[0] == '\0') {
            m_steps      = parsed;
            m_dslOnly    = false;
            m_activeStep = -1;
        } else {
            m_steps.clear();
            m_dslOnly    = true;
            m_activeStep = -1;
        }
    }

    ImVec4 col = m_validOk
        ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
        : ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextWrapped("%s", m_validMsg.c_str());
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
bool MacroCreatorModal::render() {
    if (!m_open) return false;

    const char* popupId = "##MacroCreatorModal";
    ImGui::OpenPopup(popupId);

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(680.0f, 0.0f));

    if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return false;

    const char* title = (m_mode == Mode::kLibrary) ? "Edit Macro" : "Build Macro";
    ImGui::TextDisabled("%s", title);
    ImGui::Separator();

    if (m_mode == Mode::kLibrary) {
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputText(tr("macros.name_label"), m_nameBuffer, sizeof(m_nameBuffer));
        ImGui::Spacing();
    }

    renderTokenPicker();
    if (m_stepGrid.render(m_steps, m_dslOnly, m_activeStep))
        syncDslFromSteps();
    renderActiveStepControls();
    renderDslField();

    ImGui::Spacing();
    ImGui::Separator();

    bool confirmed = false;
    bool canConfirm = m_validOk &&
                      (m_mode == Mode::kInline || m_nameBuffer[0] != '\0');

    if (!canConfirm) ImGui::BeginDisabled();
    if (ImGui::Button(tr("btn.save"), {120.0f, 0.0f})) {
        confirmed = true;
        m_open    = false;
        ImGui::CloseCurrentPopup();
    }
    if (!canConfirm) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(tr("btn.cancel"), {80.0f, 0.0f})) {
        m_open = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    return confirmed;
}
