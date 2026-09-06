// SPDX-License-Identifier: GPL-3.0-only
//
// Discovery state machine tests for OrmBleTransport.
//
// The Connect IQ simulator does not emulate the BLE stack: it accepts
// registerProfile() but never delivers onProfileRegister(), so a data field
// running there sits on INIT forever and the state machine is never exercised.
// These tests drive the delegate callbacks directly instead, which covers the
// transitions without a watch, an ESP32 board, or a BLE radio.
//
// They deliberately avoid start(). That call registers a real BLE profile
// against simulator-global state, and repeated registrations fail with
// "Too Many Profiles". Everything under test is reachable without it.
using Toybox.Test as Test;
using Toybox.BluetoothLowEnergy as Ble;

const TEST_SERVICE_UUID_TEXT = "D8185099-1302-4FEB-906F-0AE8D5329ABA";
const TEST_OTHER_UUID_TEXT = "0000FFFF-0000-1000-8000-00805F9B34FB";
const TEST_TELEMETRY_UUID_TEXT = "734A1ED9-8E4D-4AEB-A5D7-BEABC20643B8";
const ORM_DEVICE_NAME = "ORM";

function ormServiceUuid() {
    return Ble.stringToUuid(TEST_SERVICE_UUID_TEXT);
}

// A transport that has been told its profile registered successfully.
function readyTransport() {
    var transport = new OrmBleTransport();
    transport.onProfileRegister(ormServiceUuid(), Ble.STATUS_SUCCESS);
    return transport;
}

(:test)
function testStartsInInit(logger) {
    var transport = new OrmBleTransport();
    Test.assertEqual(transport.getDisplayValue(), "INIT");
    return true;
}

(:test)
function testStaysInInitUntilProfileIsReady(logger) {
    // No profile yet, so ticking must not start scanning. A field that scanned
    // before its profile registered would fail on hardware, not just here.
    var transport = new OrmBleTransport();
    transport.tickDiscovery();
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "INIT");
    return true;
}

(:test)
function testProfileRegisterFailureIsReported(logger) {
    // STATUS_NOT_ENOUGH_RESOURCES is the realistic failure here: Connect IQ
    // caps how many BLE profiles exist across all apps on the device, and this
    // is what a device already at that cap reports.
    var transport = new OrmBleTransport();
    transport.onProfileRegister(ormServiceUuid(), Ble.STATUS_NOT_ENOUGH_RESOURCES);
    Test.assertEqual(transport.getDisplayValue(), "PROFILE ERR");

    // A failed registration must not silently start scanning either.
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "PROFILE ERR");
    return true;
}

(:test)
function testIgnoresProfileRegisterForAnotherService(logger) {
    var transport = new OrmBleTransport();
    transport.onProfileRegister(Ble.stringToUuid(TEST_OTHER_UUID_TEXT),
                                Ble.STATUS_SUCCESS);
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "INIT");
    return true;
}

(:test)
function testProfileReadyDoesNotScanUntilTheNextTick(logger) {
    // Scans must start on a tick boundary. Starting one straight from the
    // callback makes the next tick -- possibly milliseconds later -- count as a
    // whole second and cuts the first scan window roughly in half.
    var transport = readyTransport();
    Test.assertEqual(transport.getDisplayValue(), "INIT");

    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "SCAN");
    // Leaving a scan running would leak simulator-global BLE scan state into
    // whichever test happens to run next.
    transport.stop();
    return true;
}

(:test)
function testScanWindowLastsTwoTicks(logger) {
    var transport = readyTransport();
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "SCAN");

    // First tick inside the window: still scanning.
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "SCAN");

    // Second tick closes the window. Nothing advertised, so: NOT FOUND.
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "NOT FOUND");
    return true;
}

(:test)
function testNotFoundBacksOffThenScansAgain(logger) {
    var transport = readyTransport();
    transport.tickDiscovery();
    transport.tickDiscovery();
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "NOT FOUND");

    // finishScan() sets _retryTicks = 2, but it also clears _scanning, so the
    // retry block further down the same tick already consumes one of them.
    // The observable back-off is therefore one further tick, not two. This is
    // intentional-by-consequence rather than by design; the test pins the real
    // behaviour so a future change to the tick order cannot alter the retry
    // cadence unnoticed.
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "NOT FOUND");

    // Then discovery restarts on its own -- the field must never wedge.
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "SCAN");
    return true;
}

(:test)
function testDiscoveryRecoversForever(logger) {
    // The real failure mode this guards against is a field stuck on one status
    // for the rest of the ride. Over many ticks it must keep cycling.
    // Counting transitions rather than just "did SCAN ever appear": an
    // implementation that scanned once, failed once and then wedged for the
    // rest of the ride would satisfy a mere sighting of each status.
    var transport = readyTransport();
    var restarts = 0;
    var previous = transport.getDisplayValue();

    for (var tick = 0; tick < 40; tick += 1) {
        transport.tickDiscovery();
        var status = transport.getDisplayValue();
        if (status.equals("SCAN") && !previous.equals("SCAN")) {
            restarts += 1;
        }
        previous = status;
    }

    // 40 ticks at roughly 4 ticks per scan/back-off cycle; demand several full
    // cycles so a single lucky transition cannot pass.
    Test.assert(restarts >= 5);
    transport.stop();
    return true;
}

(:test)
function testStopResetsDiscovery(logger) {
    var transport = readyTransport();
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "SCAN");

    transport.stop();

    // One tick after stop() proves nothing on its own: a no-op stop() would
    // just carry the existing scan from tick 0 to tick 1 and still read "SCAN".
    // The tick after that is what separates them -- a scan restarted from zero
    // is still inside its window, a stale one has already run out and closed to
    // "NOT FOUND".
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "SCAN");
    transport.tickDiscovery();
    Test.assertEqual(transport.getDisplayValue(), "SCAN");

    transport.stop();
    return true;
}

// --- Connection-side transitions -------------------------------------------
// onConnectedStateChanged() takes the device object straight through, so these
// can pass null: nothing on this path dereferences it.

(:test)
function testConnectionLossSchedulesARescan(logger) {
    var transport = readyTransport();
    transport.tickDiscovery();
    transport.onConnectedStateChanged(null, Ble.CONNECTION_STATE_DISCONNECTED);
    Test.assertEqual(transport.getDisplayValue(), "LOST");

    // A dropped board must not wedge the field: discovery has to come back.
    var recovered = false;
    for (var tick = 0; tick < 6 && !recovered; tick += 1) {
        transport.tickDiscovery();
        if (transport.getDisplayValue().equals("SCAN")) {
            recovered = true;
        }
    }
    Test.assert(recovered);
    transport.stop();
    return true;
}

(:test)
function testDoesNotScanWhileConnected(logger) {
    // Scanning while connected would drop the live link on real hardware.
    var transport = readyTransport();
    transport.onConnectedStateChanged(null, Ble.CONNECTION_STATE_CONNECTED);
    for (var tick = 0; tick < 5; tick += 1) {
        transport.tickDiscovery();
        Test.assert(!transport.getDisplayValue().equals("SCAN"));
    }
    transport.stop();
    return true;
}

(:test)
function testDoesNotClaimLiveBeforeAWriteSucceeds(logger) {
    // LIVE is documented as "telemetry writes are succeeding". Being connected
    // is not the same thing: the GATT service may be missing, the
    // characteristic may be absent, or writes may be failing. If those states
    // are masked by LIVE, the field can never report why it is not working.
    var transport = readyTransport();
    transport.onConnectedStateChanged(null, Ble.CONNECTION_STATE_CONNECTED);
    Test.assert(!transport.getDisplayValue().equals("LIVE"));
    transport.stop();
    return true;
}

// --- Advertisement matching -------------------------------------------------
// These use the fake ScanResult from OrmTestDoubles.mc. Reaching finishScan()
// means driving two ticks, since the scan window is counted in compute() ticks.

function ormResult(identity) {
    return new FakeScanResult(ORM_DEVICE_NAME, [ormServiceUuid()], identity);
}

// Drives a ready transport through one full scan window with the given results.
function scanWith(results) {
    var transport = readyTransport();
    transport.tickDiscovery();
    transport.onScanResults(new FakeIterator(results));
    transport.tickDiscovery();
    transport.tickDiscovery();
    return transport;
}

(:test)
function testTwoDistinctReceiversReportMultipleOrm(logger) {
    // Picking one of two would connect to an unpredictable board. Reporting the
    // ambiguity is the documented behaviour and the useful one.
    var transport = scanWith([ormResult(1), ormResult(2)]);
    Test.assertEqual(transport.getDisplayValue(), "MULTIPLE ORM");
    transport.stop();
    return true;
}

(:test)
function testSameReceiverSeenTwiceIsNotMultiple(logger) {
    // A board advertising repeatedly inside one scan window is normal. Treating
    // it as two receivers would wedge the field on MULTIPLE ORM forever.
    var transport = scanWith([ormResult(7), ormResult(7)]);
    Test.assert(!transport.getDisplayValue().equals("MULTIPLE ORM"));
    transport.stop();
    return true;
}

(:test)
function testIgnoresDeviceWithRightNameButWrongService(logger) {
    // The service UUID is what stops an unrelated device called ORM from
    // matching; the name alone must never be enough.
    var wrongService = new FakeScanResult(
        ORM_DEVICE_NAME, [Ble.stringToUuid(TEST_OTHER_UUID_TEXT)], 1);
    var transport = scanWith([wrongService]);
    Test.assertEqual(transport.getDisplayValue(), "NOT FOUND");
    transport.stop();
    return true;
}

(:test)
function testIgnoresDeviceWithRightServiceButWrongName(logger) {
    var wrongName = new FakeScanResult("ORMX", [ormServiceUuid()], 1);
    var transport = scanWith([wrongName]);
    Test.assertEqual(transport.getDisplayValue(), "NOT FOUND");
    transport.stop();
    return true;
}

(:test)
function testIgnoresUnnamedDevice(logger) {
    // Advertisements without a local name are common; getDeviceName() returns
    // null and must not blow up the scan loop.
    var unnamed = new FakeScanResult(null, [ormServiceUuid()], 1);
    var transport = scanWith([unnamed]);
    Test.assertEqual(transport.getDisplayValue(), "NOT FOUND");
    transport.stop();
    return true;
}

(:test)
function testScanResultsIgnoredWhenNotScanning(logger) {
    // Late results from a closed window must not resurrect a finished scan.
    var transport = readyTransport();
    transport.onScanResults(new FakeIterator([ormResult(1)]));
    Test.assertEqual(transport.getDisplayValue(), "INIT");
    transport.stop();
    return true;
}

// --- GATT resolution and write completion -----------------------------------
// The device object is whatever onConnectedStateChanged() is handed, so the
// whole chain below it can be faked. These are the states the field shows when
// a board is present but not actually usable -- the ones that were invisible
// before getDisplayValue() stopped hard-coding LIVE.

function telemetryUuid() {
    return Ble.stringToUuid(TEST_TELEMETRY_UUID_TEXT);
}

// A device exposing the ORM service with the telemetry characteristic.
function workingDevice() {
    var characteristic = new FakeCharacteristic(telemetryUuid());
    var service = new FakeService([characteristic]);
    return new FakeDevice([[ormServiceUuid(), service]], true);
}

function connectedTo(device) {
    var transport = readyTransport();
    transport.onConnectedStateChanged(device, Ble.CONNECTION_STATE_CONNECTED);
    transport.resolveCharacteristic();
    return transport;
}

(:test)
function testMissingServiceIsReported(logger) {
    // Connected to something that advertised ORM but exposes no such service.
    var transport = connectedTo(new FakeDevice([], true));
    Test.assertEqual(transport.getDisplayValue(), "NO SERVICE");
    transport.stop();
    return true;
}

(:test)
function testMissingCharacteristicIsReported(logger) {
    // Service present, telemetry characteristic absent -- a firmware mismatch.
    var emptyService = new FakeService([]);
    var device = new FakeDevice([[ormServiceUuid(), emptyService]], true);
    var transport = connectedTo(device);
    Test.assertEqual(transport.getDisplayValue(), "NO DATA");
    transport.stop();
    return true;
}

(:test)
function testResolvedCharacteristicDoesNotYetClaimLive(logger) {
    // Finding the characteristic is not streaming. LIVE has to wait for an
    // acknowledged write.
    var transport = connectedTo(workingDevice());
    Test.assert(!transport.getDisplayValue().equals("LIVE"));
    transport.stop();
    return true;
}

(:test)
function testAcknowledgedWriteReportsLive(logger) {
    var transport = connectedTo(workingDevice());
    transport.onCharacteristicWrite(new FakeCharacteristic(telemetryUuid()),
                                    Ble.STATUS_SUCCESS);
    Test.assertEqual(transport.getDisplayValue(), "LIVE");
    transport.stop();
    return true;
}

(:test)
function testFailedWriteIsReported(logger) {
    var transport = connectedTo(workingDevice());
    transport.onCharacteristicWrite(new FakeCharacteristic(telemetryUuid()),
                                    Ble.STATUS_WRITE_FAIL);
    Test.assertEqual(transport.getDisplayValue(), "WRITE ERR");
    transport.stop();
    return true;
}

(:test)
function testWriteCompletionForAnotherCharacteristicIsIgnored(logger) {
    // Reaching the UUID check requires a resolved characteristic; without one
    // the callback returns early and this would pass for the wrong reason.
    var transport = connectedTo(workingDevice());
    transport.onCharacteristicWrite(new FakeCharacteristic(telemetryUuid()),
                                    Ble.STATUS_SUCCESS);
    Test.assertEqual(transport.getDisplayValue(), "LIVE");

    transport.onCharacteristicWrite(
        new FakeCharacteristic(Ble.stringToUuid(TEST_OTHER_UUID_TEXT)),
        Ble.STATUS_WRITE_FAIL);
    Test.assertEqual(transport.getDisplayValue(), "LIVE");
    transport.stop();
    return true;
}

(:test)
function testDisconnectedDeviceIsNotResolved(logger) {
    // A device that dropped between the callback and the next tick must not be
    // walked for services.
    var transport = connectedTo(new FakeDevice([], false));
    Test.assert(!transport.getDisplayValue().equals("NO SERVICE"));
    transport.stop();
    return true;
}
