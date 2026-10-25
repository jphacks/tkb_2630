#!/bin/sh
# Flash the built firmware over the kit's on-board J-Link and restart the MCU.
#
# `make program` connects as PSC3xxF_tm, which sometimes fails with
# "InitTarget() failed: No PCode loaded" and can leave the core locked up.
# Attaching as PSC3xxF programs and resets reliably.
#
#   make build && ./flash.sh
#
# The J-Link is picked by serial number (DOOR_JLINK_SERIAL, as in host/door_link.py):
# the magnetic sensor kit's J-Link has the same name, so with both plugged in JLinkExe
# could otherwise flash the wrong board.
set -e
cd "$(dirname "$0")"
HEX=$(ls build/APP_KIT_PSC3M5_CC2/*/*.hex | head -1)
SERIAL=${DOOR_JLINK_SERIAL-591234791}
CMD=$(mktemp)
trap 'rm -f "$CMD"' EXIT
printf 'connect\nhalt\nloadfile %s\nr\ng\nexit\n' "$PWD/$HEX" > "$CMD"
JLinkExe -NoGui 1 ${SERIAL:+-USB $SERIAL} -Device PSC3xxF -If SWD -Speed 4000 -CommandFile "$CMD"
