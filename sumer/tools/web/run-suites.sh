#!/bin/sh
# Runs the browser suites against the test build on port 8123 (build.sh, then serve.sh), one after the other.
# Playwright must be installed; set PLAYWRIGHT to its node module path if it is not found.
# Logs go to tools/out/web-logs/. Exits 1 if any suite fails.
cd "$(dirname "$0")"
mkdir -p ../out/web-logs
status=0
for t in mp mp2 mp3 mp6 pvp audio; do
    echo "=== $t"
    node $t.js > ../out/web-logs/$t.log 2>&1 || status=1
    grep -E "PASS|FAIL|ERROR|passed|FAILED" ../out/web-logs/$t.log
done
exit $status
