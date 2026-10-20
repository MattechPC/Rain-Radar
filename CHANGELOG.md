# Changelog

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
