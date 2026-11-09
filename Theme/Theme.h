#pragma once

#include "imgui.h"

// UI themes. Edit the palette at the top of Theme.cpp to restyle the app.
enum class Theme
{
    GroundStation,  // custom dark theme with a blue accent (default)
    ImGuiDark,
    ImGuiLight,
    Count
};

void applyTheme(Theme theme);
const char* themeName(Theme theme);

// Color used to clear the OpenGL background behind all windows.
ImVec4 themeBackground(Theme theme);
