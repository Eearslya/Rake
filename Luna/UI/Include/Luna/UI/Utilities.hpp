#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace Luna {
namespace UI {
ImU32 ColorSaturation(const ImColor& color, float saturation);

bool BeginMenuBar(const ImRect& rect);
void EndMenuBar();

ImRect RectOffset(const ImRect& rect, float x, float y);
}  // namespace UI
}  // namespace Luna
