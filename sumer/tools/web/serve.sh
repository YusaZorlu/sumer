#!/bin/sh
# Serves the test build on port 8123 (the browser suites expect it; the battle royale's pages add ?mode=br): sh tools/web/serve.sh &
# The production build in web/ is served by: python3 server.py --port 8124 (the click tests expect that).
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT/tools/out/web-test" && exec python3 server.py --port "${PORT:-8123}"
