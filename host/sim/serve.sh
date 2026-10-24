#!/bin/sh
# Serve host/ so the simulator can read host/mounts/*.json (the same settings as the kit).
# Then open http://127.0.0.1:8791/sim/ . Opening index.html directly also works,
# with the defaults built into the page.
cd "$(dirname "$0")/.." && exec python3 -m http.server 8791 --bind 127.0.0.1
