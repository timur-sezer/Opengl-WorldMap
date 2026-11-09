#pragma once

// An ImGui widget that draws a pannable / zoomable world map (Web Mercator slippy tiles).
//   - drag with the left mouse button to pan
//   - mouse wheel to zoom around the cursor, double-click to zoom in

#include "TileManager.h"
#include "imgui.h"

#include <memory>
#include <string>

class MapWidget
{
public:
    explicit MapWidget(std::string cacheRoot);

    // Draws the map. A size of 0 on an axis fills the remaining content region.
    void draw(const char* id, ImVec2 size = ImVec2(0, 0));

    static int sourceCount();
    static const TileSource& sourceAt(int index);
    void setSource(int index);
    int sourceIndex() const { return sourceIndex_; }

    void setCenter(double latDeg, double lonDeg);
    double centerLat() const;
    double centerLon() const;
    void setZoom(double zoom);
    double zoom() const { return zoom_; }

    // Vehicle marker; heading in degrees clockwise from north.
    void setVehicle(double latDeg, double lonDeg, float headingDeg);
    void clearVehicle() { hasVehicle_ = false; }

    // Point picked by left-clicking the map (a click without dragging).
    bool hasPickedPoint() const { return hasPick_; }
    double pickedLat() const;
    double pickedLon() const;
    void clearPickedPoint() { hasPick_ = false; }

private:
    // Screen position of a normalized world point, using the wrapped copy closest to the view center.
    ImVec2 worldToScreen(double x, double y, ImVec2 mid, double ws) const;
    void drawPin(ImDrawList* dl, ImVec2 mid, double ws);

    double worldSize() const;  // size of the whole world in screen pixels at the current zoom
    void zoomAround(ImVec2 mouse, ImVec2 mid, double delta);
    void drawTile(ImDrawList* dl, const TileKey& key, ImVec2 a, ImVec2 b);
    void drawVehicle(ImDrawList* dl, ImVec2 mid, double ws);

    std::string cacheRoot_;
    std::unique_ptr<TileManager> tiles_;
    int sourceIndex_ = 0;

    // Map center in normalized Web Mercator coordinates, both in [0, 1].
    double cx_ = 0.5, cy_ = 0.5;
    double zoom_ = 3.0;

    bool hasVehicle_ = false;
    double vehicleLat_ = 0.0, vehicleLon_ = 0.0;
    float vehicleHeading_ = 0.0f;

    bool hasPick_ = false;
    double pickX_ = 0.0, pickY_ = 0.0;  // normalized Web Mercator
    bool dragged_ = false;               // current mouse press turned into a drag
};
