#!/usr/bin/env bash
# Plays complete games: real server, bots as players, real TCP connections.
# Also attacks server with broken data while game is going on.
# Run from repository root: make integration_test (or make sanitize for memory checks).
set -u
cd "$(dirname "$0")/.."

SERVER=./BattleshipsServer
BOT=./BattleshipsBot
LOG_DIR=$(mktemp -d)
SERVER_PID=""
PASSED=0

cleanup()
{
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
    fi
}
trap cleanup EXIT

fail()
{
    echo "FAIL: $*"
    echo "--- Server log (last 60 lines): $LOG_DIR/server.log"
    tail -60 "$LOG_DIR/server.log"
    exit 1
}

# start_server <test name> <server options...>
start_server()
{
    TEST_NAME=$1
    shift
    PORT=$((20000 + RANDOM % 30000))
    echo "=== $TEST_NAME (port $PORT)"
    $SERVER --port "$PORT" --lobby-wait 1 --placement-time 8 --turn-time 3 --post-game 1 "$@" > "$LOG_DIR/server.log" 2>&1 &
    SERVER_PID=$!
    for _ in $(seq 1 50); do
        grep -q "Starting server loop" "$LOG_DIR/server.log" && return
        kill -0 "$SERVER_PID" 2>/dev/null || fail "server did not start"
        sleep 0.1
    done
    fail "server did not start in time"
}

stop_server()
{
    kill -0 "$SERVER_PID" 2>/dev/null || fail "server crashed during test"
    kill -TERM "$SERVER_PID"
    wait "$SERVER_PID"
    local code=$?
    SERVER_PID=""
    [ "$code" -eq 0 ] || fail "server exit code $code"
    grep -q "Server stopped" "$LOG_DIR/server.log" || fail "server did not stop cleanly"
    if grep -E "AddressSanitizer|LeakSanitizer|runtime error|BUG" "$LOG_DIR/server.log"; then
        fail "memory or logic errors in server log"
    fi
}

# run_bots <count> <bot options...> - waits for all bots and checks that each finished successfully.
run_bots()
{
    local count=$1
    shift
    local pids=()
    for i in $(seq 1 "$count"); do
        timeout 300 $BOT --port "$PORT" --name "Bot $i" --think-ms 0 --quiet "$@" > "$LOG_DIR/bot$i.log" 2>&1 &
        pids+=($!)
    done
    BOT_PIDS=("${pids[@]}")
}

wait_bots()
{
    local i=1
    for pid in "${BOT_PIDS[@]}"; do
        wait "$pid"
        local code=$?
        if [ "$code" -ne 0 ]; then
            cat "$LOG_DIR/bot$i.log"
            fail "bot $i exit code $code"
        fi
        i=$((i + 1))
    done
}

expect_in_log()
{
    grep -qE "$1" "$LOG_DIR/server.log" || fail "server log does not contain: $1"
}

pass()
{
    PASSED=$((PASSED + 1))
    echo "    ok: $(grep -cE 'Wins!|Nobody Wins' "$LOG_DIR/server.log") game(s) finished," \
         "$(grep -cE ' Hit | Sunk ' "$LOG_DIR/server.log") hits"
}

# 1. Two teams, two games in a row (server goes back to lobby after game).
start_server "Two teams, two games" --min-players 4 --max-turns 80
run_bots 4 --games 2
wait_bots
expect_in_log "New Game Lobby Is Open"
stop_server
pass

# 2. Four teams, smaller map.
start_server "Four teams" --min-players 4 --teams 4 --map-width 40 --map-height 40 --max-turns 60
run_bots 4 --games 1
wait_bots
expect_in_log "\(Team 4\)"
stop_server
pass

# 3. Short game where shooting decides (one shot per turn gives few hits, so allow many shots).
start_server "Game until ships are sunk" --min-players 2 --shots-per-turn 10 --max-turns 2000 --map-width 40 --map-height 40
run_bots 2 --games 1
wait_bots
expect_in_log "Only One Team Has Ships Left|All Ships Are Sunk|No Ship Can Reach An Enemy Anymore"
stop_server
pass

# 4. Chaos: packets arrive in pieces, players disconnect and reconnect, broken clients attack server.
start_server "Chaos" --min-players 4 --max-turns 120
run_bots 4 --games 1 --chaos
sleep 1
python3 tests/garbage_client.py "$PORT" 1 3 > "$LOG_DIR/garbage.log" 2>&1 || fail "garbage client failed: $(cat "$LOG_DIR/garbage.log")"
wait_bots
expect_in_log "Dropping packet with wrong checksum"
expect_in_log "Too much clients connected already"
stop_server
pass

# 5. Late joiner becomes observer and still sees game end.
start_server "Observer" --min-players 2 --max-turns 30
run_bots 2 --games 1 --think-ms 150
sleep 2
timeout 120 $BOT --port "$PORT" --name "Watcher" --games 1 --quiet > "$LOG_DIR/observer.log" 2>&1 || { cat "$LOG_DIR/observer.log"; fail "observer did not see game end"; }
wait_bots
expect_in_log "Watcher Joined As Observer"
stop_server
pass

echo "All $PASSED integration tests passed."
