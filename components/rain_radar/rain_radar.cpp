#include "rain_radar.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace esphome::rain_radar {

using tessera::Font;
namespace ui = tessera::ui;

static const char *const OSM_HOST = "https://tile.openstreetmap.org";
static const char *const RADAR_HOST = "https://tilecache.rainviewer.com";
static const int MIN_ZOOM = 2;
static const int MAX_ZOOM = 7;
static const int GRID = 2;
static const int MAX_AGE = 3600;

static bool parse_degrees(const char *text, double limit, double *out) {
  std::string s = text ? text : "";
  for (char &ch : s)
    if (ch == ',') ch = '.';
  if (s.empty()) return false;
  char *end = nullptr;
  const double value = std::strtod(s.c_str(), &end);
  while (end && (*end == ' ' || *end == '\t')) ++end;
  if (!end || end == s.c_str() || *end != '\0' || !std::isfinite(value) || std::fabs(value) > limit) return false;
  *out = value;
  return true;
}

static bool path_ok(const std::string &path) {
  if (path.size() < 2 || path.size() > 47 || path[0] != '/') return false;
  for (char ch : path) {
    const bool good = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '/' || ch == '_' ||
                      ch == '-' || ch == '.';
    if (!good) return false;
  }
  return path.find("..") == std::string::npos && path.find("//") == std::string::npos;
}

class RadarTile : public tessera::Tile {
 public:
  explicit RadarTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    show_map_ = c.options["map"] | true;
    show_marker_ = c.options["marker"] | true;
    smooth_ = c.options["smooth"] | true;
    snow_ = c.options["snow"] | true;
    const std::string look = c.options["look"] | "auto";
    look_ = look == "dark" ? Look::DARK : look == "light" ? Look::LIGHT : Look::AUTO;
    zoom_ = c.options["zoom"] | 6;
    if (zoom_ < MIN_ZOOM) zoom_ = MIN_ZOOM;
    if (zoom_ > MAX_ZOOM) zoom_ = MAX_ZOOM;

    std::string zf_str = c.options["zoom_factor"] | "1.0";
    zoom_factor_ = std::strtod(zf_str.c_str(), nullptr);
    if (zoom_factor_ < 1.0) zoom_factor_ = 1.0;
    if (zoom_factor_ > 3.0) zoom_factor_ = 3.0;

    aspect_ = c.options["aspect"] | "auto";

    bool lat_ok_parse = parse_degrees(c.options["lat"] | "", 85.0, &lat_);
    bool lon_ok_parse = parse_degrees(c.options["lon"] | "", 180.0, &lon_);
    has_position_ = lat_ok_parse && lon_ok_parse;

    const std::string credit = show_map_ ? plugin_->text("credit_both") : plugin_->text("credit_radar");
    const int lines = std::min(2, std::max(1, static_cast<int>(std::ceil(ui::text_width(credit, Font::BODY) * 1.15 / std::max(1, width_)))));
    const int body = ui::line_height(Font::BODY);

    const bool is_small = (width_ < ui::mm(40) || height_ < ui::mm(40));
    const int credit_h = is_small ? 0 : lines * body;
    const int gap = is_small ? ui::px(1) : ui::px(4);

    const bool room = ui::pictures() && has_position_;
    int avail_h = std::max(10, height_ - (is_small ? body : (credit_h + body + 2 * gap)));

    map_w_ = width_;
    map_h_ = avail_h;

    if (aspect_ == "1:1") {
      int s = std::min(width_, avail_h);
      map_w_ = s;
      map_h_ = s;
    } else if (aspect_ == "4:3") {
      if (width_ * 3 > avail_h * 4) {
        map_h_ = avail_h;
        map_w_ = avail_h * 4 / 3;
      } else {
        map_w_ = width_;
        map_h_ = width_ * 3 / 4;
      }
    } else if (aspect_ == "16:9") {
      if (width_ * 9 > avail_h * 16) {
        map_h_ = avail_h;
        map_w_ = avail_h * 16 / 9;
      } else {
        map_w_ = width_;
        map_h_ = width_ * 9 / 16;
      }
    }

    tile_w_ = std::max(1, map_w_ / GRID);
    tile_h_ = std::max(1, map_h_ / GRID);
    left_ = (width_ - map_w_) / 2;
    top_ = 0;

    map_ = room && map_w_ >= ui::px(32) && map_h_ >= ui::px(32);

    int top_offset = 0;
    if (map_) {
      locate();

      for (int layer = 0; layer < 2; ++layer) {
        if (layer == 0 && !show_map_) continue;
        for (int j = 0; j < GRID; ++j) {
          for (int i = 0; i < GRID; ++i) {
            lv_obj_t *place = ui::picture(c.parent);
            lv_obj_set_size(place, tile_w_, tile_h_);
            lv_obj_set_pos(place, left_ + i * tile_w_, top_ + j * tile_h_);
            (layer == 0 ? map_places_ : radar_places_)[j * GRID + i] = place;
          }
        }
      }

      veil_ = ui::block(c.parent, theme::CAMERA_PAGE);
      lv_obj_set_size(veil_, map_w_, map_h_);
      lv_obj_set_pos(veil_, left_, top_);
      lv_obj_remove_flag(veil_, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(veil_, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_radius(veil_, 0, 0);

      if (show_marker_) {
        const int ring = ui::mm(3), dot = ui::mm(2);
        const int cx = left_ + static_cast<int>(std::lround(dx_ * map_w_));
        const int cy = top_ + static_cast<int>(std::lround(dy_ * map_h_));

        ring_ = ui::block(c.parent, theme::CARD);
        lv_obj_set_size(ring_, ring, ring);
        lv_obj_set_pos(ring_, cx - ring / 2, cy - ring / 2);
        lv_obj_set_style_radius(ring_, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(ring_, LV_OBJ_FLAG_CLICKABLE);

        dot_ = ui::block(c.parent, theme::ACCENT);
        lv_obj_set_size(dot_, dot, dot);
        lv_obj_set_pos(dot_, cx - dot / 2, cy - dot / 2);
        lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(dot_, LV_OBJ_FLAG_CLICKABLE);
      }
      top_offset = top_ + map_h_ + gap;
    } else {
      note_ = ui::text(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_size(note_, width_, std::max(body, height_ - credit_h - body - 2 * gap));
      lv_obj_set_pos(note_, 0, 0);
      top_offset = std::max(body, height_ - credit_h - body - 2 * gap) + gap;
    }

    status_ = ui::label(c.parent, Font::BODY, theme::INK);
    lv_obj_set_width(status_, width_);
    lv_obj_set_pos(status_, 0, top_offset);

    if (!is_small) {
      credit_ = ui::text(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_size(credit_, width_, credit_h);
      lv_obj_set_pos(credit_, 0, top_offset + body + gap / 2);
      ui::set_text(credit_, credit);
    }

    place_text_ = ui::pictures() ? "" : plugin_->text("no_pictures");
    paint_look();
    paint_maps();
  }

  void on_state(JsonObjectConst data) override {
    const std::string path = data["items"][0]["path"] | "";
    const uint32_t at = data["items"][0]["at"] | 0u;
    const std::string wait = data["wait"] | "";
    if (path_ok(path)) {
      path_ = path.c_str();
      frame_at_ = at;
    }
    wait_ = wait.c_str();
    paint_radar();
  }

  void on_tick(uint32_t epoch) override {
    std::string line;
    if (!has_position_) {
      line = "Error: Invalid or Missing Coordinates";
    } else if (!path_.empty() && frame_at_) {
      line = tessera::fill(plugin_->text("radar_at"), "time", tessera::clock_text(frame_at_));
      if (epoch && epoch > frame_at_ && epoch - frame_at_ > static_cast<uint32_t>(MAX_AGE)) {
        line = "Warning: Radar Data Out of Date";
      }
    } else if (wait_ == "failed" || wait_ == "refused" || wait_ == "too_large") {
      line = "API Error: Radar Unavailable (" + std::string(wait_) + ")";
    } else if (wait_ == "timeout" || wait_ == "error") {
      line = "Network Error: Failed to Fetch Radar";
    } else {
      line = "Loading Radar Data...";
    }
    
    if (map_) {
      ui::set_text(status_, line);
    } else {
      ui::set_text(status_, line);
      std::string note;
      if (has_position_) {
        char where[40];
        std::snprintf(where, sizeof(where), "%.3f, %.3f", lat_, lon_);
        note = std::string(where) + (place_text_.empty() ? "" : "\n" + roomy::plain(place_text_));
      } else {
        note = "Please configure valid Latitude and Longitude.";
      }
      if (note_) ui::set_text(note_, note);
    }
  }

  void on_theme() override {
    ui::set_color(status_, theme::INK);
    if (credit_) ui::set_color(credit_, theme::MUTED);
    if (note_) ui::set_color(note_, theme::MUTED);
    paint_look();
  }

 private:
  enum class Look { AUTO, LIGHT, DARK };

  void locate() {
    const double pi = 3.14159265358979323846;
    const double n = static_cast<double>(1 << zoom_);
    const double lat = std::max(-85.0511, std::min(85.0511, lat_)) * pi / 180.0;
    const double x = (lon_ + 180.0) / 360.0 * n;
    const double y = (1.0 - std::asinh(std::tan(lat)) / pi) / 2.0 * n;
    int x0 = static_cast<int>(std::floor(x - GRID / 2.0 + 0.5));
    int y0 = static_cast<int>(std::floor(y - GRID / 2.0 + 0.5));
    const int count = 1 << zoom_;
    if (y0 < 0) y0 = 0;
    if (y0 > count - GRID) y0 = count - GRID;
    x0 = ((x0 % count) + count) % count;
    x0_ = x0;
    y0_ = y0;
    dx_ = (x - std::floor(x - GRID / 2.0 + 0.5)) / GRID;
    dy_ = (y - static_cast<double>(y0)) / GRID;
    dx_ = std::max(0.0, std::min(1.0, dx_));
    dy_ = std::max(0.0, std::min(1.0, dy_));
  }

  std::string tile_path(int i, int j) const {
    const int count = 1 << zoom_;
    const int x = (x0_ + i) % count, y = y0_ + j;
    return std::to_string(zoom_) + "/" + std::to_string(x) + "/" + std::to_string(y);
  }

  void paint_maps() {
    if (!map_) return;
    for (int j = 0; j < GRID; ++j)
      for (int i = 0; i < GRID; ++i) {
        lv_obj_t *place = map_places_[j * GRID + i];
        if (place) ui::set_picture(place, std::string(OSM_HOST) + "/" + tile_path(i, j) + ".png", tessera::Fit::COVER);
      }
  }

  void paint_radar() {
    if (!map_ || path_.empty()) return;
    const std::string path = roomy::plain(path_);
    const std::string options = std::string(smooth_ ? "1" : "0") + "_" + (snow_ ? "1" : "0");
    for (int j = 0; j < GRID; ++j)
      for (int i = 0; i < GRID; ++i) {
        lv_obj_t *place = radar_places_[j * GRID + i];
        if (!place) continue;
        ui::set_picture(place, std::string(RADAR_HOST) + path + "/256/" + tile_path(i, j) + "/2/" + options + ".png", tessera::Fit::COVER);
      }
  }

  void paint_look() {
    if (!veil_) return;
    const bool dark = look_ == Look::DARK || (look_ == Look::AUTO && theme::dark);
    lv_obj_set_style_bg_opa(veil_, dark ? LV_OPA_50 : LV_OPA_TRANSP, 0);
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0, map_w_ = 0, map_h_ = 0, tile_w_ = 0, tile_h_ = 0, left_ = 0, top_ = 0, zoom_ = 6, x0_ = 0, y0_ = 0;
  double zoom_factor_ = 1.0;
  std::string aspect_ = "auto";
  bool show_map_ = true, show_marker_ = true, smooth_ = true, snow_ = true, has_position_ = false, map_ = false;
  Look look_ = Look::AUTO;
  double lat_ = 0, lon_ = 0, dx_ = 0.5, dy_ = 0.5;
  uint32_t frame_at_ = 0;
  tessera::Text path_, wait_;
  tessera::Text place_text_;
  lv_obj_t *map_places_[GRID * GRID]{}, *radar_places_[GRID * GRID]{};
  lv_obj_t *veil_{}, *ring_{}, *dot_{}, *status_{}, *credit_{}, *note_{};
};

void RainRadar::setup() {
  add_tile("radar", [this]() { return new RadarTile(this); });
}

}  // namespace esphome::rain_radar
