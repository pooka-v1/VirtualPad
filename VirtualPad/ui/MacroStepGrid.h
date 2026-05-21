#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <d3d11.h>
#include "../imgui/imgui.h"
#include "MacroStep.h"
#include "PadView.h"

// Renders the visual step sequence (the "Sequence" section of MacroCreatorModal).
// Owned by MacroCreatorModal; receives references to modal-owned state.
class MacroStepGrid {
public:
    explicit MacroStepGrid(const std::unordered_map<std::string, PadTexture>& icons);

    // Renders the step list. Handles step selection and "Clear all".
    // activeStep: index of the selected step (-1 = none). Updated on click.
    // dslOnly is passed by reference: cleared to false when the list is cleared.
    // Returns true if steps were modified (clear).
    bool render(std::vector<MacroStepItem>& steps, bool& dslOnly, int& activeStep);

private:
    const std::unordered_map<std::string, PadTexture>& m_icons;
    ImTextureID iconSrv(const std::string& name) const;
};
