#include "TriggerRangeModal.h"
#include "../config/Strings.h"
#include "ActionPanel.h"
#include "../imgui/imgui.h"
#include "../nlohmann/json.hpp"
using json = nlohmann::json;

#include <fstream>
#include <algorithm>

// ---------------------------------------------------------------------------
void TriggerRangeModal::open(const std::string& trigger, const std::vector<RangeEdit>& current) {
    m_forKey = trigger;
    m_work       = current;
    m_selSect    = -1;
    m_actType    = H5ActionType::Xbox;
    m_captureKeys.clear();
    m_macroSel.clear();
    m_xboxSel    = -1;
    if (m_work.empty()) {
        RangeEdit re; re.from = 0.1f; re.to = 1.0f;
        m_work.push_back(re);
    }
    m_open = true;
}

// ---------------------------------------------------------------------------
bool TriggerRangeModal::render() {
    if (!m_open) return false;
    ImGui::OpenPopup(trid("ranges.title", "rangosModal").c_str());

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 600.0f, 0.0f });

    if (!ImGui::BeginPopupModal(trid("ranges.title", "rangosModal").c_str(), nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) return false;

    struct XboxChoice { const char* display; const char* name; };
    static const XboxChoice kChoices[] = {
        {"A","a"},{"B","b"},{"X","x"},{"Y","y"},
        {"L1","l1"},{"R1","r1"},{"Select","select"},{"Start","start"},{"Home","home"},
        {"L3","l3"},{"R3","r3"},
        {"Cruceta Arriba","dpad_up"},{"Cruceta Abajo","dpad_down"},
        {"Cruceta Izq","dpad_left"},{"Cruceta Der","dpad_right"},
        {"L Arriba","left_y_pos"},{"L Abajo","left_y_neg"},
        {"L Derecha","left_x_pos"},{"L Izquierda","left_x_neg"},
        {"R Arriba","right_y_pos"},{"R Abajo","right_y_neg"},
        {"R Derecha","right_x_pos"},{"R Izquierda","right_x_neg"},
    };
    static const int kNChoices = 23;

    std::string hdrStr = m_forKey + "  \xe2\x86\x92  Zonas de recorrido";
    ImGui::TextColored({ 1.0f, 0.86f, 0.0f, 1.0f }, "%s", hdrStr.c_str());
    ImGui::Spacing();

    // ── Barra visual de rangos ────────────────────────────────────────────────
    {
        int n = (int)m_work.size();
        float barW  = ImGui::GetContentRegionAvail().x - 4.0f;
        float barH  = 28.0f;
        ImVec2 barMin = { ImGui::GetCursorScreenPos().x + 2.0f, ImGui::GetCursorScreenPos().y };
        ImVec2 barMax = { barMin.x + barW, barMin.y + barH };
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(barMin, barMax, IM_COL32(40,40,40,255), 4.0f);

        for (int i = 0; i < n; ++i) {
            float t0 = (m_work[i].from - 0.1f) / 0.9f;
            float t1 = (m_work[i].to   - 0.1f) / 0.9f;
            t0 = std::clamp(t0, 0.0f, 1.0f);
            t1 = std::clamp(t1, 0.0f, 1.0f);
            ImVec2 r0 = { barMin.x + t0 * barW + 1.0f, barMin.y + 1.0f };
            ImVec2 r1 = { barMin.x + t1 * barW - 1.0f, barMax.y - 1.0f };
            ImU32 col = (i == m_selSect)
                ? IM_COL32(255,180,0,220)
                : (m_work[i].hasAction ? IM_COL32(60,160,80,200) : IM_COL32(80,80,120,180));
            dl->AddRectFilled(r0, r1, col, 3.0f);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f\xe2\x80\x93%.2f", m_work[i].from, m_work[i].to);
            ImVec2 textSz = ImGui::CalcTextSize(buf);
            float cx = (r0.x + r1.x) * 0.5f - textSz.x * 0.5f;
            float cy = (r0.y + r1.y) * 0.5f - textSz.y * 0.5f;
            if (cx >= r0.x && cx + textSz.x <= r1.x)
                dl->AddText({ cx, cy }, IM_COL32(230,230,230,255), buf);
            if (i > 0)
                dl->AddLine({ barMin.x + t0 * barW, barMin.y }, { barMin.x + t0 * barW, barMax.y },
                             IM_COL32(200,200,200,160), 1.5f);
        }
        dl->AddRect(barMin, barMax, IM_COL32(150,150,150,200), 4.0f);

        ImGui::InvisibleButton("##rangeBar", { barW + 4.0f, barH });
        if (ImGui::IsItemClicked()) {
            float mx = ImGui::GetIO().MousePos.x - barMin.x;
            float normPos = mx / barW;
            float trigPos = normPos * 0.9f + 0.1f;
            for (int i = 0; i < n; ++i) {
                if (trigPos >= m_work[i].from && trigPos <= m_work[i].to) {
                    m_selSect = (m_selSect == i) ? -1 : i;
                    m_actType = H5ActionType::Xbox;
                    m_captureKeys.clear(); m_macroSel.clear(); m_xboxSel = -1;
                    if (m_selSect >= 0 && m_work[i].hasAction) {
                        const auto& act = m_work[i].action;
                        if (act.type == ButtonActionType::Macro)           m_actType = H5ActionType::Macro;
                        else if (act.type == ButtonActionType::Keyboard)   m_actType = H5ActionType::Keyboard;
                        else if (act.type == ButtonActionType::MouseClick) m_actType = H5ActionType::Mouse;
                        else {
                            m_actType = H5ActionType::Xbox;
                            if (act.type == ButtonActionType::VirtualButton) {
                                for (int ci = 0; ci < kNChoices; ++ci)
                                    if (act.name == kChoices[ci].name) { m_xboxSel = ci; break; }
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
    ImGui::Spacing();

    // ── Botones de gestión de zonas ───────────────────────────────────────────
    {
        int n = (int)m_work.size();
        bool canAdd = (n < 10);
        if (!canAdd) ImGui::BeginDisabled();
        if (ImGui::Button(n == 1 ? tr("ranges.split_create") : tr("ranges.split_add"), { 0.0f, 0.0f })) {
            int newN = n + 1;
            m_work.clear();
            for (int i = 0; i < newN; ++i) {
                RangeEdit re;
                re.from = 0.1f + i       * 0.9f / (float)newN;
                re.to   = 0.1f + (i + 1) * 0.9f / (float)newN;
                if (i == newN - 1) re.to = 1.0f;
                m_work.push_back(re);
            }
            m_selSect = -1; m_actType = H5ActionType::Xbox;
            m_captureKeys.clear(); m_macroSel.clear();
        }
        if (!canAdd) ImGui::EndDisabled();
        ImGui::SameLine();
        bool canRemove = (n > 1);
        if (!canRemove) ImGui::BeginDisabled();
        if (ImGui::Button(trid("ranges.remove", "rangeRm").c_str(), { 0.0f, 0.0f }) && canRemove && m_selSect >= 0) {
            m_work.erase(m_work.begin() + m_selSect);
            int newN = (int)m_work.size();
            for (int i = 0; i < newN; ++i) {
                m_work[i].from = 0.1f + i       * 0.9f / (float)newN;
                m_work[i].to   = 0.1f + (i + 1) * 0.9f / (float)newN;
                if (i == newN - 1) m_work[i].to = 1.0f;
            }
            m_selSect = -1;
        }
        if (!canRemove) ImGui::EndDisabled();
        ImGui::SameLine();
        if (m_selSect >= 0 && m_work[m_selSect].hasAction) {
            if (ImGui::Button(trid("ranges.clear_action", "rangeClear").c_str())) {
                m_work[m_selSect].hasAction = false;
                m_work[m_selSect].action = ButtonAction{};
                m_actType = H5ActionType::Xbox; m_captureKeys.clear();
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled(tr("ranges.count"), (int)m_work.size());
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ── Panel de acción de la zona seleccionada ───────────────────────────────
    if (m_selSect < 0) {
        ImGui::TextDisabled("%s", tr("ranges.hint"));
    } else {
        ImGui::Text(tr("ranges.zone"),
                    m_selSect + 1,
                    m_work[m_selSect].from,
                    m_work[m_selSect].to);
        ImGui::Spacing();

        float bW = 85.0f;
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float totalBtnW = bW * 4 + sp * 3;
        float offBX = (ImGui::GetContentRegionAvail().x - totalBtnW) * 0.5f;
        if (offBX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offBX);
        auto rTypeBtn = [&](const char* lbl, H5ActionType t) {
            bool sel = (m_actType == t);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            if (ImGui::Button(lbl, { bW, 0.0f })) { m_actType = t; m_captureKeys.clear(); m_macroSel.clear(); m_xboxSel = -1; }
            if (sel) ImGui::PopStyleColor();
        };
        rTypeBtn("Mando##rt0",   H5ActionType::Xbox);    ImGui::SameLine();
        rTypeBtn("Macro##rt1",   H5ActionType::Macro);   ImGui::SameLine();
        rTypeBtn("Teclado##rt2", H5ActionType::Keyboard); ImGui::SameLine();
        rTypeBtn("Rat\xC3\xB3n##rt3", H5ActionType::Mouse);

        ImGui::Spacing();

        if (m_actType == H5ActionType::Xbox) {
            float cW = 220.0f;
            float cOff = (ImGui::GetContentRegionAvail().x - cW - sp - 80.0f) * 0.5f;
            if (cOff > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cOff);
            ImGui::SetNextItemWidth(cW);
            const char* preview = (m_xboxSel >= 0 && m_xboxSel < kNChoices)
                ? kChoices[m_xboxSel].display
                : "-- elige bot\xC3\xB3n --";
            if (m_xboxSel < 0 && m_work[m_selSect].hasAction) {
                const auto& act = m_work[m_selSect].action;
                if (act.type == ButtonActionType::VirtualButton) {
                    for (int ci = 0; ci < kNChoices; ++ci) {
                        if (act.name == kChoices[ci].name) { m_xboxSel = ci; break; }
                    }
                    if (m_xboxSel >= 0) preview = kChoices[m_xboxSel].display;
                }
            }
            if (ImGui::BeginCombo("##rangesXbox", preview)) {
                for (int ci = 0; ci < kNChoices; ++ci) {
                    bool sel = (m_xboxSel == ci);
                    if (ImGui::Selectable(kChoices[ci].display, sel)) m_xboxSel = ci;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            bool canAssign = (m_xboxSel >= 0);
            if (!canAssign) ImGui::BeginDisabled();
            if (ImGui::Button(trid("btn.assign", "rxbAssign").c_str(), { 80.0f, 0.0f }) && canAssign) {
                ButtonAction act;
                act.type = ButtonActionType::VirtualButton;
                act.name = kChoices[m_xboxSel].name;
                m_work[m_selSect].action    = act;
                m_work[m_selSect].hasAction = true;
            }
            if (!canAssign) ImGui::EndDisabled();
            if (m_work[m_selSect].hasAction &&
                m_work[m_selSect].action.type == ButtonActionType::VirtualButton) {
                ImGui::SameLine();
                ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "\xe2\x86\x92 %s",
                    m_work[m_selSect].action.name.c_str());
            }

        } else if (m_actType == H5ActionType::Macro) {
            if (!m_macroNamesLoaded) {
                m_macroNames.clear();
                try {
                    std::ifstream f("data/macros.json");
                    if (f.is_open()) { json j = json::parse(f); for (auto& [k,v] : j.items()) m_macroNames.push_back(k); }
                } catch (...) {}
                m_macroNamesLoaded = true;
            }
            if (ActionPanel::renderMacroCombo("mac_rng", m_macroSel, m_macroNames,
                                              ImGui::GetContentRegionAvail().x)) {
                ButtonAction act;
                act.type = ButtonActionType::Macro; act.name = m_macroSel;
                m_work[m_selSect].action    = act;
                m_work[m_selSect].hasAction = true;
            }

        } else if (m_actType == H5ActionType::Keyboard) {
            if (ActionPanel::renderKeyboardCapture("kb_rng", m_captureKeys,
                                                   ImGui::GetContentRegionAvail().x, true)) {
                ButtonAction act;
                act.type = ButtonActionType::Keyboard;
                for (const auto& p : m_captureKeys) act.keys.push_back(p.first);
                m_work[m_selSect].action    = act;
                m_work[m_selSect].hasAction = true;
                m_captureKeys.clear();
            }
            if (m_work[m_selSect].hasAction &&
                m_work[m_selSect].action.type == ButtonActionType::Keyboard &&
                m_captureKeys.empty()) {
                std::string ex;
                for (const auto& k : m_work[m_selSect].action.keys) { if (!ex.empty()) ex += "+"; ex += k; }
                ImGui::TextDisabled(tr("ranges.current"), ex.c_str());
            }

        } else if (m_actType == H5ActionType::Mouse) {
            std::string mbResult;
            if (ActionPanel::renderMouseButtons("mb_rng", mbResult,
                                                ImGui::GetContentRegionAvail().x)) {
                ButtonAction act;
                act.type = ButtonActionType::MouseClick; act.mouseButton = mbResult;
                m_work[m_selSect].action    = act;
                m_work[m_selSect].hasAction = true;
            }
            if (m_work[m_selSect].hasAction &&
                m_work[m_selSect].action.type == ButtonActionType::MouseClick) {
                ImGui::SameLine();
                ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "\xe2\x86\x92 %s",
                    m_work[m_selSect].action.mouseButton.c_str());
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Aceptar / Cancelar ────────────────────────────────────────────────────
    float btnW2  = 100.0f;
    float dialogW = ImGui::GetContentRegionAvail().x;
    float btnOff = (dialogW - btnW2 * 2 - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (btnOff > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + btnOff);

    bool accepted = false;
    if (ImGui::Button(trid("btn.ok", "rangosOk").c_str(), { btnW2, 0.0f })) {
        m_open = false;
        ImGui::CloseCurrentPopup();
        accepted = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(trid("btn.cancel", "rangosCan").c_str(), { btnW2, 0.0f })) {
        m_open = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    return accepted;
}
