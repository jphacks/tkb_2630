#!/bin/sh
# Run the door with the motor mounted for "string" (settings: mounts/string.json).
# Extra flags pass through, e.g. ./run-string.sh --level 1 --open-dir -1
cd "$(dirname "$0")" && exec .venv/bin/python door_link.py run --mount string "$@"
