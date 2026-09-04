from __future__ import annotations

import argparse
import asyncio
import json
import os
import subprocess
import sys
from pathlib import Path

from . import __version__
from . import config as config_module
from .builds import build_esp, build_garmin, doctor, flash_esp, setup_esp_toolchain
from .map_builder import build as build_map, install_pack
from .paths import repo_root, state_dir
from .protocol import check_generated, generate
from .simulator import simulate_ble
from .webui import serve_directory, serve_map_ui


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(prog="orm", description="OpenRideMirror setup and build helper")
    root.add_argument("--version", action="version", version=__version__)
    commands = root.add_subparsers(dest="command", required=True)

    doctor_parser = commands.add_parser("doctor", help="check local toolchains")
    doctor_parser.add_argument("--json", action="store_true")
    commands.add_parser("setup", help="install the tested ESP32 Arduino packages")
    demo = commands.add_parser("demo", help="build and flash the self-running ESP32 demo")
    demo.add_argument("--port")
    live = commands.add_parser("live", help="build and flash the Garmin-connected ESP32 firmware")
    live.add_argument("--port")
    garmin_quick = commands.add_parser("garmin", help="build the Garmin data field")
    garmin_quick.add_argument("device", nargs="?", default="fenix7")
    configure = commands.add_parser("configure", help="create .orm/config.toml")
    configure.add_argument("--force", action="store_true")
    config = commands.add_parser("config", help="inspect configuration")
    config_sub = config.add_subparsers(dest="config_command", required=True)
    config_sub.add_parser("show")
    config_sub.add_parser("validate")

    map_parser = commands.add_parser("map", help="build and preview maps")
    map_sub = map_parser.add_subparsers(dest="map_command", required=True)
    map_build = map_sub.add_parser("build")
    map_build.add_argument("--offline", action="store_true")
    map_ui = map_sub.add_parser("ui")
    map_ui.add_argument("--port", type=int, default=8767)
    map_ui.add_argument("--no-open", action="store_true")
    map_preview = map_sub.add_parser("preview")
    map_preview.add_argument("--port", type=int, default=8767)
    map_install = map_sub.add_parser("install", help="install a map pack downloaded from the web builder")
    map_install.add_argument("pack", type=Path)
    map_flash = map_sub.add_parser("flash", help="install a map pack, build it and flash the ESP32")
    map_flash.add_argument("pack", type=Path, nargs="?", help="ZIP path; defaults to the newest ORM map pack in Downloads")
    map_flash.add_argument("--mode", choices=("live", "demo"), default="live")
    map_flash.add_argument("--port")

    simulate = commands.add_parser("simulate", help="run the canonical demo")
    simulate_sub = simulate.add_subparsers(dest="simulate_command", required=True)
    simulate_web = simulate_sub.add_parser("web")
    simulate_web.add_argument("--port", type=int, default=8766)
    simulate_web.add_argument("--no-open", action="store_true")
    simulate_ble_parser = simulate_sub.add_parser("ble")
    simulate_ble_parser.add_argument("--rate", type=float, default=1.0)

    build = commands.add_parser("build", help="build ESP and Garmin source")
    build_sub = build.add_subparsers(dest="build_command", required=True)
    esp = build_sub.add_parser("esp")
    esp.add_argument("--mode", choices=("live", "demo"), default="live")
    garmin = build_sub.add_parser("garmin")
    garmin.add_argument("--device", default="fenix7")
    garmin.add_argument("--all", action="store_true")
    all_build = build_sub.add_parser("all")
    all_build.add_argument("--device", default="fenix7")

    flash = commands.add_parser("flash", help="flash previously built ESP firmware")
    flash_sub = flash.add_subparsers(dest="flash_command", required=True)
    flash_esp_parser = flash_sub.add_parser("esp")
    flash_esp_parser.add_argument("--mode", choices=("live", "demo"), default="live")
    flash_esp_parser.add_argument("--port")

    protocol = commands.add_parser("protocol", help="maintain generated protocol constants")
    protocol_sub = protocol.add_subparsers(dest="protocol_command", required=True)
    protocol_sub.add_parser("generate")
    protocol_sub.add_parser("check")
    commands.add_parser("test", help="run source tests")
    commands.add_parser("release", help="run release checks").add_subparsers(dest="release_command", required=True).add_parser("check")
    return root


def print_doctor(as_json: bool) -> int:
    checks = doctor()
    if as_json:
        print(json.dumps(checks, indent=2, sort_keys=True))
    else:
        for name, result in checks.items():
            print(f"{'OK' if result['ok'] else 'MISSING':7} {name:24} {result.get('value') or ''}")
    return 0 if all(item["ok"] for item in checks.values()) else 1


def newest_downloaded_map_pack() -> Path:
    downloads = Path.home() / "Downloads"
    candidates = [path for path in downloads.glob("OpenRideMirror-map-pack*.zip") if path.is_file()]
    if not candidates:
        raise FileNotFoundError(
            "no OpenRideMirror map pack found in Downloads; download one from the map builder or pass its path"
        )
    return max(candidates, key=lambda path: path.stat().st_mtime)


def run_tests() -> int:
    environment = os.environ.copy()
    tools_source = str(repo_root() / "development" / "tools" / "src")
    inherited_pythonpath = environment.get("PYTHONPATH")
    environment["PYTHONPATH"] = (
        tools_source + (os.pathsep + inherited_pythonpath if inherited_pythonpath else "")
    )
    result = subprocess.run(
        [sys.executable, "-m", "unittest", "discover", "-s", "development/tests", "-v"],
        cwd=repo_root(),
        env=environment,
    )
    return result.returncode


def release_check() -> int:
    errors = check_generated()
    forbidden_extensions = {".prg", ".bin", ".elf", ".pem", ".der", ".key", ".fit"}
    ignored_roots = {".git", ".orm", ".venv", "__pycache__"}
    for path in repo_root().rglob("*"):
        if not path.is_file() or any(part in ignored_roots for part in path.parts):
            continue
        relative = path.relative_to(repo_root())
        if path.suffix.lower() in forbidden_extensions:
            errors.append(f"forbidden artifact: {relative}")
        if path.stat().st_size < 2_000_000:
            try:
                text = path.read_text()
            except UnicodeDecodeError:
                continue
            if "/" + "Users/" in text:
                errors.append(f"private absolute path: {relative}")
            if "EK" + "RANO" in text.upper():
                errors.append(f"legacy identifier: {relative}")
    if run_tests() != 0:
        errors.append("test suite failed")
    for mode in ("demo", "live"):
        result = build_esp(mode)
        if result["percent"] is not None and result["percent"] > 90:
            errors.append(f"release {mode} firmware is {result['percent']:.1f}% (>90%)")
    if errors:
        print("Release check failed:")
        for error in errors:
            print(f"- {error}")
        return 1
    print("Release check passed. Garmin compile and physical hardware checks remain local release gates.")
    return 0


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        if args.command == "doctor":
            return print_doctor(args.json)
        if args.command == "setup":
            config_module.configure(False)
            print(setup_esp_toolchain())
            print("\nGarmin tools are checked separately because Garmin requires you to accept its SDK agreement.")
            print_doctor(False)
            return 0
        if args.command in {"demo", "live"}:
            mode = args.command
            result = build_esp(mode)
            print(f"Built {mode} firmware ({result['bytes']} bytes).")
            print(flash_esp(mode, args.port))
            return 0
        if args.command == "garmin":
            print(json.dumps(build_garmin(args.device), indent=2))
            return 0
        if args.command == "configure":
            print(config_module.configure(args.force))
        elif args.command == "config":
            config = config_module.load()
            if args.config_command == "show": print(config_module.display(config))
            else: print("Configuration is valid.")
        elif args.command == "map":
            if args.map_command == "install":
                print(f"Installed map pack in {install_pack(args.pack)}")
            elif args.map_command == "flash":
                pack = args.pack or newest_downloaded_map_pack()
                print(f"Using map pack: {pack}")
                print(f"Installed map pack in {install_pack(pack)}")
                result = build_esp(args.mode)
                print(f"Built {args.mode} firmware ({result['bytes']} bytes).")
                print(flash_esp(args.mode, args.port))
            else:
                config = config_module.load()
            if args.map_command == "build":
                output, manifest = build_map(config, args.offline)
                print(json.dumps({"output": str(output), **manifest}, indent=2))
            elif args.map_command == "ui": serve_map_ui(config, args.port, not args.no_open)
            elif args.map_command == "preview": serve_map_ui(config, args.port, True)
        elif args.command == "simulate":
            if args.simulate_command == "web": serve_directory(repo_root() / "web-demo", args.port, not args.no_open)
            else: asyncio.run(simulate_ble(args.rate))
        elif args.command == "build":
            if args.build_command == "esp":
                print(json.dumps(build_esp(args.mode), indent=2))
            elif args.build_command == "garmin":
                devices = config_module.SUPPORTED_DEVICES if args.all else (args.device,)
                for device in devices: print(json.dumps(build_garmin(device), indent=2))
            else:
                print(json.dumps(build_esp("live"), indent=2)); print(json.dumps(build_garmin(args.device), indent=2))
        elif args.command == "flash":
            print(flash_esp(args.mode, args.port))
        elif args.command == "protocol":
            if args.protocol_command == "generate": generate(); print("Generated protocol constants.")
            else:
                errors = check_generated()
                if errors:
                    print("\n".join(errors)); return 1
                print("Protocol schema and generated constants agree.")
        elif args.command == "test":
            return run_tests()
        elif args.command == "release":
            return release_check()
        return 0
    except (FileNotFoundError, RuntimeError, ValueError) as error:
        print(f"orm: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
