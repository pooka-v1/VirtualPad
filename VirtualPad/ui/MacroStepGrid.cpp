#include "MacroStepGrid.h"
#include <cstdio>

MacroStepGrid::MacroStepGrid(const std::unordered_map<std::string, PadTexture>& icons)
    : m_icons(icons) {}

ImTextureID MacroStepGrid::iconSrv(const std::string& name) const {
    auto it = m_icons.find(name);
    if (it != m_icons.end() && it->second.valid())
        return (ImTextureID)(uintptr_t)it->second.srv;
    return (ImTextureID)0;
}

bool MacroStepGrid::render(std::vector<MacroStepItem>& steps, bool& dslOnly, int& activeStep) {
    constexpr float kIconSz  = 24.0f;
    constexpr float kIconGap = 3.0f;
    constexpr float kSepW    = 18.0f;

    if (dslOnly) {
        ImGui::SeparatorText("Sequence");
        float rowH = kIconSz + 2.0f * ImGui::GetStyle().FramePadding.y;
        float w    = ImGui::GetContentRegionAvail().x;
        ImVec2 p   = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(w, rowH));
        const char* txt = "(complex macro \xe2\x80\x94 appends to DSL)";
        ImVec2 ts = ImGui::CalcTextSize(txt);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (rowH - ts.y) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_TextDisabled), txt);
        return false;
    }
    ImGui::SeparatorText("Sequence");

    if (steps.empty()) {
        float rowH = kIconSz + 2.0f * ImGui::GetStyle().FramePadding.y;
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, rowH));
        return false;
    }
    float rowLimit = ImGui::GetContentRegionAvail().x - 4.0f;
    float curX     = 0.0f;
    int   clicked  = -1;

    for (int i = 0; i < (int)steps.size(); ++i) {
        ImGui::PushID(i);
        const MacroStepItem& step = steps[i];
        bool isActive = (i == activeStep);

        float stepW = (step.kind == MacroStepItem::Kind::Wait)
            ? (kIconSz + 40.0f)
            : ((float)step.tokens.size() * (kIconSz + kIconGap)
               + (step.holdMs > 0 ? 30.0f : 0.0f));

        if (i > 0) {
            float needed = kSepW + stepW;
            if (curX + needed > rowLimit) { ImGui::NewLine(); curX = 0.0f; }
            else {
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::TextDisabled("\xe2\x86\x92");   // UTF-8 →
                ImGui::SameLine(0.0f, 4.0f);
                curX += kSepW;
            }
        }

        // Active step gets a blue button tint
        ImVec4 btnColor = isActive
            ? ImVec4(0.20f, 0.45f, 0.75f, 1.0f)
            : ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

        bool stepClicked = false;
        ImGui::BeginGroup();

        if (step.kind == MacroStepItem::Kind::Wait) {
            auto srv = iconSrv("Wait");
            ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
            if (srv) {
                stepClicked = ImGui::ImageButton("##w", srv, ImVec2(kIconSz, kIconSz));
            } else {
                stepClicked = ImGui::Button("W##w", ImVec2(0, kIconSz + 4));
            }
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, kIconGap);
            char buf[16]; std::snprintf(buf, sizeof(buf), "%dms", step.waitMs);
            ImGui::TextDisabled("%s", buf);
        } else {
            for (int t = 0; t < (int)step.tokens.size(); ++t) {
                if (t > 0) ImGui::SameLine(0.0f, kIconGap);
                const std::string& tok = step.tokens[t];
                std::string img = tokenToImageName(tok);
                auto srv = img.empty() ? (ImTextureID)0 : iconSrv(img);
                ImGui::PushID(t);
                ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
                if (srv) {
                    if (ImGui::ImageButton("##t", srv, ImVec2(kIconSz, kIconSz)))
                        stepClicked = true;
                } else {
                    if (ImGui::Button(tok.c_str(), ImVec2(0, kIconSz + 4)))
                        stepClicked = true;
                }
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Step %d: %s", i + 1, step.toDsl().c_str());
                ImGui::PopID();
            }
            if (step.holdMs > 0) {
                ImGui::SameLine(0.0f, kIconGap);
                char buf[12]; std::snprintf(buf, sizeof(buf), "=%d", step.holdMs);
                ImGui::TextDisabled("%s", buf);
            }
        }

        ImGui::EndGroup();

        if (step.kind == MacroStepItem::Kind::Wait && ImGui::IsItemHovered())
            ImGui::SetTooltip("Step %d: %s", i + 1, step.toDsl().c_str());

        if (stepClicked) clicked = i;
        curX += stepW;
        ImGui::PopID();
    }

    // Toggle active step on click
    if (clicked >= 0)
        activeStep = (activeStep == clicked) ? -1 : clicked;

    return false;
}
