#include "glad/glad.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include "Map/MapWidget.h"
#include "Theme/Theme.h"
#include "imgui_internal.h"  // DockBuilder API

#ifndef WORLDMAP_ASSETS_DIR
#define WORLDMAP_ASSETS_DIR "../assets"
#endif

// Where downloaded map tiles are cached, so they work offline after the first view.
static std::string tileCacheDir()
{
#if defined(_WIN32)
    const char* base = std::getenv("LOCALAPPDATA");
    return base ? std::string(base) + "/Opengl-WorldMap/tiles" : "tile_cache";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/Library/Caches/Opengl-WorldMap/tiles" : "tile_cache";
#else
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/.cache/Opengl-WorldMap/tiles" : "tile_cache";
#endif
}


// Default docking layout: Map Settings on the left, World Map filling the rest.
// Style Editor opens as a tab next to Map Settings.
static void buildDefaultLayout(ImGuiID dockspaceId)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID center = dockspaceId;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, nullptr, &center);

    ImGui::DockBuilderDockWindow("Map Settings", left);
    ImGui::DockBuilderDockWindow("Style Editor", left);
    ImGui::DockBuilderDockWindow("World Map", center);
    ImGui::DockBuilderFinish(dockspaceId);
}

int main(void)
{
    
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.3 core + GLSL 330 (matches the bundled GLAD loader)
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    /* Create a windowed mode window and its OpenGL context */
    GLFWwindow* window = glfwCreateWindow(1600, 900, "Opengl-WorldMap", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync


    // Initialize Glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Opengl-WorldMap: failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.IniFilename = nullptr;  // Don't restore a saved layout; the fixed default layout is built every launch
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows


    ImFontConfig font_config;
    font_config.PixelSnapH = false;
    font_config.OversampleH = 5;
    font_config.OversampleV = 5;
    font_config.RasterizerMultiply = 1.2f;


    // ImGui asserts on a missing font file, so check it exists first
    const char* font_path = WORLDMAP_ASSETS_DIR "/fonts/OpenSans.ttf";
    ImFont* font1 = nullptr;
    if (FILE* f = fopen(font_path, "rb"))
    {
        fclose(f);
        font1 = io.Fonts->AddFontFromFileTTF(font_path, 20, &font_config);
    }
    if (font1 == nullptr)
    {
        fprintf(stderr, "Opengl-WorldMap: could not load OpenSans.ttf, using default font\n");
        io.Fonts->AddFontDefault();
    }
    

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Must be destroyed before the GL context goes away (it owns textures).
    auto map = std::make_unique<MapWidget>(tileCacheDir());
    map->setCenter(40.0, 20.0);
    map->setZoom(4.0);
    bool demoVehicle = false;
    double demoLat = 0.0, demoLon = 0.0;

    Theme theme = Theme::GroundStation;
    applyTheme(theme);
    bool showStyleEditor = false;
    bool showMapSettings = true;
    bool resetLayout = true;  // build the default layout on the first frame

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Menu bar
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Map Settings", nullptr, &showMapSettings);
                ImGui::MenuItem("Style Editor", nullptr, &showStyleEditor);
                ImGui::Separator();
                if (ImGui::BeginMenu("Theme"))
                {
                    for (int i = 0; i < int(Theme::Count); ++i)
                    {
                        const Theme t = Theme(i);
                        if (ImGui::MenuItem(themeName(t), nullptr, t == theme))
                        {
                            theme = t;
                            applyTheme(theme);
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Reset Layout"))
                {
                    resetLayout = true;
                    showMapSettings = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Dockspace filling the window below the menu bar; every window can be docked into it
        const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        if (resetLayout)
        {
            buildDefaultLayout(dockspaceId);
            resetLayout = false;
        }

        // --- Map Settings panel ---
        if (showMapSettings)
        {
            ImGui::Begin("Map Settings", &showMapSettings);

            ImGui::SeparatorText("Tiles");
            int source = map->sourceIndex();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##tiles", MapWidget::sourceAt(source).name))
            {
                for (int i = 0; i < MapWidget::sourceCount(); ++i)
                    if (ImGui::Selectable(MapWidget::sourceAt(i).name, i == source))
                        map->setSource(i);
                ImGui::EndCombo();
            }

            ImGui::SeparatorText("View");
            ImGui::Text("Center  %.6f, %.6f", map->centerLat(), map->centerLon());
            ImGui::Text("Zoom    %.2f", map->zoom());

            ImGui::SeparatorText("Picked point");
            if (map->hasPickedPoint())
            {
                char coords[64];
                snprintf(coords, sizeof(coords), "%.6f, %.6f", map->pickedLat(), map->pickedLon());
                ImGui::Text("Lat  %.6f", map->pickedLat());
                ImGui::Text("Lon  %.6f", map->pickedLon());
                if (ImGui::Button("Copy"))
                    ImGui::SetClipboardText(coords);
                ImGui::SameLine();
                if (ImGui::Button("Center"))
                    map->setCenter(map->pickedLat(), map->pickedLon());
                ImGui::SameLine();
                if (ImGui::Button("Clear"))
                    map->clearPickedPoint();
            }
            else
            {
                ImGui::TextDisabled("Click the map to pick a point");
            }

            ImGui::SeparatorText("Vehicle");
            if (ImGui::Checkbox("Demo vehicle", &demoVehicle) && demoVehicle)
            {
                demoLat = map->centerLat();
                demoLon = map->centerLon();
            }

            ImGui::Spacing();
            ImGui::TextDisabled("%.0f FPS", io.Framerate);
            ImGui::End();
        }

        if (demoVehicle)
        {
            // Fake telemetry: fly a circle around where the demo was started.
            // Replace this with real MAVLink position/heading later.
            const double t = glfwGetTime() * 0.3;
            const double r = 0.01;  // degrees
            const double lat = demoLat + r * std::sin(t);
            const double lon = demoLon + r * std::cos(t) / std::cos(demoLat * 3.14159265358979 / 180.0);
            const float heading = float(std::atan2(-std::sin(t), std::cos(t)) * 180.0 / 3.14159265358979);
            map->setVehicle(lat, lon, heading);
        }
        else
        {
            map->clearVehicle();
        }

        // --- Style Editor ---
        if (showStyleEditor)
        {
            ImGui::Begin("Style Editor", &showStyleEditor);
            ImGui::ShowStyleEditor();  // tweak colors/sizes live; "Export" copies them as code
            ImGui::End();
        }

        // --- World Map (edge-to-edge, no padding) ---
        // NoScrollWithMouse: the mouse wheel zooms the map instead of scrolling the window
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("World Map", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        map->draw("##map");
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        const ImVec4 clear_color = themeBackground(theme);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);

    }

    // Cleanup
    map.reset();  // stops tile downloads and frees textures while the GL context is alive
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
