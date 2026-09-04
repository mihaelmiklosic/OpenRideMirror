#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(dirname "$SCRIPT_DIR")
cd "$PROJECT_DIR"

if ! command -v python3 >/dev/null 2>&1; then
  echo "Python 3 is missing. Install Python 3.11 or newer, then rerun this script."
  exit 1
fi

python3 -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 11) else 1)' || {
  echo "Python 3.11 or newer is required."
  exit 1
}

if [ ! -d .venv ]; then
  python3 -m venv .venv
fi

.venv/bin/python -m pip install -e tools
.venv/bin/orm configure

echo
echo "OpenRideMirror local tools are ready."
echo "Next: source .venv/bin/activate"
echo "Then: orm doctor"
echo "Follow docs/beginner-guide.md for Arduino, Garmin and MTP setup."
