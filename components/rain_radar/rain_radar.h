#pragma once
#include <string>
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::rain_radar {

enum class Look { AUTO, LIGHT, DARK };
struct Config {
  double lat = 0, lon = 0;
  int zoom = 6;
  bool has_position = false, show_map = true, marker = true, smooth = true, snow = true, center = true, center_auto = false, fullscreen = true;
  Look look = Look::AUTO;
  int dim = 40;
  bool tiles = false;
  std::string marker_icon = "dot";
  std::string marker_color = "blue";
};

class RainRadar : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  Config config;
  tessera::Text frame_path;
  uint32_t frame_at = 0;
};

}  // namespace esphome::rain_radar
