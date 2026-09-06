// SPDX-License-Identifier: GPL-3.0-only
//
// Encoder tests for OrmProtocol.
//
// The golden fixtures in development/protocol/fixtures/golden-packets.json are
// checked against the Python decoder by `orm test`. Nothing checked that the
// Monkey C encoder -- the code that actually puts bytes on the wire -- produces
// those same bytes. A wrong unit conversion or a swapped offset would ship
// silently and only show up as nonsense on the display, with the watch
// connected and nothing left to bisect.
//
// These tests pin the conversions at their boundaries and rebuild the activity
// packet field by field, comparing it to the golden bytes. The byte string
// below is duplicated from that fixture on purpose: a Python test
// (test_monkeyc_golden.py) fails if the two ever drift apart, which is the only
// way to keep them tied without reading files from Monkey C.
using Toybox.Test as Test;
using Toybox.Math as Math;
using Toybox.Lang as Lang;

// development/protocol/fixtures/golden-packets.json -> "activity"
const GOLDEN_ACTIVITY = [
    0x10, 0x01, 0x2a, 0x03, 0x02, 0xff, 0x98, 0xff,
    0x7b, 0x00, 0x00, 0x00, 0x50, 0x34, 0x12, 0x00,
    0x7b, 0x00, 0xff, 0xff
]b;

function assertBytesEqual(actual as Lang.ByteArray, expected as Lang.ByteArray,
                          logger) as Void {
    Test.assertEqual(actual.size(), expected.size());
    for (var index = 0; index < expected.size(); index += 1) {
        if (actual[index] != expected[index]) {
            logger.debug("byte " + index + ": got " + actual[index] +
                         " want " + expected[index]);
        }
        Test.assertEqual(actual[index], expected[index]);
    }
}

// --- Whole-packet layout ----------------------------------------------------

(:test)
function testActivityPacketMatchesGoldenBytes(logger) {
    // Same field values the fixture encodes, pushed through the real
    // conversion helpers rather than written out as literals.
    var packet = new [20]b;
    packet[0] = OrmProtocol.PACKET_ACTIVITY;
    packet[1] = OrmProtocol.PROTOCOL_VERSION;
    packet[2] = 42 & 0xff;
    packet[3] = OrmProtocol.timerStateCode(3);
    packet[4] = 2;
    packet[5] = OrmProtocol.UNKNOWN_U8;
    packet[6] = OrmProtocol.valueU8(152);
    packet[7] = OrmProtocol.UNKNOWN_U8;
    OrmProtocol.putU32(packet, 8, OrmProtocol.millisecondsToSeconds(123000));
    OrmProtocol.putU32(packet, 12, OrmProtocol.metersToDecimeters(119304.0));
    OrmProtocol.putU16(packet, 16, OrmProtocol.metersPerSecondToCentimeters(1.23));
    OrmProtocol.putU16(packet, 18, OrmProtocol.UNKNOWN_U16);

    assertBytesEqual(packet, GOLDEN_ACTIVITY, logger);
    return true;
}

(:test)
function testLittleEndianWritersRoundTripThroughTheirOffsets(logger) {
    // A big-endian slip would still produce plausible-looking bytes, so pin the
    // byte order explicitly rather than trusting the whole-packet test alone.
    var buffer = new [20]b;
    OrmProtocol.putU32(buffer, 8, 0x00123450);
    Test.assertEqual(buffer[8], 0x50);
    Test.assertEqual(buffer[9], 0x34);
    Test.assertEqual(buffer[10], 0x12);
    Test.assertEqual(buffer[11], 0x00);

    OrmProtocol.putU16(buffer, 16, 0x0102);
    Test.assertEqual(buffer[16], 0x02);
    Test.assertEqual(buffer[17], 0x01);
    return true;
}

(:test)
function testSignedWritersEncodeNegatives(logger) {
    var buffer = new [20]b;
    OrmProtocol.putS16(buffer, 12, -1);
    Test.assertEqual(buffer[12], 0xff);
    Test.assertEqual(buffer[13], 0xff);

    OrmProtocol.putS32(buffer, 4, -1);
    Test.assertEqual(buffer[4], 0xff);
    Test.assertEqual(buffer[7], 0xff);
    return true;
}

// --- Unknown sentinels ------------------------------------------------------

(:test)
function testMissingValuesBecomeUnknownRatherThanZero(logger) {
    // Zero is a legitimate reading for most of these. Sending it for "no data"
    // would show a confident wrong number instead of an honest blank.
    Test.assertEqual(OrmProtocol.valueU8(null), OrmProtocol.UNKNOWN_U8);
    Test.assertEqual(OrmProtocol.valueU16(null), OrmProtocol.UNKNOWN_U16);
    Test.assertEqual(OrmProtocol.metersPerSecondToCentimeters(null),
                     OrmProtocol.UNKNOWN_U16);
    Test.assertEqual(OrmProtocol.metersToUnsignedDecimeters(null),
                     OrmProtocol.UNKNOWN_U16);
    Test.assertEqual(OrmProtocol.metersToSignedDecimeters(null),
                     OrmProtocol.UNKNOWN_S16);
    Test.assertEqual(OrmProtocol.radiansToCentidegrees(null),
                     OrmProtocol.UNKNOWN_U16);
    return true;
}

(:test)
function testRealValuesNeverCollideWithTheUnknownSentinel(logger) {
    // 0xff/0xffff mean "unknown", so a real reading must never saturate onto
    // them -- a genuinely huge value has to read as large, not as missing.
    Test.assertEqual(OrmProtocol.valueU8(255), 254);
    Test.assertEqual(OrmProtocol.valueU8(100000), 254);
    Test.assertEqual(OrmProtocol.valueU16(65535), 65534);
    Test.assertEqual(OrmProtocol.metersPerSecondToCentimeters(10000.0), 65534);
    Test.assertEqual(OrmProtocol.metersToUnsignedDecimeters(1000000.0), 65534);
    return true;
}

// --- Unit conversions -------------------------------------------------------

(:test)
function testUnitConversions(logger) {
    Test.assertEqual(OrmProtocol.millisecondsToSeconds(123000), 123);
    Test.assertEqual(OrmProtocol.millisecondsToSeconds(1999), 1);
    Test.assertEqual(OrmProtocol.metersToDecimeters(119304.0), 1193040);
    Test.assertEqual(OrmProtocol.metersPerSecondToCentimeters(1.23), 123);
    Test.assertEqual(OrmProtocol.metersToSignedDecimeters(12.34), 123);
    return true;
}

(:test)
function testNegativeAltitudeSurvivesAsNegative(logger) {
    // Below sea level is real. Clamping it to zero would be a silent lie.
    Test.assertEqual(OrmProtocol.metersToSignedDecimeters(-12.3), -123);
    return true;
}

(:test)
function testHeadingNormalisesIntoZeroToThreeSixty(logger) {
    // Garmin reports heading in radians and may hand back a negative angle;
    // the wire format is unsigned centidegrees, so west must not underflow.
    Test.assertEqual(OrmProtocol.radiansToCentidegrees(0.0), 0);
    Test.assertEqual(OrmProtocol.radiansToCentidegrees(Math.PI), 18000);
    Test.assertEqual(OrmProtocol.radiansToCentidegrees(-Math.PI / 2.0), 27000);

    // A full turn must stay in range rather than wrapping to 36000.
    Test.assert(OrmProtocol.radiansToCentidegrees(2.0 * Math.PI) <= 35999);
    Test.assert(OrmProtocol.radiansToCentidegrees(-0.001) <= 35999);
    return true;
}

(:test)
function testCoordinateScalingKeepsSignAndPrecision(logger) {
    Test.assertEqual(OrmProtocol.degreesToE7(45.5555555d), 455555555);
    Test.assertEqual(OrmProtocol.degreesToE7(-45.5555555d), -455555555);
    Test.assertEqual(OrmProtocol.degreesToE7(0.0d), 0);
    return true;
}

(:test)
function testClampBounds(logger) {
    Test.assertEqual(OrmProtocol.clamp(5, 0, 10), 5);
    Test.assertEqual(OrmProtocol.clamp(-1, 0, 10), 0);
    Test.assertEqual(OrmProtocol.clamp(11, 0, 10), 10);
    return true;
}
