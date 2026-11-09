#include "MapWidget.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kMaxLat = 85.05112878;  // Web Mercator limit
constexpr double kMinZoom = 1.0;
constexpr double kMaxZoom = 20.0;

const TileSource kSources[] = {
    {"osm", "OpenStreetMap",
     "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
     "\xC2\xA9 OpenStreetMap contributors", 19},
    {"esri-imagery", "Esri World Imagery (satellite)",
     "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
     "Tiles \xC2\xA9 Esri, Maxar, Earthstar Geographics", 19},
};

double lonToX(double lon) { return (lon + 180.0) / 360.0; }
double xToLon(double x) { return x * 360.0 - 180.0; }

double latToY(double lat)
{
    const double r = std::clamp(lat, -kMaxLat, kMaxLat) * kPi / 180.0;
    return (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / kPi) / 2.0;
}

double yToLat(double y)
{
    return std::atan(std::sinh(kPi * (1.0 - 2.0 * y))) * 180.0 / kPi;
}

ImTextureID toImTexture(GLuint tex) { return (ImTextureID)(intptr_t)tex; }

void label(ImDrawList* dl, ImVec2 pos, const char* text)
{
    const ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddRectFilled(ImVec2(pos.x - 4, pos.y - 2), ImVec2(pos.x + ts.x + 4, pos.y + ts.y + 2),
                      IM_COL32(0, 0, 0, 160), 3.0f);
    dl->AddText(pos, IM_COL32(255, 255, 255, 255), text);
}
} // namespace

MapWidget::MapWidget(std::string cacheRoot) : cacheRoot_(std::move(cacheRoot))
{
    setSource(0);
}

int MapWidget::sourceCount() { return int(sizeof(kSources) / sizeof(kSources[0])); }
const TileSource& MapWidget::sourceAt(int index) { return kSources[index]; }

void MapWidget::setSource(int index)
{
    index = std::clamp(index, 0, sourceCount() - 1);
    if (tiles_ && index == sourceIndex_)
        return;
    sourceIndex_ = index;
    tiles_.reset();  // stop old workers before starting new ones
    tiles_ = std::make_unique<TileManager>(kSources[index], cacheRoot_ + "/" + kSources[index].id);
}

void MapWidget::setCenter(double latDeg, double lonDeg)
{
    cx_ = lonToX(lonDeg);
    cy_ = latToY(latDeg);
}

double MapWidget::centerLat() const { return yToLat(cy_); }
double MapWidget::centerLon() const { return xToLon(cx_); }
void MapWidget::setZoom(double zoom) { zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom); }

void MapWidget::setVehicle(double latDeg, double lonDeg, float headingDeg)
{
    hasVehicle_ = true;
    vehicleLat_ = latDeg;
    vehicleLon_ = lonDeg;
    vehicleHeading_ = headingDeg;
}

double MapWidget::worldSize() const { return 256.0 * std::pow(2.0, zoom_); }

void MapWidget::zoomAround(ImVec2 mouse, ImVec2 mid, double delta)
{
    // Keep the point under the cursor fixed while zooming.
    double ws = worldSize();
    const double mx = cx_ + (mouse.x - mid.x) / ws;
    const double my = cy_ + (mouse.y - mid.y) / ws;
    setZoom(zoom_ + delta);
    ws = worldSize();
    cx_ = mx - (mouse.x - mid.x) / ws;
    cy_ = my - (mouse.y - mid.y) / ws;
}

void MapWidget::draw(const char* id, ImVec2 size)
{
    tiles_->beginFrame();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (size.x <= 0) size.x = avail.x;
    if (size.y <= 0) size.y = avail.y;
    if (size.x < 1 || size.y < 1)
        return;

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    const ImVec2 mid(std::floor((p0.x + p1.x) * 0.5f), std::floor((p0.y + p1.y) * 0.5f));

    // --- Input ---
    ImGui::InvisibleButton(id, size, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();

    // A press that is released without moving past the drag threshold is a "pick" click.
    if (ImGui::IsItemActivated())
        dragged_ = false;
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        dragged_ = true;
    if (ImGui::IsItemDeactivated() && !dragged_)
    {
        const double ws = worldSize();
        const double x = cx_ + (io.MousePos.x - mid.x) / ws;
        pickX_ = x - std::floor(x);
        pickY_ = std::clamp(cy_ + (io.MousePos.y - mid.y) / ws, 0.0, 1.0);
        hasPick_ = true;
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
    {
        const double ws = worldSize();
        cx_ -= io.MouseDelta.x / ws;
        cy_ -= io.MouseDelta.y / ws;
    }
    if (hovered && io.MouseWheel != 0.0f)
        zoomAround(io.MousePos, mid, io.MouseWheel * 0.25);
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        zoomAround(io.MousePos, mid, 1.0);

    cx_ -= std::floor(cx_);  // wrap around the date line
    cy_ = std::clamp(cy_, 0.0, 1.0);

    // --- Tiles ---
    const double ws = worldSize();
    const int tz = std::clamp(int(std::lround(zoom_)), 0, tiles_->source().maxZoom);
    const int n = 1 << tz;

    const double left = cx_ - (mid.x - p0.x) / ws;
    const double right = cx_ + (p1.x - mid.x) / ws;
    const double top = cy_ - (mid.y - p0.y) / ws;
    const double bottom = cy_ + (p1.y - mid.y) / ws;
    const int tx0 = int(std::floor(left * n));
    const int tx1 = int(std::floor(right * n));
    const int ty0 = std::max(0, int(std::floor(top * n)));
    const int ty1 = std::min(n - 1, int(std::floor(bottom * n)));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(p0, p1, true);
    dl->AddRectFilled(p0, p1, IM_COL32(24, 28, 34, 255));

    auto toScreenX = [&](double x) { return float(std::floor(mid.x + (x - cx_) * ws)); };
    auto toScreenY = [&](double y) { return float(std::floor(mid.y + (y - cy_) * ws)); };

    for (int ty = ty0; ty <= ty1; ++ty)
    {
        for (int tx = tx0; tx <= tx1; ++tx)
        {
            const int wx = ((tx % n) + n) % n;  // horizontal wrap
            const ImVec2 a(toScreenX(double(tx) / n), toScreenY(double(ty) / n));
            const ImVec2 b(toScreenX(double(tx + 1) / n), toScreenY(double(ty + 1) / n));
            drawTile(dl, TileKey{tz, wx, ty}, a, b);
        }
    }

    // --- Overlays ---
    if (hasPick_)
        drawPin(dl, mid, ws);
    if (hasVehicle_)
        drawVehicle(dl, mid, ws);

    char buf[128];
    if (hovered)
    {
        double mx = cx_ + (io.MousePos.x - mid.x) / ws;
        mx -= std::floor(mx);
        const double my = std::clamp(cy_ + (io.MousePos.y - mid.y) / ws, 0.0, 1.0);
        snprintf(buf, sizeof(buf), "%.6f, %.6f   z %.2f", yToLat(my), xToLon(mx), zoom_);
        label(dl, ImVec2(p0.x + 8, p1.y - ImGui::GetTextLineHeight() - 6), buf);
    }

    const char* attribution = tiles_->source().attribution;
    const float aw = ImGui::CalcTextSize(attribution).x;
    label(dl, ImVec2(p1.x - aw - 8, p1.y - ImGui::GetTextLineHeight() - 6), attribution);

    dl->PopClipRect();
}

void MapWidget::drawTile(ImDrawList* dl, const TileKey& key, ImVec2 a, ImVec2 b)
{
    if (GLuint tex = tiles_->getTexture(key, true))
    {
        dl->AddImage(toImTexture(tex), a, b);
        return;
    }

    // Not loaded yet: stretch the matching part of an already loaded lower-zoom tile.
    for (int d = 1; d <= 8 && key.z - d >= 0; ++d)
    {
        const TileKey parent{key.z - d, key.x >> d, key.y >> d};
        const GLuint tex = tiles_->getTexture(parent, false);
        if (!tex)
            continue;
        const float s = 1.0f / float(1 << d);
        const ImVec2 uv0(float(key.x - (parent.x << d)) * s, float(key.y - (parent.y << d)) * s);
        dl->AddImage(toImTexture(tex), a, b, uv0, ImVec2(uv0.x + s, uv0.y + s));
        return;
    }
}

double MapWidget::pickedLat() const { return yToLat(pickY_); }
double MapWidget::pickedLon() const { return xToLon(pickX_); }

ImVec2 MapWidget::worldToScreen(double x, double y, ImVec2 mid, double ws) const
{
    double dx = x - cx_;
    dx -= std::round(dx);  // the copy closest to the view center
    return ImVec2(float(mid.x + dx * ws), float(mid.y + (y - cy_) * ws));
}

void MapWidget::drawPin(ImDrawList* dl, ImVec2 mid, double ws)
{
    const ImVec2 p = worldToScreen(pickX_, pickY_, mid, ws);

    // Teardrop pin whose tip is exactly on the picked point
    const ImVec2 head(p.x, p.y - 22.0f);
    const ImU32 fill = IM_COL32(40, 120, 230, 255);
    dl->AddTriangleFilled(ImVec2(p.x - 6.5f, head.y + 4.0f), ImVec2(p.x + 6.5f, head.y + 4.0f), p, fill);
    dl->AddCircleFilled(head, 8.0f, fill);
    dl->AddCircleFilled(head, 3.0f, IM_COL32(255, 255, 255, 255));
    dl->AddCircle(p, 3.0f, IM_COL32(255, 255, 255, 220), 0, 1.5f);

    char buf[64];
    snprintf(buf, sizeof(buf), "%.6f, %.6f", pickedLat(), pickedLon());
    label(dl, ImVec2(p.x + 14.0f, head.y - ImGui::GetTextLineHeight() * 0.5f), buf);
}

void MapWidget::drawVehicle(ImDrawList* dl, ImVec2 mid, double ws)
{
    const ImVec2 c = worldToScreen(lonToX(vehicleLon_), latToY(vehicleLat_), mid, ws);

    const float h = vehicleHeading_ * float(kPi / 180.0);
    const float cs = std::cos(h), sn = std::sin(h);
    auto pt = [&](float x, float y) { return ImVec2(c.x + x * cs - y * sn, c.y + x * sn + y * cs); };

    const ImVec2 tip = pt(0, -16), right = pt(10, 11), notch = pt(0, 5), left = pt(-10, 11);
    const ImU32 fill = IM_COL32(230, 60, 50, 255);
    dl->AddTriangleFilled(tip, right, notch, fill);
    dl->AddTriangleFilled(tip, notch, left, fill);
    const ImVec2 outline[] = {tip, right, notch, left};
    dl->AddPolyline(outline, 4, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 2.0f);
}
