# Developing without riding

OpenRideMirror provides three different test surfaces.

## Firmware demo mode

```sh
orm build esp --mode demo
orm flash esp --mode demo
```

This is the closest receiver-only test. The firmware runs a repeating three-minute synthetic ride, changes speed and heart rate, moves along a fake Jarun route, and enters the ten-second Push view above 30 km/h. BLE is not needed.

## Browser UI recreation

```sh
orm simulate web
```

This serves `web-demo` at localhost. It uses the same 300 × 400 geometry, BDF label/value fonts, embedded speed font and compact map. Telemetry is fixed rather than running a fake ride; the buttons switch between the normal and Push layouts. The linked **Create your map** page generates the three firmware `.h` files and packages them as a ZIP entirely in JavaScript. GitHub Pages cannot compile or flash hardware. The browser canvas is not a substitute for checking the reflective LCD.

## BLE simulator

Install the optional dependency and flash live firmware:

```sh
python3 -m pip install -e 'tools[ble]'
orm simulate ble
```

The computer becomes a BLE central, finds exactly one receiver advertising the ORM name and service, and sends the canonical three-minute stream. This tests receiver parsing and rendering without Garmin.

macOS may request Bluetooth permission for the terminal or Python host. Stop Arduino Serial Monitor before flashing, and stop any existing BLE simulator before trying Garmin so there is only one central connection attempt.
