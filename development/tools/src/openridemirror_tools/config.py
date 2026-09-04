from __future__ import annotations

import shutil
import tomllib
from pathlib import Path
from typing import Any

from .paths import config_path, repo_root, state_dir

SUPPORTED_BOARD = "waveshare-esp32-s3-rlcd-4.2"
SUPPORTED_DEVICES = (
    "fenix7",
    "fenix7s",
    "fenix7x",
    "fenix7pro",
    "fenix7pronowifi",
    "fenix7spro",
    "fenix7xpro",
    "fenix7xpronowifi",
)


def configure(force: bool = False) -> Path:
    destination = config_path()
    if destination.exists() and not force:
        return destination
    state_dir().mkdir(parents=True, exist_ok=True)
    shutil.copyfile(repo_root() / "development" / "openridemirror.example.toml", destination)
    return destination


def load(require: bool = True) -> dict[str, Any]:
    path = config_path()
    if not path.exists():
        if require:
            raise FileNotFoundError("Run 'orm configure' first")
        return {}
    with path.open("rb") as handle:
        config = tomllib.load(handle)
    validate(config)
    return config


def validate(config: dict[str, Any]) -> None:
    if config.get("schema_version") != 1:
        raise ValueError("schema_version must be 1")
    hardware = config.get("hardware", {})
    if hardware.get("board") != SUPPORTED_BOARD:
        raise ValueError(f"hardware.board must be {SUPPORTED_BOARD!r}")
    garmin = config.get("garmin", {})
    targets = garmin.get("targets", [])
    unsupported = sorted(set(targets) - set(SUPPORTED_DEVICES))
    if unsupported:
        raise ValueError("unsupported Garmin targets: " + ", ".join(unsupported))
    firmware = config.get("firmware", {})
    if firmware.get("mode", "live") not in {"live", "demo"}:
        raise ValueError("firmware.mode must be 'live' or 'demo'")
    map_config = config.get("map", {})
    if map_config.get("preset", "balanced") not in {"minimal", "balanced", "detailed"}:
        raise ValueError("map.preset must be minimal, balanced or detailed")
    area_type = map_config.get("area_type", "sample")
    if area_type not in {"sample", "bbox", "center-radius", "gpx-buffer"}:
        raise ValueError("unsupported map.area_type")
    if area_type == "bbox":
        bounds = map_config.get("bbox")
        if not isinstance(bounds, list) or len(bounds) != 4:
            raise ValueError("map.bbox must be [south, west, north, east]")
    if area_type == "center-radius":
        if not isinstance(map_config.get("center"), list) or len(map_config["center"]) != 2:
            raise ValueError("map.center must be [latitude, longitude]")
        if float(map_config.get("radius_km", 0)) <= 0:
            raise ValueError("map.radius_km must be positive")
    if area_type == "gpx-buffer":
        if not map_config.get("gpx") or float(map_config.get("buffer_km", 0)) <= 0:
            raise ValueError("gpx-buffer requires map.gpx and positive map.buffer_km")


def display(config: dict[str, Any]) -> str:
    lines = [f"schema_version = {config['schema_version']}"]
    for section, values in config.items():
        if section == "schema_version" or not isinstance(values, dict):
            continue
        lines.append(f"\n[{section}]")
        for key, value in values.items():
            lines.append(f"{key} = {value!r}")
    return "\n".join(lines)
