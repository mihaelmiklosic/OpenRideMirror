# Attribution and third-party material

OpenRideMirror project code is GPL-3.0-only. The following bundled components or assets retain separate licenses.

## U8g2

The firmware links against U8g2, Copyright © 2016 olikraus@gmail.com, under the two-clause BSD license. The dependency is installed separately through Arduino Library Manager. Its license and font notices are reproduced in `third_party/licenses/U8g2-and-X11-fonts.txt`.

## Helvetica BDF fonts

The browser demo includes `helvB08.bdf` and `helvB14.bdf`, derived from the X11 Helvetica fonts distributed with U8g2. Copyright 1984–1989, 1994 Adobe Systems Incorporated and Copyright 1988, 1994 Digital Equipment Corporation. Their permission notice is included in `third_party/licenses/U8g2-and-X11-fonts.txt`.

## Inconsolata

The browser demo embeds Inconsolata Bold, originally by Raph Levien, under the SIL Open Font License 1.1. See `third_party/licenses/Inconsolata-OFL-1.1.txt`.

## OpenStreetMap

The checked-in firmware map is synthetic. The browser demo includes a small clipped OpenStreetMap-derived extract and displays `© OSM`; it is a separate database asset under ODbL 1.0. The map generator can download additional data from OpenStreetMap contributors through an Overpass instance. OpenStreetMap data is available under ODbL 1.0; see `third_party/licenses/ODbL-1.0.txt` and [openstreetmap.org/copyright](https://www.openstreetmap.org/copyright).

Generated personal maps and Overpass caches are ignored by default. If you redistribute a generated map or firmware containing one, you are responsible for attribution and applicable ODbL obligations.

## Garmin and Waveshare

Garmin, Fenix, Connect IQ, Waveshare and ESP32 are names or trademarks of their respective owners. This independent community project is not affiliated with, endorsed by or supported by Garmin, Waveshare or Espressif.
