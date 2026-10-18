# Rain radar

The rain radar of [RainViewer](https://www.rainviewer.com) over an [OpenStreetMap](https://www.openstreetmap.org)
map, on a tile of your Tessera screen. You give it a latitude, a longitude and a zoom; it shows the newest radar frame
over the map around that place, with a dot on the exact coordinates and the time of the frame under it.

Map data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright).
Weather data by [RainViewer](https://www.rainviewer.com).

## Set up

1. In Tessera, open Plugins, choose **Add with a link** and paste `https://github.com/MattechPC/Rain-Radar`. Add the
   plugin to a screen; the screen builds once. There is nothing to sign up for: RainViewer and OpenStreetMap need no key.
2. In Layout, place the **Rain radar** tile from the library's Plugins group. Give it two cells across and two down at
   least; the map is a square with two small lines of credits under it.
3. In the inspector, fill in the **Latitude** and **Longitude** in degrees, such as `52.2297` and `21.0122` (south and
   west are negative; a comma works as the decimal point).
4. Choose the **Zoom**, from 2 (about a continent) to 7 (about a region). 7 is the most RainViewer's radar offers.
5. Choose the **Look of the map**: *Follow the screen* dims the map when the screen is in its dark look and leaves it
   light in the light look; *Light* and *Dark* keep it one way whatever the screen does.

## Good to know

- The map is 2 × 2 tiles with the coordinates near its middle, not exactly in it: the dot shows where they are.
- The radar frame is the newest of the last two hours; RainViewer makes one every ten minutes, and Tessera asks for the
  list every five. The line under the map says the time of the frame, or that it is out of date when it is over an hour old.
- Light and dark: the tile's own colours follow the screen's look by themselves. OpenStreetMap's map has only a light
  look, so in the dark look it is dimmed with a dark veil (the radar under it a little too); **Show the map** off shows
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
