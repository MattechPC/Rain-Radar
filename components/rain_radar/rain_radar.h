#pragma once
#include <string>
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::rain_radar {

// What a map needs to be drawn: the options of the tile that shows it (or opened the card), as the editor set them.
enum class Look { AUTO, LIGHT, DARK };
struct Config {
  double lat = 0, lon = 0;
  int zoom = 6;
  bool has_position = false, show_map = true, marker = true, smooth = true, snow = true, center = true, fullscreen = false;
  Look look = Look::AUTO;
};

// The plugin: an ESPHome component and a tessera::Plugin. It registers its tile and its card in setup().
// A card is given nothing but the tile it was opened from, so the tile leaves its options and the newest radar frame here
// just before it opens the card (one card is open at a time).
class RainRadar : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  Config config;                // the options of the tile that opened the card
  tessera::Text frame_path;     // the newest radar frame's path as RainViewer gave it, "" for none yet
  uint32_t frame_at = 0;        // its time, seconds since 1970
};

}  // namespace esphome::rain_radar
