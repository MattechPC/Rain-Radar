## 0.4.3
- Fix configuration validation errors on slider options.
- Fix map sizing to cover 100% of tile area.
- Fix background color binding to theme::dark.

# Changelog

## 0.4.2
- **Marker**: the standard dot in a colour of your choice, or an icon from a list (pin, home, crosshair, car, ...), or
  none. This replaces the **Show the marker** switch.
- **Dimming in the dark look** is an option now (0 to 80%, 40% by default); before it was a fixed 70%.
- **Coordinates in the middle** has three settings: On (always in the middle, the default), Auto (only when the window
  around the coordinates stays large) and Off. 0.4.1's automatic rule had taken over the On setting.
- The backdrop of weather icons shows on the full-screen card again: the masks of the centred window had covered it.
- With the look of the map set to Light or Dark, the ground under the map and the backdrop take that look too.
- **Tile numbers**: an option that shows the zoom/x/y of the map tile after the time, to check the map against
  openstreetmap.org. **Smooth radar** is gone (the radar is always smoothed); a tile may have at most 12 options.

## 0.4.1
- Fixes a crash of the screen with a tile as tall as the screen (4 across, 5 down): the range rings of 0.4.0 reached past
  the edge of the tile and are gone.
- **Coordinates in the middle** now looks at how large the map would be: when the window around the coordinates would
  show less than 60% of what the filling block shows, the map fills the tile instead (the dot shows the place).
- The backdrop lies under the map (no lines across the radar), the dark veil covers the map alone (no black frame around
  it), and the weather icons are thrown at irregular places in two sizes and fill the free part of the tile.

## 0.4.0
- A backdrop where the map does not cover the tile or the full-screen card: weather icons at various angles and range
  rings round the coordinates, in the quiet colours of the theme, instead of a plain empty frame.
- The CARTO key option of the previous draft is gone: the dark look dims the OpenStreetMap map, as before.

## 0.2.3
- **Coordinates in the middle** is on by default again.
- No more grey empty frame: what the map tiles do not cover is filled with the page's colour (dimmed
  in the dark look), so the map looks like it fills the whole tile or full-screen card.
- With the coordinates not in the middle, the map takes the shape of up to four tiles (2 x 2, 3 x 1, 4 x 1 or upright)
  that covers most of a wide or tall tile.

## 0.2.2
- The map now fills the tile by default: **Coordinates in the middle** is off by default (it made the map a small window
  inside the tile). Turn it on for the exact centre at the cost of a smaller map.
- A dark look closer to Home Assistant's is not possible without an account key (see the README).

## 0.2.1
- **Open full screen on tap** is on by default; **Show the map** was already on.

## 0.2.0
- The map is built from square tiles that always join, with square corners: no more jumps between tiles and no dark
  gaps where they meet.
- **Coordinates in the middle** (on by default): the chosen coordinates are exactly the centre of the map. The map is
  then a window around them, smaller than the tile; turn it off to fill the whole tile.
- **Open full screen on tap**: a tap on the map opens it over the whole screen, as a camera tile does (off by default).
- The dark look dims the map more strongly.
- The tile can be made as large as Tessera offers (up to 9 x 9). The zoom factor and aspect ratio options are gone.
- Every word on the tile is translated again (English, Polish, Dutch).

## 0.1.1
- The tile can be made larger in the editor: it starts at one cell (2 x 2 or more shows the map).

## 0.1.0
- First version: the rain radar of RainViewer over an OpenStreetMap map, centred on the coordinates you choose.
