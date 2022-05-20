#include <Luna/UI/Utilities.hpp>
#include <algorithm>

namespace Luna {
namespace UI {
ImU32 ColorSaturation(const ImColor& color, float saturation) {
	const ImVec4& color4 = color.Value;
	float hue, sat, val;
	ImGui::ColorConvertRGBtoHSV(color4.x, color4.y, color4.z, hue, sat, val);

	return ImColor::HSV(hue, std::min(saturation, 1.0f), val);
}

bool BeginMenuBar(const ImRect& rect) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) { return false; }

	ImGui::BeginGroup();
	ImGui::PushID("##MenuBar");
	const ImVec2 padding = window->WindowPadding;
	const ImRect barRect = RectOffset(rect, 0.0f, padding.y);
	ImRect clipRect =
		ImRect(IM_ROUND(ImMax(window->Pos.x, barRect.Min.x + window->WindowBorderSize + window->Pos.x - 10.0f)),
	         IM_ROUND(barRect.Min.y + window->WindowBorderSize + window->Pos.y),
	         IM_ROUND(ImMax(barRect.Min.x + window->Pos.x,
	                        barRect.Max.x - ImMax(window->WindowRounding, window->WindowBorderSize))),
	         IM_ROUND(barRect.Max.y + window->Pos.y));
	clipRect.ClipWith(window->OuterRectClipped);
	ImGui::PushClipRect(clipRect.Min, clipRect.Max, false);

	window->DC.CursorPos        = ImVec2(barRect.Min.x + window->Pos.x, barRect.Min.y + window->Pos.y);
	window->DC.CursorMaxPos     = window->DC.CursorPos;
	window->DC.LayoutType       = ImGuiLayoutType_Horizontal;
	window->DC.NavLayerCurrent  = ImGuiNavLayer_Menu;
	window->DC.MenuBarAppending = true;
	ImGui::AlignTextToFramePadding();

	return true;
}

void EndMenuBar() {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems) { return; }
	ImGuiContext& g = *GImGui;

	if (ImGui::NavMoveRequestButNoResultYet() && (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right) &&
	    (g.NavWindow->Flags & ImGuiWindowFlags_ChildMenu)) {
		ImGuiWindow* nav_earliest_child = g.NavWindow;

		while (nav_earliest_child->ParentWindow && (nav_earliest_child->ParentWindow->Flags & ImGuiWindowFlags_ChildMenu)) {
			nav_earliest_child = nav_earliest_child->ParentWindow;
		}

		if (nav_earliest_child->ParentWindow == window &&
		    nav_earliest_child->DC.ParentLayoutType == ImGuiLayoutType_Horizontal &&
		    (g.NavMoveFlags & ImGuiNavMoveFlags_Forwarded) == 0) {
			const ImGuiNavLayer layer = ImGuiNavLayer_Menu;
			IM_ASSERT(window->DC.NavLayersActiveMaskNext & (1 << layer));  // Sanity check
			ImGui::FocusWindow(window);
			ImGui::SetNavID(window->NavLastIds[layer], layer, 0, window->NavRectRel[layer]);
			g.NavDisableHighlight  = true;
			g.NavDisableMouseHover = g.NavMousePosDirty = true;
			ImGui::NavMoveRequestForward(g.NavMoveDir, g.NavMoveClipDir, g.NavMoveFlags, g.NavMoveScrollFlags);
		}
	}

	IM_MSVC_WARNING_SUPPRESS(6011);
	IM_ASSERT(window->DC.MenuBarAppending);
	ImGui::PopClipRect();
	ImGui::PopID();
	window->DC.MenuBarOffset.x   = window->DC.CursorPos.x - window->Pos.x;
	g.GroupStack.back().EmitItem = false;
	ImGui::EndGroup();
	window->DC.LayoutType       = ImGuiLayoutType_Vertical;
	window->DC.NavLayerCurrent  = ImGuiNavLayer_Main;
	window->DC.MenuBarAppending = false;
}

ImRect RectOffset(const ImRect& rect, float x, float y) {
	return ImRect(rect.Min.x + x, rect.Min.y + y, rect.Max.x + x, rect.Max.y + y);
}
}  // namespace UI
}  // namespace Luna
