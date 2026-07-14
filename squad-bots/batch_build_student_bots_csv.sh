#!/bin/bash
# batch_build_student_bots_csv.sh
#
# Batch flash student bots from a CSV roster with one-board-at-a-time confirmation.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_SCRIPT="$SCRIPT_DIR/build_student_bots.sh"
CSV_FILE="$SCRIPT_DIR/bakken-campers-summer-2026.csv"

if [ ! -x "$BUILD_SCRIPT" ]; then
  echo "ERROR: build_student_bots.sh not found or not executable in $SCRIPT_DIR"
  exit 1
fi

if [ ! -f "$CSV_FILE" ]; then
  echo "ERROR: roster file not found: $CSV_FILE"
  exit 1
fi

print_header() {
  echo ""
  echo "========================================"
  echo "$1"
  echo "========================================"
}

print_footer() {
  echo ""
  echo "----------------------------------------"
  echo "$1"
  echo "----------------------------------------"
  echo ""
}

to_lower() {
  echo "$1" | tr '[:upper:]' '[:lower:]'
}

prompt_confirm() {
  local answer
  read -r -p "$1" answer </dev/tty
  answer="$(to_lower "$answer")"
  [ "$answer" = "y" ] || [ "$answer" = "yes" ]
}

print_header "CSV Batch Flash: $CSV_FILE"

wait_for_port() {
  local ports existing answer

  while true; do
    existing=()
    for ports in /dev/cu.usbserial* /dev/cu.usbmodem*; do
      if [ -e "$ports" ]; then
        existing+=("$ports")
      fi
    done

    if [ "${#existing[@]}" -eq 1 ]; then
      echo "${existing[0]}"
      return
    fi

    if [ "${#existing[@]}" -gt 1 ]; then
      echo "Found USB serial ports:"
      for ports in "${existing[@]}"; do
        echo "  $ports"
      done
      read -r -p "Enter upload port to use: " answer </dev/tty
      echo "$answer"
      return
    fi

    echo "No USB serial board detected yet. Plug in the board now."
    read -r -p "Press ENTER once the device appears, or enter a port manually: " answer </dev/tty
    if [ -n "$answer" ]; then
      echo "$answer"
      return
    fi
  done
}

wait_for_disconnect() {
  local port="$1"
  echo "Unplug flashed board ($port) before continuing."
  while [ -e "$port" ]; do
    sleep 1
  done
}

row=1
while IFS=',' read -r camper_name droid_name wifi_ssid _; do
  row=$((row + 1))
  wifi_ssid="${wifi_ssid//\"/}"
  if [ -z "$wifi_ssid" ]; then
    continue
  fi

  BOT_NAME="$wifi_ssid"
  BOARD_ENV="l0-0n-devkit"

  echo ""
  echo "Row $row: camper='$camper_name' droid='$droid_name' ssid='$wifi_ssid'"
  echo "Plug in the board for $wifi_ssid now."
  UPLOAD_PORT="$(wait_for_port)"

  if [ -z "$UPLOAD_PORT" ]; then
    echo "Upload port is required. Skipping $wifi_ssid."
    continue
  fi

  echo "Ready to flash $wifi_ssid on $UPLOAD_PORT"
  if ! prompt_confirm "Proceed? (y/N): "; then
    echo "Skipping $wifi_ssid"
    continue
  fi

  "$BUILD_SCRIPT" "$BOT_NAME" "$UPLOAD_PORT" "$BOARD_ENV"

  wait_for_disconnect "$UPLOAD_PORT"

  print_footer "Done: $wifi_ssid"

done < <(tail -n +2 "$CSV_FILE")

print_footer "Completed CSV batch flash run."
