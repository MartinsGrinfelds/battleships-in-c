#!/bin/sh
# Launch server and two clients in separate terminal windows (WSL/Linux).
# Tries common terminal emulators: gnome-terminal, konsole, xterm. Falls back to background run.
# Usage: ./scripts/run_demo_terminals.sh
set -e
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)/.."
cd "$ROOT_DIR"

echo "Building project..."
make

LAUNCH_SERVER_CMD="cd '$ROOT_DIR' && ./BattleshipsServer > server.log 2>&1; exec bash"
LAUNCH_CLIENT1_CMD="cd '$ROOT_DIR' && ./BattleshipsClient > client1.log 2>&1; exec bash"
LAUNCH_CLIENT2_CMD="cd '$ROOT_DIR' && ./BattleshipsClient > client2.log 2>&1; exec bash"

if command -v gnome-terminal >/dev/null 2>&1; then
  echo "Using gnome-terminal"
  gnome-terminal -- bash -lc "$LAUNCH_SERVER_CMD" &
  sleep 0.5
  gnome-terminal -- bash -lc "$LAUNCH_CLIENT1_CMD" &
  sleep 0.5
  gnome-terminal -- bash -lc "$LAUNCH_CLIENT2_CMD" &
  exit 0
fi

if command -v konsole >/dev/null 2>&1; then
  echo "Using konsole"
  konsole -e bash -lc "$LAUNCH_SERVER_CMD" &
  sleep 0.5
  konsole -e bash -lc "$LAUNCH_CLIENT1_CMD" &
  sleep 0.5
  konsole -e bash -lc "$LAUNCH_CLIENT2_CMD" &
  exit 0
fi

if command -v xterm >/dev/null 2>&1; then
  echo "Using xterm"
  xterm -hold -e "cd '$ROOT_DIR' && ./BattleshipsServer > server.log 2>&1" &
  sleep 0.5
  xterm -hold -e "cd '$ROOT_DIR' && ./BattleshipsClient > client1.log 2>&1" &
  sleep 0.5
  xterm -hold -e "cd '$ROOT_DIR' && ./BattleshipsClient > client2.log 2>&1" &
  exit 0
fi

# Fallback: run in background and inform user
echo "No supported terminal emulator found. Running in background (logs: server.log/client1.log/client2.log)."
./BattleshipsServer > server.log 2>&1 &
SERVER_PID=$!
sleep 1
./BattleshipsClient > client1.log 2>&1 &
CLIENT1_PID=$!
sleep 1
./BattleshipsClient > client2.log 2>&1 &
CLIENT2_PID=$!

echo "Started: server=$SERVER_PID client1=$CLIENT1_PID client2=$CLIENT2_PID"
echo "Tail server logs with: tail -f server.log"

wait
