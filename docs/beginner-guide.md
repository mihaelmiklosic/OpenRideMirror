# Beginner-friendly build guide (macOS)

You do not need to be an embedded developer, but this is still a DIY source project. You should be comfortable opening Terminal, installing a few programs, connecting USB devices and copying one file to the watch. Set aside about an hour for the first setup.

## First: what do I actually need?

### Hardware

- the exact Waveshare ESP32-S3-RLCD-4.2 reference board;
- a USB **data** cable for the ESP32;
- a Garmin Fenix 7-family watch and its USB cable;
- a Mac for the currently documented workflow.

### Which exact Waveshare product?

Buy the [Waveshare ESP32-S3-RLCD-4.2 development board](https://www.waveshare.com/product/esp32-s3-rlcd-4.2.htm):

- SKU **33298 / ESP32-S3-RLCD-4.2** includes an 18650 battery;
- SKU **33507 / ESP32-S3-RLCD-4.2-EN** does not include the battery.

Either variant is suitable for ORM; USB-C is enough while developing. Look for the integrated 4.2-inch **300 × 400 RLCD**, ESP32-S3-WROOM-1-N16R8, 16 MB flash, 8 MB PSRAM and onboard SHTC3 temperature/humidity sensor.

Do **not** buy a generic Waveshare 4.2-inch e-paper panel or an ESP32 e-paper driver board. Although this screen looks e-paper-like, it is a reflective LCD. Similar Waveshare product names do not mean the display controller and pins are compatible. More identification details are in [Reference hardware](hardware.md).

### Software on the Mac

- Python 3.11 or newer;
- Arduino IDE 2.x;
- ESP32 Arduino core 3.3.11 and U8g2 2.36.18;
- Garmin Connect IQ SDK Manager, Connect IQ SDK 9.2.0 and the Fenix device package;
- Java 17;
- an MTP file-transfer app such as OpenMTP if your watch does not appear in Finder.

### Do I need Android?

**No Android phone and no Android app are needed.** Garmin sends ride data directly from the watch to the ESP32 over BLE.

The confusing bit is file transfer on macOS. Fenix 7 uses the Media Transfer Protocol (MTP), also common on Android devices. macOS may not show it in Finder, so you may need a Mac MTP utility such as OpenMTP or the older Android File Transfer tool just to copy the Garmin `.prg` file. That utility does not mean the project uses Android.

Garmin Connect on your phone is optional for normal watch management. It is not between the watch and display. Garmin Express is also not required for the data stream and can block an MTP utility while it is using the watch, so close/eject Garmin Express before opening the watch in an MTP app. Garmin confirms that many watches are MTP devices and that macOS has limited native MTP support: [Garmin support](https://support.garmin.com/en-US/?faq=zUa4z1zKNn39o6JiqZDHNA).

## Part A — prepare this repository

You do not need to understand or edit TOML files. TOML is simply the internal text format used for optional developer settings. The setup script creates a working default automatically.

Install [Arduino IDE 2.x](https://www.arduino.cc/en/software) as a normal Mac application and open it once. Then open Terminal, enter the downloaded/cloned OpenRideMirror folder and run:

```sh
./orm setup
```

That one command creates the internal default config and installs the exact ESP32 Arduino core and U8g2 library. You do not create a Python environment, run `pip`, open Arduino's Boards Manager or edit a config file yourself. It does not flash anything and does not accept Garmin’s agreement for you.

Run:

```sh
./orm doctor
```

Every line should eventually say `OK`. A missing line tells you which section below to complete.

## Part B — install the ESP32 build tools

`./orm setup` already handles the Arduino command-line details. If Arduino IDE is missing, it stops with a direct instruction instead of partially configuring the board. Run `./orm doctor` at any time to see what is ready.

## Part C — test only the ESP32 first

Connect the ESP32 with a data cable. Start with demo mode, which needs no watch:

```sh
./orm demo
```

This builds and flashes the test firmware in one step. Expected result: the display starts a synthetic ride, changes speed and moves its fake GPS position. If the CLI finds zero or two USB modem ports, list them with:

```sh
ls -1 /dev/cu.usbmodem*
```

Then pass the exact new one:

```sh
./orm demo --port /dev/cu.usbmodemXXXX
```

Do not choose a Bluetooth, headphone or debug-console serial port.

### Optional: replace the sample with your own map

Open the separate **[Create your map](https://mihaelmiklosic.github.io/OpenRideMirror/map-builder.html)** tool. Select a center point and radius, click **Build map pack**, inspect the preview and download the ZIP. Then run:

```sh
./orm map flash --mode demo
```

The browser generates all three `.h` map files. The command automatically uses the newest OpenRideMirror map pack in Downloads, installs it, builds the demo and flashes the connected display. You do not open, edit or manually copy anything. For a real area, the chosen coordinates are sent to the public Overpass API only to retrieve OpenStreetMap data.

## Part D — install Garmin’s developer tools

This is needed because the repository does not distribute a ready-made `.prg`; you build it from source locally.

1. Install Java 17. With Homebrew, the simple option is:

   ```sh
   brew install openjdk@17
   ```

2. Download Garmin’s [Connect IQ SDK Manager](https://developer.garmin.com/connect-iq/sdk/).
3. Launch SDK Manager and personally read/accept Garmin’s SDK agreement.
4. Install Connect IQ SDK 9.2.0.
5. In SDK Manager, download your exact Fenix device package and make 9.2.0 the active SDK.
6. Close SDK Manager and run `./orm doctor` again.

Garmin’s official command-line setup also documents installing SDK Manager, selecting an active SDK and using Java: [Connect IQ command-line setup](https://developer.garmin.com/connect-iq/reference-guides/monkey-c-command-line-setup/).

## Part E — build the Garmin data field

For a regular Fenix 7:

```sh
./orm garmin fenix7
```

The output path is printed at the end and looks like:

```text
.orm/build/garmin/OpenRideMirror-fenix7.prg
```

Use the device ID matching your watch. Current IDs are listed in [Garmin setup](garmin.md). If you choose the wrong one, the watch can reject the file even though compilation succeeded.

## Part F — copy the `.prg` to the watch

1. Close Garmin Express completely, or eject the watch from Garmin Express.
2. Connect the watch by USB.
3. If it appears as a normal drive in Finder, open it. If it does not, open it using an MTP utility such as OpenMTP.
4. Open `GARMIN`, then `APPS` on the watch.
5. Copy the newly built `.prg` into `GARMIN/APPS`.
6. Eject the watch cleanly and disconnect it.

Garmin’s developer instructions specify USB sideloading into `GARMIN/APPS`: [Garmin developer guide](https://developer.garmin.com/downloads/connect-iq/wearable-programming-for-the-active-lifestyle.pdf). Modern Garmin watches may move the file into hidden internal storage after processing it. The `.prg` disappearing from `GARMIN/APPS` does not automatically mean installation failed.

## Part G — add ORM Live to an activity

On the watch, edit the activity you want to test (Bike is the main target, but Walk also works with GPS):

1. open the activity settings;
2. open **Data Screens**;
3. add a new data screen, or edit one field on an existing screen;
4. choose **Connect IQ Fields**;
5. choose **ORM Live**;
6. save the activity profile.

The data field must be on one of that activity’s data screens. You do not have to keep looking at that page while riding, but the field must be part of the active profile so Garmin runs it.

## Part H — first real connection

1. Build and flash live firmware to the ESP32:

   ```sh
   ./orm live
   ```

2. Keep only one ORM receiver powered nearby.
3. On Garmin, start the activity containing ORM Live.
4. Wait through `SCAN` and `LINK` until the Garmin field says `LIVE`.
5. Go outside and wait for GPS lock. The display map updates only after Garmin exposes a valid position to the field.

There is no MAC address to type and no separate phone-pairing screen. The watch finds the receiver by exact BLE name plus service UUID. A replacement ESP32 can have a new address and still work.

## If something fails

- Demo firmware fails: solve ESP32/USB/display setup before involving Garmin.
- Garmin build fails: rerun `./orm doctor` and confirm the exact device package is installed.
- Watch does not appear on Mac: close Garmin Express and use an MTP utility.
- `.prg` vanishes: modern watches may move it after processing; check whether ORM Live appears in Connect IQ Fields.
- `MULTIPLE ORM`: power off the extra receiver.
- `PAIR ERR`: stop the desktop BLE simulator, reboot both devices and retry.
- Garmin says `LIVE` but display waits: rebuild both sides from the same repository and run `./orm protocol check`.

The full diagnostic list is in [Troubleshooting](troubleshooting.md).
