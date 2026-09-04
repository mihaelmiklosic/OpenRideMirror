from __future__ import annotations

import asyncio
import math
import time

from .protocol import ActivityPacket, SERVICE_UUID, TELEMETRY_UUID, encode_activity, encode_extended, encode_gps

ROUTE = [
    (45.78440, 15.90870), (45.78290, 15.91400), (45.77980, 15.91700),
    (45.77770, 15.91300), (45.77840, 15.90700), (45.78130, 15.90500),
    (45.78440, 15.90870),
]


def speed_for(second: float) -> float:
    if second < 20: return 10 + second / 20 * 14
    if second < 45: return 25 + math.sin(second * .25) * 1.5
    if second < 70: return 34 + math.sin(second * .35) * 2
    if second < 90: return 17 + math.sin(second * .20)
    if second < 115: return 38 + (second - 90) / 25 * 4
    if second < 140: return 26 + math.sin(second * .18) * 2
    if second < 158: return 33 + math.sin(second * .30) * 2
    return max(0, 21 - max(0, second - 165) / 15 * 21)


def route_position(progress: float) -> tuple[float, float, int]:
    position = min(.999999, progress) * (len(ROUTE) - 1)
    index, amount = int(position), position - int(position)
    start, end = ROUTE[index], ROUTE[index + 1]
    latitude = start[0] + (end[0] - start[0]) * amount
    longitude = start[1] + (end[1] - start[1]) * amount
    heading = math.degrees(math.atan2((end[1] - start[1]) * math.cos(math.radians(latitude)), end[0] - start[0])) % 360
    return latitude, longitude, round(heading * 100)


async def simulate_ble(rate: float = 1.0) -> None:
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError as error:
        raise RuntimeError(
            "Install the BLE extra first: pip install -e './development/tools[ble]'"
        ) from error
    devices = await BleakScanner.discover(timeout=2.0, return_adv=True)
    matches = []
    for device, advertisement in devices.values():
        uuids = {value.upper() for value in advertisement.service_uuids}
        if device.name == "ORM" and SERVICE_UUID.upper() in uuids:
            matches.append(device)
    if len(matches) != 1:
        raise RuntimeError(f"expected exactly one ORM device, found {len(matches)}")
    distance_dm = 0.0
    sequence = 0
    started = time.monotonic()
    async with BleakClient(matches[0]) as client:
        while True:
            elapsed = (time.monotonic() - started) * rate
            if elapsed >= 180:
                break
            speed_kmh = speed_for(elapsed)
            speed_cms = round(speed_kmh / .036)
            distance_dm += speed_kmh / 3.6 * .5 * rate * 10
            heart_rate = min(184, round(101 + speed_kmh * 1.65))
            zone = 1 if heart_rate < 115 else 2 if heart_rate < 135 else 3 if heart_rate < 152 else 4 if heart_rate < 168 else 5
            sequence = (sequence + 1) & 0xff
            activity = encode_activity(ActivityPacket(sequence, 3, 2, heart_rate, int(elapsed), int(distance_dm), speed_cms))
            await client.write_gatt_char(TELEMETRY_UUID, activity, response=True)
            latitude, longitude, heading = route_position(elapsed / 180)
            sequence = (sequence + 1) & 0xff
            await client.write_gatt_char(TELEMETRY_UUID, encode_gps(
                sequence=sequence, quality=4, latitude_e7=round(latitude * 1e7),
                longitude_e7=round(longitude * 1e7), altitude_decimeters=1180,
                heading_centidegrees=heading, timer_seconds=int(elapsed)), response=True)
            sequence = (sequence + 1) & 0xff
            await client.write_gatt_char(TELEMETRY_UUID, encode_extended(
                sequence=sequence, zone=zone, average_hr=max(90, heart_rate - 8), maximum_hr=heart_rate,
                average_speed=speed_cms, maximum_speed=max(speed_cms, 1100), ascent=round(elapsed * 1.9),
                calories=round(elapsed * .15), hour=time.localtime().tm_hour,
                minute=time.localtime().tm_min, second=time.localtime().tm_sec), response=True)
            await asyncio.sleep(.5)
