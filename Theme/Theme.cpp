#include "Theme.h"

namespace
{
// --- Ground Station palette: change these to restyle the whole app ---
constexpr ImVec4 kBackground = {0.067f, 0.078f, 0.094f, 1.00f};  // #111418, behind all windows
constexpr ImVec4 kPanel      = {0.106f, 0.122f, 0.145f, 1.00f};  // window background
constexpr ImVec4 kSurface    = {0.149f, 0.173f, 0.204f, 1.00f};  // frames, headers
constexpr ImVec4 kSurfaceHi  = {0.196f, 0.227f, 0.267f, 1.00f};  // hovered surfaces
constexpr ImVec4 kBorder     = {0.220f, 0.255f, 0.298f, 1.00f};
constexpr ImVec4 kText       = {0.902f, 0.925f, 0.949f, 1.00f};
constexpr ImVec4 kTextDim    = {0.533f, 0.580f, 0.635f, 1.00f};
constexpr ImVec4 kAccent     = {0.157f, 0.471f, 0.902f, 1.00f};  // matches the map pin
constexpr ImVec4 kAccentHi   = {0.271f, 0.565f, 0.965f, 1.00f};
constexpr ImVec4 kAccentLo   = {0.118f, 0.353f, 0.682f, 1.00f};

ImVec4 alpha(ImVec4 c, float a) { return ImVec4(c.x, c.y, c.z, a); }

void applyGroundStation()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();  // start from defaults so switching themes is clean

    // Shape
    s.WindowRounding = 6.0f;
    s.ChildRounding = 6.0f;
    s.FrameRounding = 4.0f;
    s.PopupRounding = 6.0f;
    s.GrabRounding = 4.0f;
    s.TabRounding = 4.0f;
    s.ScrollbarRounding = 8.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.TabBorderSize = 0.0f;

    // Spacing
    s.WindowPadding = ImVec2(10, 10);
    s.FramePadding = ImVec2(8, 5);
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.ScrollbarSize = 12.0f;
    s.GrabMinSize = 10.0f;
    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = kText;
    c[ImGuiCol_TextDisabled]          = kTextDim;
    c[ImGuiCol_WindowBg]              = kPanel;
    c[ImGuiCol_ChildBg]               = alpha(kPanel, 0.0f);
    c[ImGuiCol_PopupBg]               = alpha(kPanel, 0.98f);
    c[ImGuiCol_Border]                = kBorder;
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]               = kSurface;
    c[ImGuiCol_FrameBgHovered]        = kSurfaceHi;
    c[ImGuiCol_FrameBgActive]         = alpha(kAccentLo, 0.6f);

    c[ImGuiCol_TitleBg]               = kBackground;
    c[ImGuiCol_TitleBgActive]         = kSurface;
    c[ImGuiCol_TitleBgCollapsed]      = alpha(kBackground, 0.8f);
    c[ImGuiCol_MenuBarBg]             = kBackground;

    c[ImGuiCol_ScrollbarBg]           = alpha(kBackground, 0.5f);
    c[ImGuiCol_ScrollbarGrab]         = kSurfaceHi;
    c[ImGuiCol_ScrollbarGrabHovered]  = kBorder;
    c[ImGuiCol_ScrollbarGrabActive]   = kAccent;

    c[ImGuiCol_CheckMark]             = kAccentHi;
    c[ImGuiCol_SliderGrab]            = kAccent;
    c[ImGuiCol_SliderGrabActive]      = kAccentHi;

    c[ImGuiCol_Button]                = kSurface;
    c[ImGuiCol_ButtonHovered]         = kAccentLo;
    c[ImGuiCol_ButtonActive]          = kAccent;

    c[ImGuiCol_Header]                = alpha(kAccent, 0.35f);
    c[ImGuiCol_HeaderHovered]         = alpha(kAccent, 0.55f);
    c[ImGuiCol_HeaderActive]          = kAccent;

    c[ImGuiCol_Separator]             = kBorder;
    c[ImGuiCol_SeparatorHovered]      = kAccentLo;
    c[ImGuiCol_SeparatorActive]       = kAccent;

    c[ImGuiCol_ResizeGrip]            = alpha(kAccent, 0.20f);
    c[ImGuiCol_ResizeGripHovered]     = alpha(kAccent, 0.60f);
    c[ImGuiCol_ResizeGripActive]      = kAccent;

    c[ImGuiCol_Tab]                   = kBackground;
    c[ImGuiCol_TabHovered]            = kAccentLo;
    c[ImGuiCol_TabActive]             = kSurface;
    c[ImGuiCol_TabUnfocused]          = kBackground;
    c[ImGuiCol_TabUnfocusedActive]    = kPanel;
    c[ImGuiCol_DockingPreview]        = alpha(kAccent, 0.5f);
    c[ImGuiCol_DockingEmptyBg]        = kBackground;

    c[ImGuiCol_PlotLines]             = kAccentHi;
    c[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.60f, 0.20f, 1.0f);
    c[ImGuiCol_PlotHistogram]         = kAccent;
    c[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.20f, 1.0f);

    c[ImGuiCol_TableHeaderBg]         = kSurface;
    c[ImGuiCol_TableBorderStrong]     = kBorder;
    c[ImGuiCol_TableBorderLight]      = alpha(kBorder, 0.5f);
    c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, 0.03f);

    c[ImGuiCol_TextSelectedBg]        = alpha(kAccent, 0.40f);
    c[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 0.60f, 0.20f, 0.9f);
    c[ImGuiCol_NavHighlight]          = kAccentHi;
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.7f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0, 0, 0, 0.5f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.6f);
}
} // namespace

void applyTheme(Theme theme)
{
    switch (theme)
    {
    case Theme::ImGuiDark:
        ImGui::GetStyle() = ImGuiStyle();
        ImGui::StyleColorsDark();
        break;
    case Theme::ImGuiLight:
        ImGui::GetStyle() = ImGuiStyle();
        ImGui::StyleColorsLight();
        break;
    default:
        applyGroundStation();
        break;
    }
}

const char* themeName(Theme theme)
{
    switch (theme)
    {
    case Theme::ImGuiDark:  return "ImGui Dark";
    case Theme::ImGuiLight: return "ImGui Light";
    default:                return "Ground Station";
    }
}

ImVec4 themeBackground(Theme theme)
{
    switch (theme)
    {
    case Theme::ImGuiDark:  return ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
    case Theme::ImGuiLight: return ImVec4(0.80f, 0.82f, 0.85f, 1.0f);
    default:                return kBackground;
    }
}
