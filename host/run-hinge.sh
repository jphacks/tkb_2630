#!/bin/sh
# Run the door with the motor mounted for "hinge" (settings: mounts/hinge.json).
# Extra flags pass through, e.g. ./run-hinge.sh --level 1 --open-dir -1
cd "$(dirname "$0")" && exec .venv/bin/python door_link.py run --mount hinge "$@"
