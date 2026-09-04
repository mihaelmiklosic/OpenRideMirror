// SPDX-License-Identifier: GPL-3.0-only
using Toybox.Activity as Activity;
using Toybox.Lang as Lang;
using Toybox.Math as Math;
using Toybox.System as System;
using Toybox.UserProfile as UserProfile;

module OrmProtocol {
    const PROTOCOL_VERSION = 1;
    const PACKET_ACTIVITY = 0x10;
    const PACKET_GPS = 0x11;
    const PACKET_EXTENDED = 0x12;
    const UNKNOWN_U8 = 0xff;
    const UNKNOWN_U16 = 0xffff;
    const UNKNOWN_S16 = -32768;

    // Little-endian, exactly 20 bytes:
    //  0 type, 1 version, 2 sequence, 3 timer state
    //  4 sport, 5 sub-sport, 6 heart rate, 7 reserved
    //  8..11 timer seconds, 12..15 distance decimeters
    // 16..17 speed cm/s, 18..19 reserved
    function activityPacket(info, profile, sequence) {
        var packet = new [20]b;
        packet[0] = PACKET_ACTIVITY;
        packet[1] = PROTOCOL_VERSION;
        packet[2] = sequence & 0xff;
        packet[3] = timerStateCode(info.timerState);
        packet[4] = sportCode(profile);
        packet[5] = UNKNOWN_U8;
        packet[6] = valueU8(info.currentHeartRate);
        // Reserved in protocol v1.
        packet[7] = UNKNOWN_U8;

        putU32(packet, 8, millisecondsToSeconds(info.timerTime));
        putU32(packet, 12, metersToDecimeters(info.elapsedDistance));
        putU16(packet, 16, metersPerSecondToCentimeters(info.currentSpeed));
        // Reserved in protocol v1.
        putU16(packet, 18, UNKNOWN_U16);
        return packet;
    }

    // Little-endian, exactly 20 bytes:
    //  0 type, 1 version, 2 sequence, 3 GPS quality
    //  4..7 latitude E7, 8..11 longitude E7
    // 12..13 altitude decimeters, 14..15 heading centidegrees
    // 16..19 activity timer seconds (timestamp for this position)
    function gpsPacket(info, sequence) {
        if (info.currentLocation == null) {
            return null;
        }

        var coordinates = info.currentLocation.toDegrees() as Lang.Array<Lang.Double>;
        var packet = new [20]b;
        packet[0] = PACKET_GPS;
        packet[1] = PROTOCOL_VERSION;
        packet[2] = sequence & 0xff;
        packet[3] = valueU8(info.currentLocationAccuracy);
        putS32(packet, 4, degreesToE7(coordinates[0]));
        putS32(packet, 8, degreesToE7(coordinates[1]));
        putS16(packet, 12, metersToSignedDecimeters(info.altitude));
        putU16(packet, 14, radiansToCentidegrees(info.currentHeading));
        putU32(packet, 16, millisecondsToSeconds(info.timerTime));
        return packet;
    }

    // Extended cycling/statistics packet:
    //  0 type, 1 version, 2 sequence, 3 current HR zone
    //  4 average HR, 5 max HR, 6..7 average speed cm/s
    //  8..9 max speed cm/s, 10..11 total ascent dm
    // 12..13 calories kcal, 14 hour, 15 minute, 16 second
    // 17..19 reserved
    function extendedPacket(info, profile, sequence) {
        var clock = System.getClockTime();
        var packet = new [20]b;
        packet[0] = PACKET_EXTENDED;
        packet[1] = PROTOCOL_VERSION;
        packet[2] = sequence & 0xff;
        packet[3] = heartRateZone(info.currentHeartRate, profile);
        packet[4] = valueU8(info.averageHeartRate);
        packet[5] = valueU8(info.maxHeartRate);
        putU16(packet, 6, metersPerSecondToCentimeters(info.averageSpeed));
        putU16(packet, 8, metersPerSecondToCentimeters(info.maxSpeed));
        putU16(packet, 10, metersToUnsignedDecimeters(info.totalAscent));
        putU16(packet, 12, valueU16(info.calories));
        packet[14] = valueU8(clock.hour);
        packet[15] = valueU8(clock.min);
        packet[16] = valueU8(clock.sec);
        packet[17] = UNKNOWN_U8;
        putU16(packet, 18, UNKNOWN_U16);
        return packet;
    }

    function timerStateCode(value) {
        if (value == Activity.TIMER_STATE_OFF) { return 0; }
        if (value == Activity.TIMER_STATE_STOPPED) { return 1; }
        if (value == Activity.TIMER_STATE_PAUSED) { return 2; }
        if (value == Activity.TIMER_STATE_ON) { return 3; }
        return UNKNOWN_U8;
    }

    function sportCode(profile) {
        if (profile == null || profile.sport == null) { return UNKNOWN_U8; }
        if (profile.sport == Activity.SPORT_RUNNING) { return 1; }
        if (profile.sport == Activity.SPORT_CYCLING) { return 2; }
        if (profile.sport == Activity.SPORT_WALKING) { return 3; }
        if (profile.sport == Activity.SPORT_HIKING) { return 4; }
        if (profile.sport == Activity.SPORT_SWIMMING) { return 5; }
        if (profile.sport == Activity.SPORT_E_BIKING) { return 6; }
        return 0;
    }

    function heartRateZone(heartRate, profile) {
        if (heartRate == null) {
            return UNKNOWN_U8;
        }

        var zoneSport = UserProfile.HR_ZONE_SPORT_GENERIC;
        if (profile != null) {
            if (profile.sport == Activity.SPORT_CYCLING ||
                    profile.sport == Activity.SPORT_E_BIKING) {
                zoneSport = UserProfile.HR_ZONE_SPORT_BIKING;
            } else if (profile.sport == Activity.SPORT_RUNNING ||
                    profile.sport == Activity.SPORT_WALKING) {
                zoneSport = UserProfile.HR_ZONE_SPORT_RUNNING;
            } else if (profile.sport == Activity.SPORT_SWIMMING) {
                zoneSport = UserProfile.HR_ZONE_SPORT_SWIMMING;
            }
        }

        try {
            var zones = UserProfile.getHeartRateZones(zoneSport);
            if (zones == null || zones.size() < 6) {
                return UNKNOWN_U8;
            }
            var bpm = heartRate.toNumber();
            if (bpm < zones[0]) {
                return 0;
            }
            for (var index = 1; index < 6; index += 1) {
                if (bpm <= zones[index]) {
                    return index;
                }
            }
            return 5;
        } catch (exception) {
            return UNKNOWN_U8;
        }
    }

    function valueU8(value) {
        if (value == null) {
            return UNKNOWN_U8;
        }
        return clamp(value.toNumber(), 0, 254);
    }

    function valueU16(value) {
        if (value == null) {
            return UNKNOWN_U16;
        }
        return clamp(value.toNumber(), 0, 65534);
    }

    function millisecondsToSeconds(value) {
        if (value == null) {
            return 0;
        }
        return clamp((value / 1000).toNumber(), 0, 2147483647);
    }

    function metersToDecimeters(value) {
        if (value == null) {
            return 0;
        }
        return clamp((value * 10.0).toNumber(), 0, 2147483647);
    }

    function metersPerSecondToCentimeters(value) {
        if (value == null) {
            return UNKNOWN_U16;
        }
        return clamp((value * 100.0).toNumber(), 0, 65534);
    }

    function metersToUnsignedDecimeters(value) {
        if (value == null) {
            return UNKNOWN_U16;
        }
        return clamp((value * 10.0).toNumber(), 0, 65534);
    }

    function degreesToE7(value) {
        return (value * 10000000.0d).toNumber();
    }

    function metersToSignedDecimeters(value) {
        if (value == null) {
            return UNKNOWN_S16;
        }
        return clamp((value * 10.0).toNumber(), -32767, 32767);
    }

    function radiansToCentidegrees(value) {
        if (value == null) {
            return UNKNOWN_U16;
        }

        var degrees = value * 180.0 / Math.PI;
        if (degrees < 0.0) {
            degrees += 360.0;
        }
        return clamp((degrees * 100.0).toNumber(), 0, 35999);
    }

    function clamp(value, minimum, maximum) {
        if (value < minimum) {
            return minimum;
        }
        if (value > maximum) {
            return maximum;
        }
        return value;
    }

    function putU16(buffer as Lang.ByteArray, offset as Lang.Number, value as Lang.Number) as Void {
        buffer[offset] = value & 0xff;
        buffer[offset + 1] = (value >> 8) & 0xff;
    }

    function putS16(buffer as Lang.ByteArray, offset as Lang.Number, value as Lang.Number) as Void {
        putU16(buffer, offset, value);
    }

    function putU32(buffer as Lang.ByteArray, offset as Lang.Number, value as Lang.Number) as Void {
        buffer[offset] = value & 0xff;
        buffer[offset + 1] = (value >> 8) & 0xff;
        buffer[offset + 2] = (value >> 16) & 0xff;
        buffer[offset + 3] = (value >> 24) & 0xff;
    }

    function putS32(buffer as Lang.ByteArray, offset as Lang.Number, value as Lang.Number) as Void {
        putU32(buffer, offset, value);
    }
}
