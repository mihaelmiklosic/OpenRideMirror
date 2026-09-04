from __future__ import annotations

import os
from pathlib import Path


def repo_root() -> Path:
    override = os.environ.get("OPENRIDEMIRROR_ROOT")
    if override:
        return Path(override).expanduser().resolve()
    candidate = Path(__file__).resolve().parents[3]
    if (candidate / "protocol" / "orm-protocol.json").exists():
        return candidate
    current = Path.cwd().resolve()
    for parent in (current, *current.parents):
        if (parent / "protocol" / "orm-protocol.json").exists():
            return parent
    raise RuntimeError("Could not locate the OpenRideMirror repository root")


def state_dir() -> Path:
    return repo_root() / ".orm"


def config_path() -> Path:
    return state_dir() / "config.toml"
