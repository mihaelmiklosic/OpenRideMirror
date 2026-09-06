// SPDX-License-Identifier: GPL-3.0-only
using Toybox.BluetoothLowEnergy as Ble;
using Toybox.System as System;

class OrmBleDelegate extends Ble.BleDelegate {
    private var _owner;

    function initialize(owner) {
        BleDelegate.initialize();
        _owner = owner;
    }

    function onProfileRegister(uuid, status) {
        _owner.onProfileRegister(uuid, status);
    }

    function onScanResults(results) {
        _owner.onScanResults(results);
    }

    function onScanStateChange(scanState, status) {
        _owner.onScanStateChange(scanState, status);
    }

    function onConnectedStateChanged(device, state) {
        _owner.onConnectedStateChanged(device, state);
    }

    function onCharacteristicWrite(characteristic, status) {
        _owner.onCharacteristicWrite(characteristic, status);
    }
}

class OrmBleTransport {
    const DEVICE_NAME = "ORM";
    const SERVICE_UUID_TEXT = "D8185099-1302-4FEB-906F-0AE8D5329ABA";
    const TELEMETRY_UUID_TEXT = "734A1ED9-8E4D-4AEB-A5D7-BEABC20643B8";
    const GPS_INTERVAL_TICKS = 2;
    // The scan window used to be a 2000 ms Timer. Toybox.Timer is not usable
    // from a data field -- constructing one throws UnexpectedTypeException and
    // kills the app before its first compute tick -- so the window is counted
    // in compute() ticks instead. compute() runs at 1 Hz, so 2 ticks ~= 2 s.
    const SCAN_WINDOW_TICKS = 2;
    const EXTENDED_INTERVAL_TICKS = 2;

    private var _delegate;
    private var _scanTicks = 0;
    private var _serviceUuid;
    private var _telemetryUuid;
    private var _device;
    private var _telemetryCharacteristic;
    private var _profileReady = false;
    private var _connected = false;
    private var _connecting = false;
    private var _scanning = false;
    private var _retryTicks = 0;
    private var _writeInProgress = false;
    private var _pendingActivity;
    private var _pendingGps;
    private var _pendingExtended;
    private var _sequence = 0;
    private var _gpsTicks = 0;
    private var _extendedTicks = 0;
    private var _status = "INIT";
    private var _scanCandidate;
    private var _scanHasMultiple = false;
    private var _nextPacketKind = 0;

    function initialize() {
        _serviceUuid = Ble.stringToUuid(SERVICE_UUID_TEXT);
        _telemetryUuid = Ble.stringToUuid(TELEMETRY_UUID_TEXT);
        _delegate = new OrmBleDelegate(self);
    }

    function start() {
        _status = "INIT";
        Ble.setDelegate(_delegate);
        // Baseline receiver accepts an ordinary unencrypted GATT connection.
        // This removes bonding/security from the path while transport is tested.
        Ble.setConnectionStrategy(Ble.CONNECTION_STRATEGY_DEFAULT);

        try {
            Ble.registerProfile({
                :uuid => _serviceUuid,
                :characteristics => [
                    { :uuid => _telemetryUuid }
                ]
            });
        } catch (exception) {
            _status = "PROFILE ERR";
            System.println("ORM profile registration failed: " + errorText(exception));
        }
    }

    function stop() {
        _scanTicks = 0;
        if (_scanning) {
            Ble.setScanState(Ble.SCAN_STATE_OFF);
        }
        _scanning = false;
        _connecting = false;
        _connected = false;
        _retryTicks = 0;
        _writeInProgress = false;
        _telemetryCharacteristic = null;
        _device = null;
    }

    // One tick of the discovery state machine, split out from updateActivity()
    // so it can be driven from unit tests without an Activity.Info. Everything
    // here is time-based rather than data-based.
    function tickDiscovery() {
        // Close the scan window here instead of from a Timer callback.
        if (_scanning) {
            _scanTicks += 1;
            if (_scanTicks >= SCAN_WINDOW_TICKS) {
                finishScan();
            }
        }

        // Keep discovery self-healing. A scan timeout, rejected connection or
        // replacement ESP must not leave the data field permanently stuck in
        // SCAN/PAIR ERR. Activity compute ticks provide a safe retry point.
        if (_profileReady && !_connected && !_connecting && !_scanning) {
            if (_retryTicks > 0) {
                _retryTicks -= 1;
            } else {
                startScan();
            }
        }
    }

    function updateActivity(info, profile) {
        tickDiscovery();

        _pendingActivity = OrmProtocol.activityPacket(info, profile, nextSequence());

        _gpsTicks += 1;
        if (_gpsTicks >= GPS_INTERVAL_TICKS) {
            _gpsTicks = 0;
            try {
                var gps = OrmProtocol.gpsPacket(info, nextSequence());
                if (gps != null) {
                    _pendingGps = gps;
                }
            } catch (exception) {
                System.println("ORM optional GPS packet failed: " + errorText(exception));
            }
        }

        _extendedTicks += 1;
        if (_extendedTicks >= EXTENDED_INTERVAL_TICKS) {
            _extendedTicks = 0;
            try {
                _pendingExtended = OrmProtocol.extendedPacket(info, profile, nextSequence());
            } catch (exception) {
                System.println("ORM optional stats packet failed: " + errorText(exception));
            }
        }

        try {
            if (_connected && _telemetryCharacteristic == null) {
                resolveCharacteristic();
            }
            drainQueue();
        } catch (exception) {
            // Service discovery can briefly be unavailable immediately after
            // the connection callback. Keep the session alive and retry on the
            // next Activity compute tick.
            _telemetryCharacteristic = null;
            _writeInProgress = false;
            _status = "BLE WAIT";
        }
    }

    function getDisplayValue() {
        // Report the status the state machine actually reached. Returning
        // "LIVE" for any connection masked NO SERVICE, NO DATA and WRITE ERR:
        // once connected the field claimed to be working even when no write
        // had ever succeeded, so a half-broken link was indistinguishable from
        // a good one and the documented diagnostics were unreachable.
        return _status;
    }

    function onProfileRegister(uuid, status) {
        if (!uuid.equals(_serviceUuid)) {
            return;
        }

        if (status != Ble.STATUS_SUCCESS) {
            _status = "PROFILE ERR";
            return;
        }

        // Do not start scanning straight from this callback. The scan window
        // is now counted in compute() ticks, and a scan started mid-tick would
        // see the next tick -- possibly milliseconds later -- as a full second,
        // cutting the first window roughly in half. updateActivity() starts it
        // on the next tick boundary instead, so every window is the same
        // length. ORM uses the open/default BLE strategy and no saved bond, so
        // there is nothing to reconnect to and nothing is lost by waiting one
        // tick.
        _profileReady = true;
    }

    function startScan() {
        if (_scanning || _connecting || !_profileReady) {
            return;
        }
        _status = "SCAN";
        _scanning = true;
        _retryTicks = 0;
        _scanCandidate = null;
        _scanHasMultiple = false;
        _scanTicks = 0;
        Ble.setScanState(Ble.SCAN_STATE_SCANNING);
    }

    function onScanResults(results) {
        if (!_scanning) {
            return;
        }

        for (var result = results.next(); result != null; result = results.next()) {
            if (!matches(result)) {
                continue;
            }
            if (_scanCandidate == null) {
                _scanCandidate = result;
            } else if (!_scanCandidate.isSameDevice(result)) {
                _scanHasMultiple = true;
            }
        }
    }

    function finishScan() {
        if (!_scanning) {
            return;
        }
        _scanning = false;
        Ble.setScanState(Ble.SCAN_STATE_OFF);
        if (_scanHasMultiple) {
            _status = "MULTIPLE ORM";
            _scanCandidate = null;
            _retryTicks = 5;
        } else if (_scanCandidate != null) {
            var candidate = _scanCandidate;
            _scanCandidate = null;
            _status = "LINK";
            pair(candidate);
        } else {
            _status = "NOT FOUND";
            _retryTicks = 2;
        }
    }

    function onScanStateChange(scanState, status) {
        if (status != Ble.STATUS_SUCCESS) {
            _scanning = false;
            _status = "SCAN ERR";
            _retryTicks = 2;
            return;
        }
        _scanning = scanState == Ble.SCAN_STATE_SCANNING;
        if (!_scanning && !_connecting && !_connected) {
            _retryTicks = 2;
        }
    }

    function matches(result) {
        var name = result.getDeviceName();
        if (name == null || !name.equals(DEVICE_NAME)) {
            return false;
        }

        var uuids = result.getServiceUuids();
        for (var uuid = uuids.next(); uuid != null; uuid = uuids.next()) {
            if (uuid.equals(_serviceUuid)) {
                return true;
            }
        }
        return false;
    }

    function pair(result) {
        try {
            _connecting = true;
            _device = Ble.pairDevice(result);
            if (_device == null) {
                _connecting = false;
                _status = "PAIR ERR";
                _retryTicks = 2;
            }
        } catch (exception) {
            _connecting = false;
            _device = null;
            _status = "PAIR ERR";
            _retryTicks = 2;
            System.println("ORM pairing failed: " + errorText(exception));
        }
    }

    function onConnectedStateChanged(device, state) {
        if (state == Ble.CONNECTION_STATE_CONNECTED) {
            _device = device;
            _connecting = false;
            _connected = true;
            // Do not access GATT from inside the connection callback. On real
            // Fenix hardware authentication/service discovery may still be
            // finishing; updateActivity() resolves it on the next tick.
            _status = "LINK";
            return;
        }

        _connecting = false;
        _connected = false;
        _writeInProgress = false;
        _telemetryCharacteristic = null;
        _device = null;
        _retryTicks = 2;
        _status = state == Ble.CONNECTION_STATE_REJECTED ? "REJECTED" : "LOST";
        // updateActivity() consumes _retryTicks and re-scans on a tick
        // boundary; see onProfileRegister() for why scans do not start here.
    }

    function resolveCharacteristic() {
        if (_device == null || !_device.isConnected()) {
            return;
        }

        var service = _device.getService(_serviceUuid);
        if (service == null) {
            _status = "NO SERVICE";
            return;
        }

        _telemetryCharacteristic = service.getCharacteristic(_telemetryUuid);
        if (_telemetryCharacteristic == null) {
            _status = "NO DATA";
        }
        // Finding the characteristic is not the same as streaming: the field
        // stays on LINK until a write is acknowledged in onCharacteristicWrite,
        // which is what LIVE is documented to mean.
    }

    function drainQueue() {
        if (!_connected || _writeInProgress || _telemetryCharacteristic == null) {
            return;
        }

        var packet = null;
        for (var offset = 0; offset < 3 && packet == null; offset += 1) {
            var kind = (_nextPacketKind + offset) % 3;
            if (kind == 0 && _pendingActivity != null) {
                packet = _pendingActivity;
                _pendingActivity = null;
            } else if (kind == 1 && _pendingGps != null) {
                packet = _pendingGps;
                _pendingGps = null;
            } else if (kind == 2 && _pendingExtended != null) {
                packet = _pendingExtended;
                _pendingExtended = null;
            }
            if (packet != null) {
                _nextPacketKind = (kind + 1) % 3;
            }
        }

        if (packet == null) {
            return;
        }

        try {
            _writeInProgress = true;
            _telemetryCharacteristic.requestWrite(packet, {
                :writeType => Ble.WRITE_TYPE_WITH_RESPONSE
            });
        } catch (exception) {
            _writeInProgress = false;
            _status = "WRITE ERR";
            System.println("ORM write failed: " + errorText(exception));
        }
    }

    function onCharacteristicWrite(characteristic, status) {
        if (_telemetryCharacteristic == null ||
                !characteristic.getUuid().equals(_telemetryUuid)) {
            return;
        }

        _writeInProgress = false;
        if (status == Ble.STATUS_SUCCESS) {
            _status = "LIVE";
            drainQueue();
        } else {
            _status = "WRITE ERR";
        }
    }

    // Monkey C throws UnexpectedTypeException on String + null, so a failure
    // whose getErrorMessage() is null would turn a handled error into an app
    // crash -- the opposite of what these catch blocks are for.
    function errorText(exception) {
        var message = exception.getErrorMessage();
        return message == null ? "unknown" : message;
    }

    function nextSequence() {
        _sequence = (_sequence + 1) & 0xff;
        return _sequence;
    }
}
