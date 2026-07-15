#!/bin/bash
# batch_build_student_bots_csv.sh
#
# Batch flash student bots from a CSV roster with one-board-at-a-time confirmation.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_SCRIPT="$SCRIPT_DIR/build_student_bots.sh"
CSV_FILE_DEFAULT="$SCRIPT_DIR/bakken-campers-summer-2026.csv"
CSV_FILE="$CSV_FILE_DEFAULT"
BOARD_ENV="l0-0n-devkit"
EXPECTED_COUNT=""
START_AT=1
DRY_RUN=0
AUDIT_LOG="$SCRIPT_DIR/flash_audit_$(date +%Y%m%d-%H%M%S).log"
SSID_KEYS_FILE=""

usage() {
  cat <<EOF
Usage: ./batch_build_student_bots_csv.sh [options]

Options:
  --csv <path>              Path to roster CSV
  --env <platformio_env>    PlatformIO environment (default: l0-0n-devkit)
  --expected-count <n>      Fail if parsed roster count is not exactly n
  --start-at <n>            Start at roster row number n (1-based)
  --dry-run                 Validate and print plan without flashing
  --help                    Show this help

Example:
  ./batch_build_student_bots_csv.sh --expected-count 23
  ./batch_build_student_bots_csv.sh --csv ./bakken-campers-summer-2026.csv --expected-count 23 --start-at 12
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --csv)
      CSV_FILE="$2"
      shift 2
      ;;
    --env)
      BOARD_ENV="$2"
      shift 2
      ;;
    --expected-count)
      EXPECTED_COUNT="$2"
      shift 2
      ;;
    --start-at)
      START_AT="$2"
      shift 2
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "ERROR: Unknown argument: $1"
      usage
      exit 1
      ;;
  esac
done

if [ ! -x "$BUILD_SCRIPT" ]; then
  echo "ERROR: build_student_bots.sh not found or not executable in $SCRIPT_DIR"
  exit 1
fi

if [ ! -f "$CSV_FILE" ]; then
  echo "ERROR: roster file not found: $CSV_FILE"
  exit 1
fi

if [ -n "$EXPECTED_COUNT" ] && ! [[ "$EXPECTED_COUNT" =~ ^[0-9]+$ ]]; then
  echo "ERROR: --expected-count must be a positive integer"
  exit 1
fi

if ! [[ "$START_AT" =~ ^[0-9]+$ ]] || [ "$START_AT" -lt 1 ]; then
  echo "ERROR: --start-at must be an integer >= 1"
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

trim_field() {
  local value="$1"
  value="${value%$'\r'}"
  value="${value#\"}"
  value="${value%\"}"
  echo "$value" | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//'
}

prompt_confirm() {
  local answer
  read -r -p "$1" answer </dev/tty
  answer="$(to_lower "$answer")"
  [ "$answer" = "y" ] || [ "$answer" = "yes" ]
}

print_header "CSV Batch Flash: $CSV_FILE"

cleanup() {
  if [ -n "$SSID_KEYS_FILE" ] && [ -f "$SSID_KEYS_FILE" ]; then
    rm -f "$SSID_KEYS_FILE"
  fi
}

trap cleanup EXIT

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
      # NOTE: status/prompt output below MUST go to stderr (>&2), not stdout.
      # The caller does UPLOAD_PORT="$(wait_for_port)" — anything printed to
      # stdout here gets captured into UPLOAD_PORT and corrupts the port path.
      echo "Found USB serial ports:" >&2
      for ports in "${existing[@]}"; do
        echo "  $ports" >&2
      done
      read -r -p "Enter upload port to use: " answer </dev/tty
      echo "$answer"
      return
    fi

    echo "No USB serial board detected yet. Plug in the board now." >&2
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

SSID_KEYS_FILE="$(mktemp)"

CAMPER_NAMES=()
DROID_NAMES=()
WIFI_SSIDS=()

row=1
while IFS=',' read -r camper_name droid_name wifi_ssid _; do
  row=$((row + 1))
  camper_name="$(trim_field "$camper_name")"
  droid_name="$(trim_field "$droid_name")"
  wifi_ssid="$(trim_field "$wifi_ssid")"
  if [ -z "$wifi_ssid" ]; then
    continue
  fi

  ssid_key="$(to_lower "$wifi_ssid")"
  if grep -Fxq "$ssid_key" "$SSID_KEYS_FILE"; then
    echo "ERROR: Duplicate WiFi SSID '$wifi_ssid' found in CSV (line $row)."
    exit 1
  fi
  echo "$ssid_key" >> "$SSID_KEYS_FILE"

  CAMPER_NAMES+=("$camper_name")
  DROID_NAMES+=("$droid_name")
  WIFI_SSIDS+=("$ssid_key")
done < <(tail -n +2 "$CSV_FILE")

TOTAL="${#WIFI_SSIDS[@]}"

if [ "$TOTAL" -eq 0 ]; then
  echo "ERROR: No valid roster rows found in CSV."
  exit 1
fi

if [ -n "$EXPECTED_COUNT" ] && [ "$TOTAL" -ne "$EXPECTED_COUNT" ]; then
  echo "ERROR: Parsed $TOTAL roster entries but expected $EXPECTED_COUNT."
  echo "Refusing to proceed to avoid flashing the wrong number of boards."
  exit 1
fi

if [ "$START_AT" -gt "$TOTAL" ]; then
  echo "ERROR: --start-at ($START_AT) is greater than roster size ($TOTAL)."
  exit 1
fi

echo "Roster parsed successfully."
echo "  Total entries: $TOTAL"
echo "  Start at:      $START_AT"
echo "  Build env:     $BOARD_ENV"
echo "  Dry run:       $DRY_RUN"
echo ""
echo "Planned roster:"
idx=0
while [ "$idx" -lt "$TOTAL" ]; do
  num=$((idx + 1))
  printf "  %02d. camper='%s' droid='%s' ssid='%s'\n" \
    "$num" "${CAMPER_NAMES[$idx]}" "${DROID_NAMES[$idx]}" "${WIFI_SSIDS[$idx]}"
  idx=$((idx + 1))
done

if [ "$DRY_RUN" -eq 1 ]; then
  print_footer "Dry run complete. No boards were flashed."
  exit 0
fi

echo ""
echo "Audit log: $AUDIT_LOG"
echo "timestamp,row,camper_name,droid_name,wifi_ssid,upload_port,status" > "$AUDIT_LOG"

if ! prompt_confirm "Proceed with this exact roster? (y/N): "; then
  print_footer "Cancelled before flashing."
  exit 0
fi

idx=$((START_AT - 1))
while [ "$idx" -lt "$TOTAL" ]; do
  row_num=$((idx + 1))
  camper_name="${CAMPER_NAMES[$idx]}"
  droid_name="${DROID_NAMES[$idx]}"
  wifi_ssid="${WIFI_SSIDS[$idx]}"

  BOT_NAME="$wifi_ssid"

  echo ""
  echo "Row $row_num/$TOTAL: camper='$camper_name' droid='$droid_name' ssid='$wifi_ssid'"
  echo "Plug in the board for $wifi_ssid now."
  UPLOAD_PORT="$(wait_for_port)"

  if [ -z "$UPLOAD_PORT" ]; then
    echo "Upload port is required. Skipping $wifi_ssid."
    echo "$(date '+%Y-%m-%d %H:%M:%S'),$row_num,$camper_name,$droid_name,$wifi_ssid,,SKIPPED_NO_PORT" >> "$AUDIT_LOG"
    idx=$((idx + 1))
    continue
  fi

  echo "Ready to flash $wifi_ssid on $UPLOAD_PORT"
  if ! prompt_confirm "Proceed? (y/N): "; then
    echo "Skipping $wifi_ssid"
    echo "$(date '+%Y-%m-%d %H:%M:%S'),$row_num,$camper_name,$droid_name,$wifi_ssid,$UPLOAD_PORT,SKIPPED_BY_OPERATOR" >> "$AUDIT_LOG"
    idx=$((idx + 1))
    continue
  fi

  if "$BUILD_SCRIPT" "$BOT_NAME" "$UPLOAD_PORT" "$BOARD_ENV"; then
    echo "$(date '+%Y-%m-%d %H:%M:%S'),$row_num,$camper_name,$droid_name,$wifi_ssid,$UPLOAD_PORT,FLASHED_OK" >> "$AUDIT_LOG"
  else
    echo "$(date '+%Y-%m-%d %H:%M:%S'),$row_num,$camper_name,$droid_name,$wifi_ssid,$UPLOAD_PORT,FLASH_FAILED" >> "$AUDIT_LOG"
    if ! prompt_confirm "Flash failed. Continue to next board? (y/N): "; then
      print_footer "Stopped due to flash failure. Review audit log: $AUDIT_LOG"
      exit 1
    fi
    idx=$((idx + 1))
    continue
  fi

  wait_for_disconnect "$UPLOAD_PORT"

  print_footer "Done: $wifi_ssid"
  idx=$((idx + 1))

  if [ "$idx" -lt "$TOTAL" ]; then
    if ! prompt_confirm "Continue to next roster entry? (y/N): "; then
      print_footer "Stopped by operator. Resume later with --start-at $((idx + 1))"
      exit 0
    fi
  fi

done

print_footer "Completed CSV batch flash run. Audit log: $AUDIT_LOG"
