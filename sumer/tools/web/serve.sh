#!/bin/sh
# Serves the test build on port 8123 (the browser suites expect it): sh tools/web/serve.sh &
# In test mode (--test) new accounts cost no work and nothing is rate-limited, each suite opens rooms of its
# own (survival-*, pvp-*, royale-*), and /test/host lets the suites ask the server program about its world.
# The accounts and worlds of earlier runs are cleared first.
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT/tools/out/web-test" && rm -rf data && exec python3 server.py --port "${PORT:-8123}" --test --host-bin "$ROOT/tools/out/web-test/sumer-host" \
    --data "$ROOT/tools/out/web-test/data" --max-active 40 --idle 20
