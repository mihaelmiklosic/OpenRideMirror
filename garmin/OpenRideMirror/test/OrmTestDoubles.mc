// SPDX-License-Identifier: GPL-3.0-only
//
// Stand-ins for the BLE objects Connect IQ hands to the delegate callbacks.
//
// Ble.ScanResult and Ble.Characteristic cannot be constructed from test code,
// but Monkey C dispatches dynamically: the transport only ever calls a handful
// of methods on them, so plain classes exposing that same surface are accepted
// wherever the real objects would be. This is what makes the scan, pairing and
// write paths reachable without a watch or an ESP32 board.
using Toybox.Lang as Lang;

// Iterator shape shared by scan results and service UUID lists.
class FakeIterator {
    private var _items as Lang.Array;
    private var _index = 0;

    function initialize(items as Lang.Array) {
        _items = items;
    }

    function next() {
        if (_index >= _items.size()) {
            return null;
        }
        var item = _items[_index];
        _index += 1;
        return item;
    }
}

class FakeScanResult {
    private var _name;
    private var _uuids;
    private var _identity;

    // identity models the BLE address: two results with the same identity are
    // the same physical device seen twice, which is what isSameDevice() reports
    // and what separates "one board advertising twice" from "two boards".
    function initialize(name, uuids, identity) {
        _name = name;
        _uuids = uuids;
        _identity = identity;
    }

    function getDeviceName() {
        return _name;
    }

    function getServiceUuids() {
        return new FakeIterator(_uuids);
    }

    function isSameDevice(other) {
        return _identity == other.getIdentity();
    }

    function getIdentity() {
        return _identity;
    }
}

class FakeCharacteristic {
    private var _uuid;

    function initialize(uuid) {
        _uuid = uuid;
    }

    function getUuid() {
        return _uuid;
    }
}

// The GATT chain the transport walks after connecting: device -> service ->
// characteristic. Injected through onConnectedStateChanged(), which stores
// whatever device object it is handed, so the NO SERVICE / NO DATA / WRITE ERR
// paths become reachable without a real peripheral.
class FakeService {
    private var _characteristics as Lang.Array;

    // characteristics maps a UUID to a FakeCharacteristic; a missing entry
    // models a service that does not expose the telemetry characteristic.
    function initialize(characteristics as Lang.Array) {
        _characteristics = characteristics;
    }

    function getCharacteristic(uuid) {
        for (var index = 0; index < _characteristics.size(); index += 1) {
            var candidate = _characteristics[index];
            if (candidate.getUuid().equals(uuid)) {
                return candidate;
            }
        }
        return null;
    }
}

class FakeDevice {
    private var _services as Lang.Array;
    private var _connected;

    function initialize(services as Lang.Array, connected) {
        _services = services;
        _connected = connected;
    }

    function isConnected() {
        return _connected;
    }

    // services is a list of [uuid, FakeService] pairs; absent means the device
    // advertised the service but does not actually expose it over GATT.
    function getService(uuid) {
        for (var index = 0; index < _services.size(); index += 1) {
            var pair = _services[index] as Lang.Array;
            if (pair[0].equals(uuid)) {
                return pair[1];
            }
        }
        return null;
    }
}
