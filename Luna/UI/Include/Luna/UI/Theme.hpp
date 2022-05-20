#pragma once

#include <imgui.h>

namespace Luna {
namespace UI {
struct Theme {
	ImColor Accent             = IM_COL32(236, 158, 36, 255);
	ImColor Background         = IM_COL32(36, 36, 36, 255);
	ImColor BackgroundDark     = IM_COL32(26, 26, 26, 255);
	ImColor Button             = IM_COL32(56, 56, 56, 200);
	ImColor ButtonActive       = IM_COL32(56, 56, 56, 150);
	ImColor ButtonHover        = IM_COL32(70, 70, 70, 255);
	ImColor GroupHeader        = IM_COL32(47, 47, 47, 255);
	ImColor GroupHeaderHover   = IM_COL32(67, 67, 67, 255);
	ImColor Highlight          = IM_COL32(39, 185, 242, 255);
	ImColor PropertyField      = IM_COL32(15, 15, 15, 255);
	ImColor PropertyFieldHover = IM_COL32(25, 25, 25, 255);
	ImColor TabActive          = IM_COL32(156, 21, 218, 100);
	ImColor TabHover           = IM_COL32(156, 21, 218, 140);
	ImColor Titlebar           = IM_COL32(21, 21, 21, 255);
	ImColor Text               = IM_COL32(192, 192, 192, 255);
	ImColor TextBright         = IM_COL32(210, 210, 210, 255);
	ImColor TextDark           = IM_COL32(128, 128, 128, 255);
};
}  // namespace UI
}  // namespace Luna
