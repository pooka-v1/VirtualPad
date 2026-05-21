#include "MacroManagerPanel.h"
#include "../config/ConfigLoader.h"
#include "../config/Strings.h"
#include "../macros/Macro.h"
#include "../macros/MacroParser.h"
#include "../imgui/imgui.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

// ============================================================
//  Lifecycle
// ============================================================

void MacroManagerPanel::init(ID3D11Device* device) {
    m_creator.init(device);
}

void MacroManagerPanel::activate() {
    m_active      = true;
    m_selectedIdx = -1;
    m_commitError.clear();
    load();
}

void MacroManagerPanel::load() {
    auto raw = loadMacroLibrary("data/macros.json");
    m_macros.clear();
    m_macros.reserve(raw.size());
    for (auto& [name, dsl] : raw)
        m_macros.emplace_back(name, dsl);
    std::sort(m_macros.begin(), m_macros.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
}

void MacroManagerPanel::save() {
    saveMacroLibrary("data/macros.json", m_macros);
}

// ============================================================
//  Edit state helpers
// ============================================================

void MacroManagerPanel::selectMacro(int idx) {
    m_selectedIdx = idx;
    m_commitError.clear();
}

void MacroManagerPanel::showToast(const char* key) {
    m_toastMsg  = tr(key);
    m_toastTime = GetTickCount64();
}

void MacroManagerPanel::commitFromModal() {
    std::string newName = m_creator.getName();
    std::string newDsl  = m_creator.getExecution();

    // Normalize DSL to uppercase
    for (char& c : newDsl)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    for (int i = 0; i < (int)m_macros.size(); i++) {
        if (i == m_selectedIdx) continue;
        if (m_macros[i].first == newName) {
            m_commitError = tr("macros.err_name_taken");
            return;
        }
    }
    m_commitError.clear();

    if (m_selectedIdx >= 0)
        m_macros[m_selectedIdx] = { newName, newDsl };
    else
        m_macros.emplace_back(newName, newDsl);

    std::sort(m_macros.begin(), m_macros.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    m_selectedIdx = -1;
    for (int i = 0; i < (int)m_macros.size(); i++) {
        if (m_macros[i].first == newName) { m_selectedIdx = i; break; }
    }

    save();
    m_macrosSaved = true;
    showToast("macros.toast_saved");
}

// ============================================================
//  Render
// ============================================================

void MacroManagerPanel::render() {
    // Modal must be called at the top level, before any child windows
    if (m_creator.render())
        commitFromModal();

    if (ImGui::Button(tr("btn.back"))) {
        m_active = false;
        return;
    }
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Text("%s", tr("macros.title"));

    if (!m_toastMsg.empty()) {
        if (GetTickCount64() - m_toastTime < 2500) {
            ImGui::SameLine(0.0f, 20.0f);
            ImGui::TextColored({ 0.3f, 1.0f, 0.3f, 1.0f }, "%s", m_toastMsg.c_str());
        } else {
            m_toastMsg.clear();
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    renderList();
    ImGui::SameLine(0.0f, 6.0f);
    renderActions();
}

void MacroManagerPanel::renderList() {
    ImGui::BeginChild("##mlib_list", { 220.0f, 0.0f }, true);

    if (m_macros.empty())
        ImGui::TextDisabled("%s", tr("macros.no_macros"));

    for (int i = 0; i < (int)m_macros.size(); i++) {
        bool sel = (i == m_selectedIdx);
        ImGui::PushID(i);

        const std::string& dsl = m_macros[i].second;
        std::string preview = dsl.size() > 38 ? dsl.substr(0, 35) + "..." : dsl;

        if (ImGui::Selectable("##sel", sel, 0, ImVec2(0.0f, 34.0f)))
            selectMacro(i);
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(m_macros[i].first.c_str());
        ImGui::TextDisabled("%s", preview.c_str());
        ImGui::EndGroup();

        ImGui::PopID();
    }

    ImGui::EndChild();
}

void MacroManagerPanel::renderActions() {
    ImGui::BeginChild("##mlib_actions", { 0.0f, 0.0f }, false);

    bool hasSel = (m_selectedIdx >= 0);

    // ── Action buttons ────────────────────────────────────────
    if (ImGui::Button(tr("btn.new")))
        m_creator.open(MacroCreatorModal::Mode::kLibrary, "", "");

    ImGui::SameLine();
    ImGui::BeginDisabled(!hasSel);
    if (ImGui::Button(tr("btn.edit")))
        m_creator.open(MacroCreatorModal::Mode::kLibrary,
                       m_macros[m_selectedIdx].first,
                       m_macros[m_selectedIdx].second);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!hasSel);
    if (ImGui::Button(tr("btn.copy")))
        m_creator.open(MacroCreatorModal::Mode::kLibrary,
                       m_macros[m_selectedIdx].first + " Copy",
                       m_macros[m_selectedIdx].second);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!hasSel);
    if (ImGui::Button(tr("btn.delete")))
        ImGui::OpenPopup("##macro_del_confirm");
    ImGui::EndDisabled();

    if (!m_commitError.empty()) {
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f }, "%s", m_commitError.c_str());
    }

    // ── Delete confirmation ───────────────────────────────────
    if (ImGui::BeginPopupModal("##macro_del_confirm", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", tr("macros.confirm_delete"));
        ImGui::Spacing();
        if (ImGui::Button(tr("btn.delete"), { 100.0f, 0.0f })) {
            m_macros.erase(m_macros.begin() + m_selectedIdx);
            save();
            m_macrosSaved = true;
            m_selectedIdx = -1;
            m_commitError.clear();
            showToast("macros.toast_deleted");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (ImGui::Button(tr("btn.cancel"), { 100.0f, 0.0f }))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (!hasSel) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", tr("macros.hint_select"));
    }

    // ── DSL reference (always visible) ───────────────────────
    ImGui::Spacing();
    renderReference();

    ImGui::EndChild();
}

void MacroManagerPanel::renderReference() {
    if (!ImGui::CollapsingHeader(tr("macros.ref_title")))
        return;

    ImGui::Spacing();

    if (ImGui::BeginTabBar("##ref_tabs")) {

        // ── Operators tab ─────────────────────────────────────
        if (ImGui::BeginTabItem(tr("macros.ref_operators"))) {
            ImGui::Spacing();
            static const struct { const char* syntax; const char* effect; } kOps[] = {
                { "A, B, C",        "Sequence: A then B then C (200 ms each)" },
                { "A + Y",          "Combo: A and Y at the same time" },
                { "B=1000",         "Hold B for 1000 ms" },
                { "500",            "Wait (pause) of 500 ms" },
                { "A*5000",         "Repeat A for 5000 ms (200 ms interval)" },
                { "A*1000/10",      "Repeat A 10 times in 1000 ms" },
                { "A*UP",           "Repeat while physical button held" },
                { "A*DO",           "Toggle: start on press, stop on next press" },
                { "(A,B)*5000",     "Loop sequence A,B for 5000 ms" },
                { "(A,B)*1000/5",   "Loop sequence 5 times in 1000 ms" },
                { "(A,B)*UP",       "Loop sequence while button held" },
                { "(A,B)*DO",       "Toggle sequence on/off" },
            };
            constexpr int kOpsRows = (int)(sizeof(kOps) / sizeof(kOps[0]));

            if (ImGui::BeginTable("##ref_ops", 2,
                    ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH |
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Syntax",  ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("Effect",  ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (int i = 0; i < kOpsRows; i++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(kOps[i].syntax);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("%s", kOps[i].effect);
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        // ── Buttons / Analog tab ──────────────────────────────
        if (ImGui::BeginTabItem(tr("macros.ref_buttons"))) {
            ImGui::Spacing();
            static const struct { const char* token; const char* desc; } kBtns[] = {
                { "A  B  X  Y",         "Face buttons" },
                { "L1  R1",             "LB / RB  (bumpers)" },
                { "L2  R2",             "LT / RT  (triggers)" },
                { "L3  R3",             "LS / RS  (stick clicks)" },
                { "CU  CD  CL  CR",     "D-pad  Up / Down / Left / Right" },
                { "CUR  CUL  CDR  CDL", "D-pad diagonals" },
                { "ST  SE",             "Start / Select (Back)" },
                { "LAX  LAY",           "Left stick  X / Y  [-1.0 .. 1.0]" },
                { "RAX  RAY",           "Right stick X / Y  [-1.0 .. 1.0]" },
            };
            constexpr int kBtnsRows = (int)(sizeof(kBtns) / sizeof(kBtns[0]));

            if (ImGui::BeginTable("##ref_btns", 2,
                    ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH |
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Token", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn("",      ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                for (int i = 0; i < kBtnsRows; i++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(kBtns[i].token);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("%s", kBtns[i].desc);
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Analog tip: diagonals use +/-0.71 (cos/sin 45deg), not +/-0.5");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}
