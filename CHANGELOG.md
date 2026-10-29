# Changelog

## 0.5.2
- The option **Own colour (hex)** of 0.5.1 is gone again: it was a 13th key on a tile that already held 12, which Tessera
  refuses ("Invalid or unsupported page configuration fields" when a colour was changed). The options are now the same
  keys as 0.5.0 or fewer, so a tile of 0.5.0 or 0.5.1 can be edited without being added again.
- With the coordinates in the middle the map is a window evenly round them (the rest of the block is hidden under the
  backdrop), so on the full screen it no longer sticks to the top. Auto keeps the window when it is at least 30% of the tile.
- On a squarish tile Auto shows a single map tile at zoom 7 (about 190 km across) when the coordinates lie well inside it,
  instead of 2 x 2 tiles (about 390 km).

## 0.5.1
- The marker has 46 icons to pick from and eleven colours (orange, yellow, cyan and pink are new), and Tessera offers a plugin
  no icon grid or colour wheel, so these are lists.
- **Smooth radar** and **Show snow** are gone from the options (the radar is always smoothed and shows snow): fewer
  options leave room on a tile within Tessera's limit of 12. A tile of an earlier version that shows the error when an
  option is changed must be deleted and added again once.
- Auto keeps the coordinates within 6% of the middle (was 12%), and when the centred window would be too small it fills
  the tile zoomed in by up to two levels (never past 7), so about 100 to 190 km around the coordinates show on a square tile
  too. The default zoom is 7.

## 0.5.0
- Fixes the error "Invalid or unsupported page configuration fields" when changing options: Tessera keeps at most 12 options
  on a tile, and a tile that carried the names of 0.4.2's options (centering, marker_icon, tiles) went over it. The
  options keep the names of 0.4.0 where they had one (center, marker, smooth), so a tile of any older version stays within
  the limit; delete a tile that already shows the error and add it again once. **Tile numbers** is gone.
- The map's tiles are squares again, of one size, so they always join: stretching them to the shape of a wide tile cut
  every tile at its edges and put the wrong places next to each other.
- **Coordinates in the middle** zooms in by up to two levels (never past 7) when that gives a larger window, and the Auto
  setting (the default) keeps the centred window only when it fills at least half of the tile.
- The ground and the backdrop under the map follow the screen's look again (dark screen, dark backdrop).
- Numbers of the options (zoom, dimming) are read whether Tessera sends them as whole numbers or decimals.

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
