#!/bin/bash
# batch_build_student_bots.sh
#
# Interactive wrapper for one-board-at-a-time student bot flashing.
# It calls build_student_bots.sh for each entry and prompts before the next board.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_SCRIPT="$SCRIPT_DIR/build_student_bots.sh"

if [ ! -x "$BUILD_SCRIPT" ]; then
  echo "ERROR: build_student_bots.sh not found or not executable in $SCRIPT_DIR"
  exit 1
fi

prompt() {
  local prompt_text="$1"
  local default_value="$2"
  local answer

  if [ -n "$default_value" ]; then
    read -r -p "$prompt_text [$default_value]: " answer
    answer="${answer:-$default_value}"
  else
    read -r -p "$prompt_text: " answer
  fi

  echo "$answer"
}

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

print_header "Student Bot Flash Loop"

while true; do
  BOT_NAME="$(prompt 'Enter student name (or press ENTER to finish)' '')"
  if [ -z "$BOT_NAME" ]; then
    print_footer "No student name entered. Exiting."
    exit 0
  fi

  UPLOAD_PORT="$(prompt 'Enter USB upload port' '/dev/cu.usbserial-0001')"
  BOARD_ENV="$(prompt 'Enter PlatformIO build env' 'l0-0n-devkit')"

  echo ""
  echo "Ready to flash:" 
  echo "  Student name: $BOT_NAME"
  echo "  Upload port:  $UPLOAD_PORT"
  echo "  Build env:    $BOARD_ENV"
  echo ""

  read -r -p "Proceed with this board? (y/N): " confirm
  confirm="${confirm,,}"
  if [[ "$confirm" != "y" && "$confirm" != "yes" ]]; then
    echo "Skipping this entry."
    continue
  fi

  "$BUILD_SCRIPT" "$BOT_NAME" "$UPLOAD_PORT" "$BOARD_ENV"

  print_footer "Build complete for $BOT_NAME"

  read -r -p "Plug in the next board and press ENTER to continue, or type q to quit: " next
  if [[ "${next,,}" == "q" || "${next,,}" == "quit" ]]; then
    print_footer "Done. Exiting batch flash loop."
    exit 0
  fi

done
