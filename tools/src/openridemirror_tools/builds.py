from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

from .config import SUPPORTED_DEVICES
from .paths import repo_root, state_dir

FQBN = "esp32:esp32:esp32s3:CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,UploadMode=default,UploadSpeed=921600,USBMode=hwcdc"
APP_PARTITION_BYTES = 3_145_728


def find_arduino_cli() -> Path | None:
    candidates = [
        os.environ.get("ARDUINO_CLI"),
        shutil.which("arduino-cli"),
        "/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli",
    ]
    return next((Path(item) for item in candidates if item and Path(item).is_file()), None)


def connectiq_sdk() -> Path | None:
    override = os.environ.get("CONNECTIQ_HOME")
    if override and (Path(override) / "bin" / "monkeyc").exists():
        return Path(override)
    config = Path.home() / "Library/Application Support/Garmin/ConnectIQ/current-sdk.cfg"
    if config.exists():
        path = Path(config.read_text().strip())
        if (path / "bin" / "monkeyc").exists():
            return path
    return None


def java_bin_dir() -> Path | None:
    java_home = os.environ.get("JAVA_HOME")
    candidates = [
        Path(java_home) / "bin" if java_home else None,
        Path("/opt/homebrew/opt/openjdk@17/bin"),
        Path("/usr/local/opt/openjdk@17/bin"),
    ]
    return next((path for path in candidates if path and (path / "java").exists()), None)


def run(command: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None) -> str:
    result = subprocess.run(command, cwd=cwd, env=env, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, check=False)
    if result.returncode:
        raise RuntimeError(result.stdout.strip() or f"command failed: {' '.join(command)}")
    return result.stdout


def doctor() -> dict[str, dict[str, Any]]:
    checks: dict[str, dict[str, Any]] = {}
    checks["python"] = {"ok": sys.version_info >= (3, 11), "value": sys.version.split()[0], "required": ">=3.11"}
    arduino = find_arduino_cli()
    checks["arduino_cli"] = {"ok": arduino is not None, "value": str(arduino) if arduino else None}
    sdk = connectiq_sdk()
    checks["connectiq_sdk"] = {"ok": sdk is not None, "value": str(sdk) if sdk else None, "tested": "9.2.0"}
    java = java_bin_dir()
    checks["java_17"] = {"ok": java is not None, "value": str(java) if java else None}
    openssl = shutil.which("openssl")
    checks["openssl"] = {"ok": openssl is not None, "value": openssl}
    if arduino:
        output = run([str(arduino), "core", "list"])
        checks["esp32_core_3_3_11"] = {"ok": bool(re.search(r"esp32:esp32\s+3\.3\.11", output)), "value": "3.3.11"}
        libraries = run([str(arduino), "lib", "list"])
        checks["u8g2_2_36_18"] = {"ok": bool(re.search(r"^U8g2\s+2\.36\.18", libraries, re.MULTILINE)), "value": "2.36.18"}
    return checks


def stage_firmware() -> Path:
    source = repo_root() / "firmware" / "esp32" / "OpenRideMirror"
    destination = state_dir() / "staging" / "OpenRideMirror"
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination)
    generated = state_dir() / "generated" / "map"
    for name in ("OrmMapData.h", "OrmMapLabels.h", "OrmGreenMask.h"):
        if (generated / name).exists():
            shutil.copyfile(generated / name, destination / name)
    return destination


def build_esp(mode: str = "live") -> dict[str, Any]:
    if mode not in {"live", "demo"}:
        raise ValueError("ESP mode must be live or demo")
    arduino = find_arduino_cli()
    if arduino is None:
        raise RuntimeError("arduino-cli not found; run 'orm doctor'")
    sketch = stage_firmware()
    build_dir = state_dir() / "build" / f"esp-{mode}"
    build_dir.mkdir(parents=True, exist_ok=True)
    command = [str(arduino), "compile", "--fqbn", FQBN, "--build-path", str(build_dir)]
    if mode == "demo":
        command.extend(["--build-property", "compiler.cpp.extra_flags=-DORM_BUILD_DEMO=1"])
    command.append(str(sketch))
    output = run(command, cwd=repo_root())
    match = re.search(r"Sketch uses (\d+) bytes", output)
    used = int(match.group(1)) if match else None
    percent = used * 100 / APP_PARTITION_BYTES if used is not None else None
    if percent is not None and percent >= 95:
        raise RuntimeError(f"firmware uses {percent:.1f}% of app partition; limit is 95%")
    return {"mode": mode, "build_dir": str(build_dir), "bytes": used, "percent": percent, "output": output.strip()}


def ensure_garmin_key() -> Path:
    key_dir = state_dir() / "keys"
    pem, der = key_dir / "developer_key.pem", key_dir / "developer_key.der"
    if der.exists():
        return der
    openssl = shutil.which("openssl")
    if openssl is None:
        raise RuntimeError("OpenSSL not found; run 'orm doctor'")
    key_dir.mkdir(parents=True, exist_ok=True)
    run([openssl, "genrsa", "-out", str(pem), "4096"])
    run([openssl, "pkcs8", "-topk8", "-inform", "PEM", "-outform", "DER",
         "-in", str(pem), "-out", str(der), "-nocrypt"])
    pem.chmod(0o600)
    der.chmod(0o600)
    return der


def build_garmin(device: str = "fenix7") -> dict[str, Any]:
    if device not in SUPPORTED_DEVICES:
        raise ValueError(f"unsupported Garmin device: {device}")
    sdk = connectiq_sdk()
    java = java_bin_dir()
    if sdk is None or java is None:
        raise RuntimeError("Connect IQ SDK or Java 17 not found; run 'orm doctor'")
    output_dir = state_dir() / "build" / "garmin"
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f"OpenRideMirror-{device}.prg"
    env = os.environ.copy()
    env["PATH"] = str(java) + os.pathsep + env.get("PATH", "")
    log = run([str(sdk / "bin" / "monkeyc"), "-d", device, "-f",
               str(repo_root() / "garmin" / "OpenRideMirror" / "monkey.jungle"),
               "-o", str(output), "-y", str(ensure_garmin_key()), "-r", "-O", "2", "-w"],
              cwd=repo_root() / "garmin" / "OpenRideMirror", env=env)
    return {"device": device, "prg": str(output), "output": log.strip()}


def flash_esp(mode: str = "live", port: str | None = None) -> str:
    arduino = find_arduino_cli()
    if arduino is None:
        raise RuntimeError("arduino-cli not found")
    if port is None:
        ports = sorted(Path("/dev").glob("cu.usbmodem*"))
        if len(ports) != 1:
            raise RuntimeError(f"expected exactly one /dev/cu.usbmodem* port, found {len(ports)}; pass --port")
        port = str(ports[0])
    build_dir = state_dir() / "build" / f"esp-{mode}"
    if not build_dir.exists():
        build_esp(mode)
    return run([str(arduino), "upload", "-p", port, "--fqbn", FQBN,
                "--input-dir", str(build_dir), str(state_dir() / "staging" / "OpenRideMirror")])


def save_doctor_report(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doctor(), indent=2, sort_keys=True) + "\n")
