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
   does. **Coordinates in the middle** (off by default) is explained below.
6. Choose the **Look of the map**: *Follow the screen* dims the map when the screen is in its dark look and leaves it
   light in the light look; *Light* and *Dark* keep it one way whatever the screen does.

## Good to know

- The map is made of 2 × 2 OpenStreetMap tiles with the radar tiles exactly aligned over them. In the normal tile, **Coordinates in the
  middle** moves the map so the coordinates are centred and masks the uneven edges; with it off, the map fills the tile and
  the dot shows where the coordinates are.
- The fullscreen card now uses the card's actual width and height and a camera-like **cover** fit: it scales the tile block
  until the available screen area is covered, clipping the excess at the screen edges. This adapts to different screen sizes
  and orientations and reserves no space for the status line. Provider attribution remains overlaid at the bottom.
- The map and radar are still requested as separate provider tiles and layered by Tessera. The current plugin API accepts
  remote picture URLs rather than a locally composed raster, so producing one physical PNG/JPEG inside this plugin would
  require a new image-composition endpoint or a change to Tessera's image API; this package does not pretend to implement
  that unsupported part.
- The radar frame is the newest of the last two hours; RainViewer makes one every ten minutes, and Tessera asks for the
  list every five. The line under the map says the time of the frame, or that it is out of date when it is over an hour old.
- Light and dark: the tile's own colours follow the screen's look by themselves. OpenStreetMap's map has only a light
  look, so in the dark look it is dimmed with a dark veil, the radar with it. A black map like Home Assistant's (CARTO
  Dark Matter) needs an account key since September 2026, and a key cannot be passed to the screen's pictures, so it is
  not offered; **Show the map** off shows
  the radar alone on the card's colour.
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
