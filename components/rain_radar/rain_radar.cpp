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
static const int MAX_ICONS = 36;      // the backdrop's icons at most
static const double CENTRE_SHARE = 0.6;   // the window around centred coordinates must show this share of what the filling block shows
static const int MAX_CELLS = 4;       // a layer has at most 4 places (2 layers fit the 8 places of a tile)
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
//    are its exact centre. The window can be much smaller than the room (the nearest edge of the block limits it), so it
//    is used only when it shows at least CENTRE_SHARE of what the filling block would; otherwise the block fills the room.
//  * Off, or too small a window: the block (2 x 2, or 3 x 1 and so on, whichever covers most of the room) fills the room
//    and the coordinates lie where they fall; the dot shows the place.
class Panel {
 public:
  void build(lv_obj_t *parent, int width, int height, const Config &config, const tessera::Plugin *plugin, int radar_size) {
    plugin_ = plugin;
    cfg_ = config;
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
      // The shape that fills the room best (3 x 1, 2 x 2, ...), and, if asked, the block of 2 x 2 with the coordinates in
      // its middle: that one is taken only when its window is not much smaller than what the filling block shows.
      const double fill_area = pick_grid(room_h);
      const int fill_cols = cols_, fill_rows = rows_;
      bool centred = false;
      if (cfg_.center) {
        cols_ = rows_ = 2;
        locate();
        centred = layout_centred(room_h) && (!cfg_.center_auto || 4.0 * hx_ * hy_ >= CENTRE_SHARE * fill_area);
      }
      if (!centred) {
        cols_ = fill_cols;
        rows_ = fill_rows;
        locate();
        if (!layout_fill(room_h)) map_ = false;
      }
      cfg_.center = centred;
    }

    int top = 0;
    if (map_) {
      // What the map covers: the window (coordinates in the middle) or the block of tiles.
      int mx0 = x0_px_, my0 = y0_px_, mx1 = x0_px_ + cols_ * tile_, my1 = y0_px_ + rows_ * tile_;
      if (cfg_.center) {
        mx0 = cx_ - hx_;
        mx1 = cx_ + hx_;
        my0 = cy_ - hy_;
        my1 = cy_ + hy_;
      }
      // The ground under everything, in the colour of the look the map has (the card's own when it follows the screen).
      ground_ = cover(parent, ground_role(), 0, 0, width_, room_h);
      // The layers, from the back to the front: the map, then the radar over it; each is cols x rows places.
      for (int layer = 0; layer < 2; ++layer) {
        if (layer == 0 && !cfg_.show_map) continue;
        for (int j = 0; j < rows_; ++j) {
          for (int i = 0; i < cols_; ++i) {
            lv_obj_t *place = ui::picture(parent);
            lv_obj_set_size(place, tile_, tile_);
            lv_obj_set_pos(place, x0_px_ + i * tile_, y0_px_ + j * tile_);
            lv_obj_set_style_radius(place, 0, 0);   // square corners: no card colour showing between the tiles
            (layer == 0 ? map_places_ : radar_places_)[j * cols_ + i] = place;
          }
        }
      }
      // The window: what lies outside it is hidden under the card's colour, so the coordinates are its exact centre.
      if (cfg_.center) {
        const int bx1 = x0_px_ + cols_ * tile_, by1 = y0_px_ + rows_ * tile_;
        const int wl = cx_ - hx_, wr = cx_ + hx_, wt = cy_ - hy_, wb = cy_ + hy_;
        masks_[0] = cover(parent, ground_role(), x0_px_, y0_px_, wl - x0_px_, by1 - y0_px_);
        masks_[1] = cover(parent, ground_role(), wr, y0_px_, bx1 - wr, by1 - y0_px_);
        masks_[2] = cover(parent, ground_role(), wl, y0_px_, wr - wl, wt - y0_px_);
        masks_[3] = cover(parent, ground_role(), wl, wb, wr - wl, by1 - wb);
      }
      // The backdrop: weather icons in the free part of the room, at various angles, none over the map. It is made after
      // the masks of the window, which would hide what lay under them.
      decorate(parent, room_h, mx0, my0, mx1, my1);
      // The map is light, whatever the screen's look: a dark veil over the map alone (not the backdrop) for the dark
      // look. The radar is part of the same picture, so it is dimmed with it.
      veil_ = cover(parent, theme::CAMERA_PAGE, mx0, my0, mx1 - mx0, my1 - my0);
      if (cfg_.marker) {
        const uint32_t glyph = marker_glyph(cfg_.marker_icon);
        if (glyph) {
          // An icon of the person's choice, in the colour chosen. A pin's tip is on the coordinates, any other icon's middle.
          const int box = ui::line_height(Font::ICON);
          const bool pin = cfg_.marker_icon == "pin" || cfg_.marker_icon == "pin_outline" || cfg_.marker_icon == "home_pin";
          icon_ = ui::label(parent, Font::ICON, theme::INK);
          lv_obj_set_size(icon_, box, box);
          lv_obj_set_pos(icon_, dot_x_ - box / 2, dot_y_ - box / 2 - (pin ? box / 2 : 0));
          lv_obj_remove_flag(icon_, LV_OBJ_FLAG_CLICKABLE);
          lv_obj_set_style_text_align(icon_, LV_TEXT_ALIGN_CENTER, 0);
          ui::set_text(icon_, ::tessera::ui::icon(glyph));
        } else {
          const int ring = ui::mm(3), dot = ui::mm(2);
          ring_ = cover(parent, cfg_.marker_color == "white" ? theme::CAMERA_PAGE : theme::CARD, dot_x_ - ring / 2, dot_y_ - ring / 2, ring, ring);
          if (ring_) lv_obj_set_style_radius(ring_, LV_RADIUS_CIRCLE, 0);
          dot_ = cover(parent, theme::ACCENT, dot_x_ - dot / 2, dot_y_ - dot / 2, dot, dot);
          if (dot_) lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
        }
        paint_marker();
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
    if (cfg_.tiles && map_) line += "  ·  " + std::to_string(cfg_.zoom) + "/" + std::to_string(x0_) + "/" + std::to_string(y0_);
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
    paint_backdrop();
    paint_marker();
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

  // A backdrop of Tessera's weather icons over what the map does not cover (the free part of the room), in the quiet
  // colours of the theme, each at its own angle and in one of two sizes. The places are thrown one by one with a fixed
  // seed (so the backdrop is the same on every screen and does not jump), each at least a little way from the others
  // and wholly inside the room, whatever the turn, and none over the map. Nothing here reaches past the room: an object
  // that did, on a tile that touches the screen's edge, once crashed a screen.
  void decorate(lv_obj_t *parent, int room_h, int mx0, int my0, int mx1, int my1) {
    const int big = ui::line_height(Font::ICON), small = ui::line_height(Font::ICON_SMALL);
    if (big < 4 || big * 2 > std::min(width_, room_h)) return;   // a room this small has no space for a backdrop
    lv_obj_t *deco = cover(parent, theme::CARD, 0, 0, width_, room_h);
    if (!deco) return;
    lv_obj_set_style_bg_opa(deco, LV_OPA_TRANSP, 0);
    static const uint32_t ICONS[] = {0xF0596, 0xF0597, 0xF0590, 0xF0595, 0xF0599, 0xF0598, 0xF059D, 0xF067E, 0xF0593, 0xF0591, 0xF0592, 0xF067F};
    struct Spot {
      int x, y, r;
    };
    Spot spots[MAX_ICONS];
    int n = 0;
    uint32_t seed = 2463534242u ^ static_cast<uint32_t>(width_ * 7919 + room_h * 104729);
    auto next = [&seed](int bound) {
      seed = seed * 1664525u + 1013904223u;
      return static_cast<int>((seed >> 8) % static_cast<uint32_t>(std::max(1, bound)));
    };
    // Enough icons for the free area, at most MAX_ICONS (each costs a little memory).
    const long free_area = static_cast<long>(width_) * room_h - static_cast<long>(mx1 - mx0) * (my1 - my0);
    const int wanted = static_cast<int>(std::min<long>(MAX_ICONS, free_area / (static_cast<long>(big) * big * 3)));
    for (int attempt = 0; attempt < 700 && n < wanted; ++attempt) {
      const bool is_big = next(3) != 0;
      const int box = is_big ? big : small;
      const int r = box * 3 / 4;   // the radius of the circle the turned icon stays within
      if (width_ < 2 * r || room_h < 2 * r) continue;
      const int cx = r + next(width_ - 2 * r + 1), cy = r + next(room_h - 2 * r + 1);
      if (cx + r > mx0 && cx - r < mx1 && cy + r > my0 && cy - r < my1) continue;   // over the map
      bool free_spot = true;
      for (int k = 0; k < n && free_spot; ++k) {
        const long dx = cx - spots[k].x, dy = cy - spots[k].y, min = (r + spots[k].r) * 115L / 100;
        free_spot = dx * dx + dy * dy >= min * min;
      }
      if (!free_spot) continue;
      spots[n++] = {cx, cy, r};
      lv_obj_t *icon = ui::label(deco, is_big ? Font::ICON : Font::ICON_SMALL, icon_role());
      icons_[n - 1] = icon;
      icon_count_ = n;
      lv_obj_set_size(icon, box, box);
      lv_obj_set_pos(icon, cx - box / 2, cy - box / 2);
      lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
      lv_obj_set_style_transform_pivot_x(icon, box / 2, 0);
      lv_obj_set_style_transform_pivot_y(icon, box / 2, 0);
      lv_obj_set_style_transform_rotation(icon, (next(91) - 45) * 10, 0);   // -45 to 45 degrees, in tenths
      ui::set_text(icon, ::tessera::ui::icon(ICONS[next(12)]));
    }
  }

  // The tile of the map under the chosen coordinates (Web Mercator, as OpenStreetMap and RainViewer cut it): the block of
  // cols_ x rows_ tiles whose middle is nearest to them, and where they lie in it, in tiles from its corner.
  void locate() {
    const double pi = 3.14159265358979323846;
    const int count = 1 << cfg_.zoom;
    const double lat = std::max(-85.0511, std::min(85.0511, cfg_.lat)) * pi / 180.0;
    const double x = (cfg_.lon + 180.0) / 360.0 * count;
    const double y = (1.0 - std::asinh(std::tan(lat)) / pi) / 2.0 * count;
    const double first = std::floor(x - cols_ / 2.0 + 0.5);   // the block's first column, before the wrap round the world
    const int y0 = std::max(0, std::min(count - rows_, static_cast<int>(std::floor(y - rows_ / 2.0 + 0.5))));
    x0_ = ((static_cast<int>(first) % count) + count) % count;
    y0_ = y0;
    px_ = std::max(0.0, std::min(static_cast<double>(cols_), x - first));
    py_ = std::max(0.0, std::min(static_cast<double>(rows_), y - y0));
  }

  // The shape of the block that fills the room best, of at most 4 tiles: 2 x 2, or 3 x 1, 4 x 1 and their upright forms for
  // a room much wider or taller than it is high or wide. Sets cols_ and rows_ and returns the area it covers.
  double pick_grid(int room_h) {
    static const int shapes[][2] = {{2, 2}, {3, 1}, {1, 3}, {4, 1}, {1, 4}, {2, 1}, {1, 2}};
    cols_ = rows_ = 2;
    double best = 0;
    for (const auto &sh : shapes) {
      const int t = std::min(width_ / sh[0], room_h / sh[1]);
      const double covered = static_cast<double>(t) * t * sh[0] * sh[1];
      if (covered > best * 1.05) {   // 2 x 2 first: another shape must be clearly better
        best = covered;
        cols_ = sh[0];
        rows_ = sh[1];
      }
    }
    return best;
  }

  // The coordinates in the middle of the room (width_ x room_h), the block as large as it can be and still lie wholly in
  // the room: false when the window around them would be too small to be a map.
  bool layout_centred(int room_h) {
    const double eps = 0.001;
    cx_ = width_ / 2;
    cy_ = room_h / 2;
    const double fit = std::min(std::min(cx_ / std::max(px_, eps), (width_ - cx_) / std::max(cols_ - px_, eps)),
                                std::min(cy_ / std::max(py_, eps), (room_h - cy_) / std::max(rows_ - py_, eps)));
    tile_ = static_cast<int>(std::floor(fit));
    hx_ = static_cast<int>(std::floor(std::min(px_, cols_ - px_) * tile_));
    hy_ = static_cast<int>(std::floor(std::min(py_, rows_ - py_) * tile_));
    if (tile_ < 1 || std::min(hx_, hy_) * 2 < ui::mm(12)) return false;
    x0_px_ = cx_ - static_cast<int>(std::lround(px_ * tile_));
    y0_px_ = cy_ - static_cast<int>(std::lround(py_ * tile_));
    dot_x_ = cx_;
    dot_y_ = cy_;
    return true;
  }

  // The block as large as the room allows, in its middle; the dot shows where the coordinates lie in it.
  bool layout_fill(int room_h) {
    cx_ = width_ / 2;
    cy_ = room_h / 2;
    tile_ = std::min(width_ / cols_, room_h / rows_);
    if (tile_ < 1) return false;
    x0_px_ = (width_ - cols_ * tile_) / 2;
    y0_px_ = (room_h - rows_ * tile_) / 2;
    dot_x_ = x0_px_ + static_cast<int>(std::lround(px_ * tile_));
    dot_y_ = y0_px_ + static_cast<int>(std::lround(py_ * tile_));
    return true;
  }

  std::string tile_path(int i, int j) const {
    const int count = 1 << cfg_.zoom;
    return std::to_string(cfg_.zoom) + "/" + std::to_string((x0_ + i) % count) + "/" + std::to_string(y0_ + j);
  }

  // The map's addresses depend on the options only, so they are set once.
  void paint_maps() {
    if (!map_ || !cfg_.show_map) return;
    for (int j = 0; j < rows_; ++j)
      for (int i = 0; i < cols_; ++i)
        ui::set_picture(map_places_[j * cols_ + i], std::string(OSM_HOST) + "/" + tile_path(i, j) + ".png", tessera::Fit::COVER);
  }

  // The radar's addresses: the newest frame's path as RainViewer gave it, the size, the zoom, the tile, the colour scheme
  // (2, Universal Blue) and the options smooth and snow.
  void paint_radar() {
    if (!map_ || path_.empty()) return;
    const std::string path = roomy::plain(path_);
    const std::string options = std::string(cfg_.smooth ? "1" : "0") + "_" + (cfg_.snow ? "1" : "0");
    for (int j = 0; j < rows_; ++j)
      for (int i = 0; i < cols_; ++i)
        ui::set_picture(radar_places_[j * cols_ + i],
                        std::string(RADAR_HOST) + path + "/" + std::to_string(radar_size_) + "/" + tile_path(i, j) + "/2/" + options + ".png",
                        tessera::Fit::COVER);
  }

  // The dark veil: over the light map when the look is dark (the screen's own, or the one chosen in the tile). OpenStreetMap
  // has no dark map, and the providers that have one ask for an account key, so the light map is dimmed.
  void paint_look() {
    if (!veil_) return;
    lv_obj_set_style_bg_opa(veil_, dark_look() ? static_cast<lv_opa_t>(255 * cfg_.dim / 100) : LV_OPA_TRANSP, 0);
  }

  bool dark_look() const { return cfg_.look == Look::DARK || (cfg_.look == Look::AUTO && ::theme::dark); }

  // The ground under the map and its backdrop. It is the card's own colour while the map follows the screen, and a colour
  // that is dark (or light) in both looks when the look of the map is chosen: a dark map then lies on a dark ground.
  theme::Role ground_role() const {
    if (cfg_.look == Look::AUTO) return theme::CARD;
    return cfg_.look == Look::DARK ? theme::CAMERA_PAGE : theme::QR_LIGHT;
  }
  theme::Role icon_role() const {
    if (cfg_.look == Look::AUTO) return theme::TRACK;
    return cfg_.look == Look::DARK ? theme::CAMERA_TRACK : theme::CAMERA_INK;
  }

  // The ground, the masks of the window and the backdrop's icons take the colours of the look again.
  void paint_backdrop() {
    if (ground_) lv_obj_set_style_bg_color(ground_, theme::color(ground_role()), 0);
    for (lv_obj_t *m : masks_)
      if (m) lv_obj_set_style_bg_color(m, theme::color(ground_role()), 0);
    for (int k = 0; k < icon_count_; ++k)
      if (icons_[k]) lv_obj_set_style_text_color(icons_[k], theme::color(icon_role()), 0);
  }

  // The codepoint of the marker's icon in Tessera's icon set; 0 for the standard marker, a dot in a ring.
  static uint32_t marker_glyph(const std::string &name) {
    static const struct {
      const char *name;
      uint32_t code;
    } TABLE[] = {{"pin", 0xF034E},     {"pin_outline", 0xF07D9}, {"home", 0xF02DC},  {"home_pin", 0xF05F8}, {"cross", 0xF01A3},
                 {"heart", 0xF02D1},   {"car", 0xF010B},         {"tree", 0xF0531},  {"person", 0xF0004},   {"compass", 0xF1382},
                 {"rain", 0xF0596},    {"tower", 0xF0D3E},       {"bike", 0xF00A3}};
    for (const auto &entry : TABLE)
      if (name == entry.name) return entry.code;
    return 0;
  }

  // The marker's colour: roles of the theme, the tones of Tessera for red and green.
  lv_color_t marker_colour() const {
    const std::string &c = cfg_.marker_color;
    if (c == "red") return ui::tone_color(tessera::Tone::ALERT, theme::ACCENT);
    if (c == "green") return ui::tone_color(tessera::Tone::GOOD, theme::ACCENT);
    if (c == "amber") return theme::color(theme::MARK_AMBER);
    if (c == "purple") return theme::color(theme::MARK_PURPLE);
    if (c == "white") return theme::color(theme::QR_LIGHT);
    if (c == "black") return theme::color(theme::QR_DARK);
    return theme::color(theme::ACCENT);
  }

  void paint_marker() {
    if (dot_) lv_obj_set_style_bg_color(dot_, marker_colour(), 0);
    if (icon_) lv_obj_set_style_text_color(icon_, marker_colour(), 0);
  }

  const tessera::Plugin *plugin_ = nullptr;
  Config cfg_;
  int cols_ = 2, rows_ = 2, width_ = 0, height_ = 0, tile_ = 0, x0_ = 0, y0_ = 0, radar_size_ = 256;
  int x0_px_ = 0, y0_px_ = 0, cx_ = 0, cy_ = 0, hx_ = 0, hy_ = 0, dot_x_ = 0, dot_y_ = 0;
  int icon_count_ = 0;
  bool map_ = false;
  double px_ = 1, py_ = 1;
  uint32_t frame_at_ = 0;
  tessera::Text path_, wait_, note_text_;   // kept as long as the card: PSRAM first (docs/FIRMWARE_API.md, "Memory")
  lv_obj_t *map_places_[MAX_CELLS]{}, *radar_places_[MAX_CELLS]{};
  lv_obj_t *ground_{}, *masks_[4]{}, *icons_[MAX_ICONS]{}, *icon_{}, *veil_{}, *ring_{}, *dot_{}, *status_{}, *credit_{}, *note_{};
};

// The options of a tile, with the defaults and limits applied.
static Config read_config(JsonObjectConst options) {
  Config c;
  c.show_map = options["map"] | true;
  c.marker_icon = std::string(options["marker_icon"] | "dot");
  c.marker = c.marker_icon != "none";
  c.tiles = options["tiles"] | false;
  const std::string centring = std::string(options["centering"] | "on");   // on: always in the middle; auto: when the map stays large
  c.center = centring != "off";
  c.center_auto = centring == "auto";
  c.dim = std::max(0, std::min(80, static_cast<int>(options["dim"] | 40)));
  c.marker_color = std::string(options["marker_color"] | "blue");
  c.smooth = options["smooth"] | true;
  c.snow = options["snow"] | true;
  const std::string look = options["look"] | "auto";
  c.look = look == "dark" ? Look::DARK : look == "light" ? Look::LIGHT : Look::AUTO;
  c.zoom = std::max(MIN_ZOOM, std::min(MAX_ZOOM, static_cast<int>(options["zoom"] | 6)));
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
