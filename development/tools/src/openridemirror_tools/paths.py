from __future__ import annotations

import os
from pathlib import Path


def _is_repo_root(candidate: Path) -> bool:
    return (
        (candidate / "orm").is_file()
        and (candidate / "development" / "protocol" / "orm-protocol.json").is_file()
    )


def _ancestors(start: Path):
    current = start if start.is_dir() else start.parent
    yield current
    yield from current.parents


def repo_root() -> Path:
    override = os.environ.get("OPENRIDEMIRROR_ROOT")
    if override:
        candidate = Path(override).expanduser().resolve()
        if _is_repo_root(candidate):
            return candidate
        raise RuntimeError(
            f"OPENRIDEMIRROR_ROOT does not point to an OpenRideMirror repository: {candidate}"
        )

    seen: set[Path] = set()
    for start in (Path(__file__).resolve(), Path.cwd().resolve()):
        for candidate in _ancestors(start):
            if candidate in seen:
                continue
            seen.add(candidate)
            if _is_repo_root(candidate):
                return candidate
    raise RuntimeError("Could not locate the OpenRideMirror repository root")


def state_dir() -> Path:
    return repo_root() / ".orm"


def config_path() -> Path:
    return state_dir() / "config.toml"
