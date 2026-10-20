#include "rain_radar.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace esphome::rain_radar {

using tessera::Font;
namespace ui = tessera::ui;

// ---------------------------------------------------------------------------------------------------------------------
// Where the pictures come from (all of it also named in the manifest, permissions.network). The screen fetches nothing:
// it names an address for each place and the Tessera app fetches it from one of those hosts.
//
//  * The map: the standard OpenStreetMap tile layer, https://tile.openstreetmap.org/{z}/{x}/{y}.png
//  * The radar: https://tilecache.rainviewer.com + the `path` of the newest frame in RainViewer's weather-maps.json,
//    used exactly as given, then /{size}/{z}/{x}/{y}/{color}/{smooth}_{snow}.png (RainViewer's Weather Maps API).
//    Colour scheme 2 (Universal Blue, the only one RainViewer lists) and zoom 7 at most (RainViewer's limit).
// ---------------------------------------------------------------------------------------------------------------------
static const char *const OSM_HOST = "https://tile.openstreetmap.org";
static const char *const RADAR_HOST = "https://tilecache.rainviewer.com";
static const int MIN_ZOOM = 2;
static const int MAX_ZOOM = 7;        // RainViewer's limit for radar tiles
static const int GRID = 2;            // the map is GRID x GRID tiles; 2 x 2 per layer fits the 8 places of the tile
static const int MAX_AGE = 3600;      // a frame older than an hour is called old: RainViewer lists two hours at most

// A text of a coordinate as a number; a comma counts as the decimal point. False when it is not a number.
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

// The path of a frame as RainViewer gives it ("/v2/radar/..."): kept only when it is made of the characters of a path,
// so a strange answer can never become a strange address.
static bool path_ok(const std::string &path) {
  if (path.size() < 2 || path.size() > 47 || path[0] != '/') return false;
  for (char ch : path) {
    const bool good = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '/' || ch == '_' ||
                      ch == '-' || ch == '.';
    if (!good) return false;
  }
  return path.find("..") == std::string::npos && path.find("//") == std::string::npos;
}

// The map and its words, drawn the same on the tile and on the card that a tap opens: 2 x 2 OpenStreetMap tiles with the
// newest rain radar over them, a dot on the chosen coordinates, the time of the radar frame under them, and the credits
// RainViewer and OpenStreetMap ask for. build() makes every part once; frame() names the radar frame; tick() writes the
// words; restyle() paints the look again.
//
// How the map is laid out. A place for a picture must lie wholly within the card and shows its picture whole (it cannot
// be cut at an offset), so the map is whole tiles: the block of 2 x 2 whose middle is nearest the coordinates, every tile
// a square of the same size, the radar's tiles of the same grid exactly over the map's, so they always join. The
// coordinates fall inside the block at (px, py) tiles from its corner, px and py between about 0.5 and 1.5.
//  * "Keep the coordinates in the middle" (default): the block is moved so that the coordinates are the middle of the
//    card's map room, as large as it can be and still lie within the card. What lies past the same distance on every
//    side of the coordinates is covered with the card's own colour, so the map is a window around them: the coordinates
//    are its exact centre. The window is between a third and the whole of the room, as the coordinates fall in their tile.
//  * Off: the block fills the room and the coordinates lie where they fall; the dot shows the place.
class Panel {
 public:
  void build(lv_obj_t *parent, int width, int height, const Config &config, const tessera::Plugin *plugin, int radar_size) {
    plugin_ = plugin;
    cfg_ = config;
    // Always fill the actual Tessera tile/card. Preserve the map aspect ratio by digitally zooming
    // (cover), rather than shrinking a square 2x2 tile block and leaving empty card space.
    cfg_.center = false;
    width_ = width;
    height_ = height;
    radar_size_ = radar_size;

    // The credits, wrapped over as many lines as the width needs. RainViewer: "Weather data by RainViewer";
    // OpenStreetMap: "(c) OpenStreetMap contributors", which the screen cannot link, so the README does.
    const std::string credit = cfg_.show_map ? plugin_->text("credit_both") : plugin_->text("credit_radar");
    const int lines = std::min(3, std::max(1, static_cast<int>(std::ceil(ui::text_width(credit, Font::BODY) * 1.15 / std::max(1, width_)))));
    const int body = ui::line_height(Font::BODY);
    const int credit_h = lines * body;
    const int gap = ui::px(4);

    // The room for the map: above the status line and the credits.
    const int room_h = std::max(1, height_ - credit_h - body - 2 * gap);
    const bool pictures = ui::pictures();
    map_ = pictures && cfg_.has_position && std::min(width_, room_h) >= ui::mm(12);
    if (map_) {
      locate();
      if (!layout(room_h)) map_ = false;
    }

    int top = 0;
    if (map_) {
      // The layers, from the back to the front: the map, then the radar over it; each is 2 x 2 places.
      for (int layer = 0; layer < 2; ++layer) {
        if (layer == 0 && !cfg_.show_map) continue;
        for (int j = 0; j < GRID; ++j) {
          for (int i = 0; i < GRID; ++i) {
            lv_obj_t *place = ui::picture(parent);
            lv_obj_set_size(place, tile_, tile_);
            lv_obj_set_pos(place, x0_px_ + i * tile_, y0_px_ + j * tile_);
            lv_obj_set_style_radius(place, 0, 0);   // square corners: no card colour showing between the tiles
            (layer == 0 ? map_places_ : radar_places_)[j * GRID + i] = place;
          }
        }
      }
      // The map is light, whatever the screen's look: a dark veil over it (above the pictures) for the dark look. The
      // radar is part of the same picture, so it is dimmed with it.
      veil_ = cover(parent, theme::CAMERA_PAGE, x0_px_, y0_px_, GRID * tile_, GRID * tile_);
      // The window: what lies outside it is hidden under the card's own colour.
      if (cfg_.center) {
        const int mx1 = x0_px_ + GRID * tile_, my1 = y0_px_ + GRID * tile_;
        const int wl = cx_ - hx_, wr = cx_ + hx_, wt = cy_ - hy_, wb = cy_ + hy_;
        cover(parent, theme::CARD, x0_px_, y0_px_, wl - x0_px_, my1 - y0_px_);
        cover(parent, theme::CARD, wr, y0_px_, mx1 - wr, my1 - y0_px_);
        cover(parent, theme::CARD, wl, y0_px_, wr - wl, wt - y0_px_);
        cover(parent, theme::CARD, wl, wb, wr - wl, my1 - wb);
      }
      if (cfg_.marker) {
        const int ring = ui::mm(3), dot = ui::mm(2);
        ring_ = cover(parent, theme::CARD, dot_x_ - ring / 2, dot_y_ - ring / 2, ring, ring);
        if (ring_) lv_obj_set_style_radius(ring_, LV_RADIUS_CIRCLE, 0);
        dot_ = cover(parent, theme::ACCENT, dot_x_ - dot / 2, dot_y_ - dot / 2, dot, dot);
        if (dot_) lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
      }
      top = room_h + gap;
    } else {
      // No pictures on this screen (the CYD and the small panels), no coordinates yet, or a tile too small for the map:
      // say so in words.
      note_ = ui::text(parent, Font::BODY, theme::MUTED);
      lv_obj_set_size(note_, width_, room_h);
      lv_obj_set_pos(note_, 0, 0);
      top = room_h + gap;
    }

    status_ = ui::label(parent, Font::BODY, theme::INK);
    lv_obj_set_width(status_, width_);
    lv_obj_set_pos(status_, 0, top);
    credit_ = ui::text(parent, Font::BODY, theme::MUTED);
    lv_obj_set_size(credit_, width_, credit_h);
    lv_obj_set_pos(credit_, 0, top + body + gap / 2);
    ui::set_text(credit_, credit);
    note_text_ = !pictures ? plugin_->text("no_pictures") : cfg_.has_position ? plugin_->text("too_small") : "";
    paint_look();
    paint_maps();
  }

  // The newest radar frame (its path as RainViewer gave it) and when it is from, and why there is none.
  void frame(const std::string &path, uint32_t at, const std::string &wait) {
    if (path_ok(path)) {
      path_ = path.c_str();
      frame_at_ = at;
    }
    wait_ = wait.c_str();
    paint_radar();
  }

  void tick(uint32_t epoch) {
    // The line under the map: the time of the radar frame, or what is missing.
    std::string line;
    if (!cfg_.has_position) {
      line = plugin_->text("no_position");
    } else if (!path_.empty() && frame_at_) {
      line = tessera::fill(plugin_->text("radar_at"), "time", tessera::clock_text(frame_at_));
      if (epoch && epoch > frame_at_ && epoch - frame_at_ > static_cast<uint32_t>(MAX_AGE)) line = plugin_->text("radar_old");
    } else if (wait_ == "failed" || wait_ == "refused" || wait_ == "too_large") {
      line = plugin_->text("radar_failed");
    } else {
      line = plugin_->text("radar_loading");
    }
    ui::set_text(status_, line);
    if (note_) {
      std::string note;
      if (cfg_.has_position) {
        char where[40];
        std::snprintf(where, sizeof(where), "%.3f, %.3f", cfg_.lat, cfg_.lon);
        note = std::string(where) + (note_text_.empty() ? "" : "\n" + roomy::plain(note_text_));
      }
      ui::set_text(note_, note);
    }
  }

  // Light and dark: the theme roles have other values now, and the map's veil follows the look.
  void restyle() {
    ui::set_color(status_, theme::INK);
    ui::set_color(credit_, theme::MUTED);
    if (note_) ui::set_color(note_, theme::MUTED);
    paint_look();
  }

 private:
  // A plain rectangle of a theme role over the pictures: not clickable, square unless asked. Null when it has no size.
  static lv_obj_t *cover(lv_obj_t *parent, theme::Role role, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return nullptr;
    lv_obj_t *b = ui::block(parent, role);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(b, 0, 0);
    return b;
  }

  // The tile of the map under the chosen coordinates (Web Mercator, as OpenStreetMap and RainViewer cut it): the block of
  // GRID x GRID tiles whose middle is nearest to them, and where they lie in it, in tiles from its corner.
  void locate() {
    const double pi = 3.14159265358979323846;
    const int count = 1 << cfg_.zoom;
    const double lat = std::max(-85.0511, std::min(85.0511, cfg_.lat)) * pi / 180.0;
    const double x = (cfg_.lon + 180.0) / 360.0 * count;
    const double y = (1.0 - std::asinh(std::tan(lat)) / pi) / 2.0 * count;
    const double first = std::floor(x - GRID / 2.0 + 0.5);   // the block's first column, before the wrap round the world
    const int y0 = std::max(0, std::min(count - GRID, static_cast<int>(std::floor(y - GRID / 2.0 + 0.5))));
    x0_ = ((static_cast<int>(first) % count) + count) % count;
    y0_ = y0;
    px_ = std::max(0.0, std::min(static_cast<double>(GRID), x - first));
    py_ = std::max(0.0, std::min(static_cast<double>(GRID), y - y0));
  }

  // Size the square 2x2 source block to COVER the available area. This is a digital zoom: the
  // source map keeps its proportions, while the excess beyond the card edges is clipped by LVGL.
  // Width and height come from Tessera at runtime, so this works for tiles and fullscreen cards.
  bool layout(int room_h) {
    cx_ = width_ / 2;
    cy_ = room_h / 2;
    // Each source tile is square and the source block is 2x2. Use the larger dimension so
    // the block always covers the whole target, regardless of its aspect ratio. Rounding up
    // avoids a one-pixel seam at the far edge. The excess is intentionally cropped, not stretched.
    tile_ = static_cast<int>(std::ceil(std::max(width_, room_h) / static_cast<double>(GRID)));
    if (tile_ < 1) return false;
    x0_px_ = (width_ - GRID * tile_) / 2;
    y0_px_ = (room_h - GRID * tile_) / 2;
    dot_x_ = x0_px_ + static_cast<int>(std::lround(px_ * tile_));
    dot_y_ = y0_px_ + static_cast<int>(std::lround(py_ * tile_));
    hx_ = width_ / 2;
    hy_ = room_h / 2;
    return true;
  }

  std::string tile_path(int i, int j) const {
    const int count = 1 << cfg_.zoom;
    return std::to_string(cfg_.zoom) + "/" + std::to_string((x0_ + i) % count) + "/" + std::to_string(y0_ + j);
  }

  // The map's addresses depend on the options only, so they are set once.
  void paint_maps() {
    if (!map_ || !cfg_.show_map) return;
    for (int j = 0; j < GRID; ++j)
      for (int i = 0; i < GRID; ++i)
        ui::set_picture(map_places_[j * GRID + i], std::string(OSM_HOST) + "/" + tile_path(i, j) + ".png", tessera::Fit::COVER);
  }

  // The radar's addresses: the newest frame's path as RainViewer gave it, the size, the zoom, the tile, the colour scheme
  // (2, Universal Blue) and the options smooth and snow.
  void paint_radar() {
    if (!map_ || path_.empty()) return;
    const std::string path = roomy::plain(path_);
    const std::string options = std::string(cfg_.smooth ? "1" : "0") + "_" + (cfg_.snow ? "1" : "0");
    for (int j = 0; j < GRID; ++j)
      for (int i = 0; i < GRID; ++i)
        ui::set_picture(radar_places_[j * GRID + i],
                        std::string(RADAR_HOST) + path + "/" + std::to_string(radar_size_) + "/" + tile_path(i, j) + "/2/" + options + ".png",
                        tessera::Fit::COVER);
  }

  // The dark veil: over the light map when the look is dark (the screen's own, or the one chosen in the tile). OpenStreetMap
  // has no dark map, and the providers that have one ask for an account key, so the light map is dimmed.
  void paint_look() {
    if (!veil_) return;
    const bool dark = cfg_.look == Look::DARK || (cfg_.look == Look::AUTO && ::theme::dark);
    lv_obj_set_style_bg_opa(veil_, dark ? LV_OPA_70 : LV_OPA_TRANSP, 0);
  }

  const tessera::Plugin *plugin_ = nullptr;
  Config cfg_;
  int width_ = 0, height_ = 0, tile_ = 0, x0_ = 0, y0_ = 0, radar_size_ = 256;
  int x0_px_ = 0, y0_px_ = 0, cx_ = 0, cy_ = 0, hx_ = 0, hy_ = 0, dot_x_ = 0, dot_y_ = 0;
  bool map_ = false;
  double px_ = 1, py_ = 1;
  uint32_t frame_at_ = 0;
  tessera::Text path_, wait_, note_text_;   // kept as long as the card: PSRAM first (docs/FIRMWARE_API.md, "Memory")
  lv_obj_t *map_places_[GRID * GRID]{}, *radar_places_[GRID * GRID]{};
  lv_obj_t *veil_{}, *ring_{}, *dot_{}, *status_{}, *credit_{}, *note_{};
};

// The options of a tile, with the defaults and limits applied.
static Config read_config(JsonObjectConst options) {
  Config c;
  c.show_map = options["map"] | true;
  c.marker = options["marker"] | true;
  c.smooth = options["smooth"] | true;
  c.snow = options["snow"] | true;
  const std::string look = options["look"] | "auto";
  c.look = look == "dark" ? Look::DARK : look == "light" ? Look::LIGHT : Look::AUTO;
  c.zoom = std::max(MIN_ZOOM, std::min(MAX_ZOOM, static_cast<int>(options["zoom"] | 6)));
  c.center = options["center"] | false;
  c.fullscreen = options["fullscreen"] | true;
  c.has_position = parse_degrees(options["lat"] | "", 85.0, &c.lat) && parse_degrees(options["lon"] | "", 180.0, &c.lon);
  return c;
}

// The tile: the map on the card of the page. When its option "Open full screen on tap" is on, a tap opens the card, the
// same map over the whole glass (as a camera tile of Tessera does).
class RadarTile : public tessera::Tile {
 public:
  explicit RadarTile(RainRadar *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    config_ = read_config(c.options);
    tile_ = c.tile;
    panel_.build(c.parent, c.width, c.height, config_, plugin_, 256);
  }
  void on_state(JsonObjectConst data) override {
    const std::string path = data["items"][0]["path"] | "";
    const uint32_t at = data["items"][0]["at"] | 0u;
    panel_.frame(path, at, data["wait"] | "");
    if (path_ok(path)) {
      plugin_->frame_path = path.c_str();
      plugin_->frame_at = at;
    }
  }
  void on_tick(uint32_t epoch) override { panel_.tick(epoch); }
  void on_theme() override { panel_.restyle(); }
  void on_tap() override {
    if (!config_.has_position || !config_.fullscreen) return;
    plugin_->config = config_;
    tessera::open_card(plugin_->plugin_id(), "radar_card", "", tile_);
  }

 private:
  RainRadar *plugin_;
  Config config_;
  int tile_ = -1;
  Panel panel_;
};

// The card a tap on the tile opens: the same map as large as the glass allows (RainViewer's tiles at 512 pixels).
class RadarCard : public tessera::Card {
 public:
  explicit RadarCard(RainRadar *plugin) : plugin_(plugin) {}

  void open(const tessera::CardContext &c) override {
    panel_.build(c.parent, c.width, c.height, plugin_->config, plugin_, 512);
    panel_.frame(roomy::plain(plugin_->frame_path), plugin_->frame_at, "");
  }
  void on_state(JsonObjectConst data) override {
    const std::string path = data["items"][0]["path"] | "";
    panel_.frame(path, data["items"][0]["at"] | 0u, data["wait"] | "");
  }
  void on_tick(uint32_t epoch) override { panel_.tick(epoch); }
  void on_theme() override { panel_.restyle(); }

 private:
  RainRadar *plugin_;
  Panel panel_;
};

void RainRadar::setup() {
  // The ids are the manifest's: the core makes a RadarTile for every card of the page that shows one, and a RadarCard
  // when the tile opens it.
  add_tile("radar", [this]() { return new RadarTile(this); });
  add_card("radar_card", [this]() { return new RadarCard(this); });
}

}  // namespace esphome::rain_radar
