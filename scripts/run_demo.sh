#!/bin/sh
# Simple demo runner for WSL / Linux
# - Builds the project with `make`
# - Starts server and two clients in background, logging to files
# Usage: ./scripts/run_demo.sh
set -e
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)/.."
cd "$ROOT_DIR"

echo "Building project..."
make

echo "Starting server (logs -> server.log)..."
./BattleshipsServer > server.log 2>&1 &
SERVER_PID=$!

sleep 1

echo "Starting client 1 (logs -> client1.log)..."
./BattleshipsClient > client1.log 2>&1 &
CLIENT1_PID=$!

sleep 1

echo "Starting client 2 (logs -> client2.log)..."
./BattleshipsClient > client2.log 2>&1 &
CLIENT2_PID=$!

echo "Started: server=$SERVER_PID client1=$CLIENT1_PID client2=$CLIENT2_PID"
echo "To stop: kill $SERVER_PID $CLIENT1_PID $CLIENT2_PID"
echo "Tail server logs with: tail -f server.log"

# Wait for background processes (so script doesn't exit immediately)
wait $SERVER_PID $CLIENT1_PID $CLIENT2_PID
