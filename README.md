# Rain radar

The rain radar of [RainViewer](https://www.rainviewer.com) over an [OpenStreetMap](https://www.openstreetmap.org)
map, on a tile of your Tessera screen. You give it a latitude, a longitude and a zoom; it shows the newest radar frame
over the map around that place, with a dot on the exact coordinates and the time of the frame under it.

Map data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright).
Weather data by [RainViewer](https://www.rainviewer.com).

## Set up

1. In Tessera, open Plugins, choose **Add with a link** and paste `https://github.com/MattechPC/Rain-Radar`. Add the
   plugin to a screen; the screen builds once. There is nothing to sign up for: RainViewer and OpenStreetMap need no key.
2. In Layout, place the **Rain radar** tile from the library's Plugins group and drag its edges to make it larger. From
   two cells across and two down the map shows (a smaller tile says so); the larger the tile, the larger the map.
   Making a tile taller needs "taller tiles" to be on in Tessera.
3. In the inspector, fill in the **Latitude** and **Longitude** in degrees, such as `52.2297` and `21.0122` (south and
   west are negative; a comma works as the decimal point).
4. Choose the **Zoom**, from 2 (about a continent) to 7 (about a region). 7 is the most RainViewer's radar offers.
5. **Open full screen on tap** (on by default) opens the map over the whole screen when you tap it, as a camera tile
   does. **Coordinates in the middle** is explained below.
6. Choose the **Look of the map**: *Follow the screen* dims the map when the screen is in its dark look and leaves it
   light in the light look; *Light* and *Dark* keep it one way whatever the screen does. **Dimming in the dark look**
   says how strongly (40% by default).
7. Choose the **Marker**: the standard dot, an icon from the list, or none, and its **colour**.

## Good to know

- The map is made of up to four OpenStreetMap tiles with the radar's tiles exactly over them, every tile a square of the
  same size so that they always join. Tessera can show a picture only whole and only inside the tile (at most 8 pictures
  per tile), so a map cannot be cut to the exact shape of a tile: it takes the shape of 2 × 2, 3 × 1, 4 × 1 or upright
  tiles that covers most of the tile, and what it does not cover shows a quiet backdrop of weather icons in the theme's
  colours (dark screen, dark backdrop).
- **Coordinates in the middle** has three settings. *Auto* (the default) always fills at least 80% of the tile (where the
  shapes of tile and map allow it): it tries the zoom you chose and up to two levels closer (never past 7) and several
  blocks of map tiles, also one tile or a row of tiles stretched over the whole tile, each tile then cut at the top and
  bottom (at the sides for a column), and takes the one that puts your coordinates nearest the middle. A block of several rows
  and columns is never stretched, since its tiles would not meet. *On* puts the coordinates exactly in the middle and shows
  a window evenly round them (the rest of the block lies under the backdrop), which can be small. *Off* fills the tile with
  the coordinates where they fall. If a tile that you made with an older version shows an error when you change options,
  delete it and add the tile again once: Tessera keeps at most 12 options on a tile, and the old tile still holds names that
  this version no longer uses.
- The radar frame is the newest of the last two hours; RainViewer makes one every ten minutes, and Tessera asks for the
  list every five. The line under the map says the time of the frame, or that it is out of date when it is over an hour old.
- Light and dark: the tile's own colours follow the screen's look by themselves. OpenStreetMap's map has only a light
  look, so in the dark look it is dimmed with a dark veil, the radar with it, as strongly as **Dimming in the dark look**
  says. A black map like Home Assistant's would need a provider's account key, so it is not offered. **Show the map**
  off shows the radar alone on the card's colour.
- A screen without the memory for pictures (the CYD, the Waveshare 3.5-inch, the Hosyond 4-inch) shows the coordinates
  and the credits in words instead of the map.
- RainViewer's radar covers where weather radars exist; elsewhere the map shows without rain.
- RainViewer provides its free radar for personal and educational use and small community projects, and does not
  promise that it is always there. Read [its API page](https://www.rainviewer.com/api.html) and [terms](https://www.rainviewer.com/terms.html) before you use this
  anywhere else than at home. OpenStreetMap's tile servers are run by volunteers for light use
  ([tile usage policy](https://operations.osmfoundation.org/policies/tiles/)); the plugin asks for four map tiles per
  tile on the screen, once, and the Tessera app keeps them.

## Privacy

Tessera (the app in Home Assistant, not the screen) asks `api.rainviewer.com` for the list of radar frames and fetches
the radar tiles from `tilecache.rainviewer.com` and the map tiles from `tile.openstreetmap.org`. The map and radar
servers see the address of your Home Assistant and the map tiles you look at, which show roughly where the coordinates
you chose are; nothing else of your home. No key and no account are involved. The screen never reaches the internet.

## Marker
**Marker** takes the standard dot, no marker or one of 46 icons from the list (a pin's tip is on the coordinates, any other
icon's middle). **Marker colour** has eleven colours. Tessera's options for a plugin are a text, a list, a number or a
switch, so there is no icon grid or colour wheel for them.
