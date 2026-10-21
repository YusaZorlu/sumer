#!/bin/sh
# Serves the test build on port 8123 and its battle royale on 8133 (the browser suites expect them): sh tools/web/serve.sh &
# The production build in web/ is served by: python3 server.py --port 8124 (the click tests expect that).
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT/tools/out/web-test" && exec python3 server.py --port "${PORT:-8123}" --br-port "${BR_PORT:-8133}"
