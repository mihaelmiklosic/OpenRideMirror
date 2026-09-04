# Troubleshooting

## Garmin stays on `SCAN` or `NOT FOUND`

- Confirm the ESP32 has live firmware, not a different experiment.
- Check that the receiver advertises exact name `ORM` and the ORM service UUID.
- Ensure the ORM data field is actually placed on the activity screen you started.
- Power-cycle the receiver and restart the activity after a firmware replacement.
- Move other ORM receivers out of range; two matches intentionally produce `MULTIPLE ORM`.

## `PAIR ERR`

In Connect IQ, the API method is named `pairDevice`, even though ORM uses the default open connection strategy and no persistent bond. Stop another central such as the desktop BLE simulator, reboot both devices, and retry. A new ESP32 does not require its MAC address to be added anywhere.

## Garmin says `LIVE`, display waits for activity data

Make sure Garmin and ESP builds use protocol v1 and the same UUIDs. Run `./orm protocol check`, rebuild both from this repository, and confirm the data field is receiving compute callbacks inside an active profile.

## Watch has GPS, display says waiting for GPS

The watch map being visible does not guarantee the data field has received `currentLocation` yet. Wait outdoors with a good fix, keep the activity running, and inspect whether activity/GPS/extended packets are all rotating. Firmware rejects invalid coordinates and requires a recent GPS packet before drawing the map.

## New firmware appears unchanged

Before upload, rediscover the `/dev/cu.usbmodem*` port. Close Arduino Serial Monitor if it owns that port. The CLI compiles and flashes the staged OpenRideMirror sketch; a successful upload should include flash verification and reset output.

## Map build fails

- `--offline` requires a previously cached response for exactly that query.
- Keep requested areas small; Overpass may reject large or expensive queries.
- Validate latitude/longitude order: center uses `[latitude, longitude]`, while a bounding box is `[south, west, north, east]`.
- Personal GPX paths belong in `.orm/config.toml`, not committed example config.

## Text or map looks good in browser but poor on hardware

The reflective panel, pixel orientation and custom ST7305 update path are the truth. Browser simulation reproduces geometry and fonts, not optical contrast. Test outdoors, inspect at actual size and reduce map density before changing the driver.
