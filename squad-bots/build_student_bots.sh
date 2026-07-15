#!/bin/bash
# build_student_bots.sh
# 
# Usage:
#   ./build_student_bots.sh CRUST-P0
#   ./build_student_bots.sh B00-B0 /dev/cu.usbserial-0002
#
# This script:
# 1. Builds firmware with student-specific values passed as compile-time flags
# 2. Flashes to the specified USB port
# 3. Leaves the repo baseline unchanged

set -e

BOT_NAME="${1:-}"
UPLOAD_PORT="${2:-/dev/cu.usbserial-0001}"
BOARD_ENV="${3:-l0-0n-devkit}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="$REPO_ROOT/src/config.h"
MAIN_FILE="$REPO_ROOT/src/main.cpp"
BACKUP_CONFIG=""
BACKUP_MAIN=""

# Accept either /dev/cu.usbserial-XXXX or cu.usbserial-XXXX for convenience.
if [ -n "$UPLOAD_PORT" ] && [ "${UPLOAD_PORT#/dev/}" = "$UPLOAD_PORT" ]; then
    UPLOAD_PORT="/dev/$UPLOAD_PORT"
fi

sanitize_name() {
    echo "$1" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9]+//g' | cut -c1-31
}

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Functions
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

cleanup_and_exit() {
    restore_backups
    cd "$REPO_ROOT"
    exit "$1"
}

create_backups() {
    BACKUP_CONFIG="$(mktemp)"
    BACKUP_MAIN="$(mktemp)"
    cp "$CONFIG_FILE" "$BACKUP_CONFIG"
    cp "$MAIN_FILE" "$BACKUP_MAIN"
}

restore_backups() {
    if [ -n "$BACKUP_CONFIG" ] && [ -f "$BACKUP_CONFIG" ]; then
        cp "$BACKUP_CONFIG" "$CONFIG_FILE"
        rm -f "$BACKUP_CONFIG"
        BACKUP_CONFIG=""
    fi

    if [ -n "$BACKUP_MAIN" ] && [ -f "$BACKUP_MAIN" ]; then
        cp "$BACKUP_MAIN" "$MAIN_FILE"
        rm -f "$BACKUP_MAIN"
        BACKUP_MAIN=""
    fi
}

patch_runtime_values() {
    # Escape any double quotes in the student/bot name to prevent breaking the C++ header
    local SAFE_BOT_NAME="${BOT_NAME//\"/\\\"}"

    sed -i '' "s|^#define BOT_NAME \".*\"$|#define BOT_NAME \"$SAFE_BOT_NAME\"|" "$CONFIG_FILE"
    sed -i '' "s|^#define WIFI_SSID_NAME \".*\"$|#define WIFI_SSID_NAME \"$WIFI_SSID\"|" "$CONFIG_FILE"
    sed -i '' "s|^#define WIFI_SSID \".*\"$|#define WIFI_SSID \"$WIFI_SSID\"|" "$CONFIG_FILE"
    sed -i '' "s|^#define WIFI_PASS \".*\"$|#define WIFI_PASS \"$WIFI_PASS\"|" "$CONFIG_FILE"
    sed -i '' "s|^static const char \*MDNS_HOST = \".*\";|static const char *MDNS_HOST = \"$MDNS_HOST\";|" "$MAIN_FILE"
}

free_upload_port() {
    if ! command -v lsof >/dev/null 2>&1; then
        return 0
    fi

    local pids
    pids=$(lsof -t "$UPLOAD_PORT" 2>/dev/null || true)
    if [ -n "$pids" ]; then
        print_warning "Port $UPLOAD_PORT is busy. Killing stale processes: $pids"
        kill $pids 2>/dev/null || true
        sleep 0.5
    fi
}

# Ensure cleanup on any failure
trap 'cleanup_and_exit 1' ERR

# Validation
if [ -z "$BOT_NAME" ]; then
    print_header "L0-0N Student Bot Build Helper"
    echo ""
    echo "Usage: ./build_student_bots.sh <STUDENT_NAME> [UPLOAD_PORT] [BOARD_ENV]"
    echo ""
    echo "Examples:"
    echo "  ./build_student_bots.sh LucasJ"
    echo "  ./build_student_bots.sh LucasD /dev/cu.usbserial-0002"
    echo "  ./build_student_bots.sh Mia /dev/cu.usbserial-0003 l0-0n-devkit"
    echo ""
    echo "Naming scheme:"
    echo "  - WiFi SSID: <studentname>"
    echo "  - mDNS host: <studentname>.local"
    echo "  - Password: <studentname>123"
    echo ""
    echo "Default build env: l0-0n-devkit"
    echo ""
    exit 1
fi

# Verify USB port exists
if [ ! -e "$UPLOAD_PORT" ]; then
    print_error "USB port $UPLOAD_PORT not found!"
    echo "Available ports:"
    ls -la /dev/cu.usb* 2>/dev/null || echo "  (no USB devices detected)"
    exit 1
fi

# Derived values
SANITIZED_NAME=$(sanitize_name "$BOT_NAME")
WIFI_SSID="$SANITIZED_NAME"
WIFI_PASS="${SANITIZED_NAME}123"
if [ ${#WIFI_PASS} -lt 8 ]; then
    WIFI_PASS="${SANITIZED_NAME}1234"
fi
MDNS_HOST="$SANITIZED_NAME"

# Header
print_header "L0-0N Student Bot: $BOT_NAME"
echo "WiFi SSID:     $WIFI_SSID"
echo "Password:      $WIFI_PASS"
echo "mDNS Hostname: $MDNS_HOST.local"
echo "Upload Port:   $UPLOAD_PORT"
echo "Build Env:    $BOARD_ENV"
echo ""

# Navigate to repo
cd "$REPO_ROOT"

# Step 1: Patch source with student identity (restored automatically after flash)
print_header "Step 1/3: Preparing Student Identity"
print_warning "Patching source identity values for this board..."
create_backups
patch_runtime_values
print_success "Identity patch applied"
echo "Verification:"
grep -E "^#define BOT_NAME|^#define WIFI_SSID_NAME|^#define WIFI_SSID|^#define WIFI_PASS" "$CONFIG_FILE"
grep -E "^static const char \*MDNS_HOST" "$MAIN_FILE"

# Step 2: Build
print_header "Step 2/3: Building Firmware"
PIO_CMD="~/.platformio/penv/bin/pio"

# Expand ~ in PIO_CMD
PIO_CMD=$(eval echo "$PIO_CMD")

if [ ! -f "$PIO_CMD" ]; then
    print_error "PlatformIO not found at $PIO_CMD"
    cleanup_and_exit 1
fi

echo "Running: $PIO_CMD run -e $BOARD_ENV"
"$PIO_CMD" run -e "$BOARD_ENV" -t clean
if "$PIO_CMD" run -e "$BOARD_ENV"; then
    print_success "Build succeeded"
else
    print_error "Build failed! Check output above."
    cleanup_and_exit 1
fi

FIRMWARE_ELF="$REPO_ROOT/.pio/build/$BOARD_ENV/firmware.elf"
if [ ! -f "$FIRMWARE_ELF" ]; then
    print_error "Expected firmware image not found: $FIRMWARE_ELF"
    cleanup_and_exit 1
fi

if ! strings "$FIRMWARE_ELF" | grep -q "$WIFI_SSID"; then
    print_error "Identity check failed: firmware does not contain expected SSID '$WIFI_SSID'."
    cleanup_and_exit 1
fi

print_success "Identity check passed: firmware contains SSID '$WIFI_SSID'."

upload_firmware() {
    local attempt=1
    local max_attempts=3

    print_warning "Erasing flash before upload..."
    if ! "$PIO_CMD" run -e "$BOARD_ENV" -t erase --upload-port "$UPLOAD_PORT"; then
        print_error "Flash erase failed."
        return 1
    fi

    while [ $attempt -le $max_attempts ]; do
        echo "Running: $PIO_CMD run -e $BOARD_ENV -t upload --upload-port $UPLOAD_PORT (attempt $attempt)"
        if "$PIO_CMD" run -e "$BOARD_ENV" -t upload --upload-port "$UPLOAD_PORT"; then
            print_success "Flash completed!"
            return 0
        fi

        print_warning "Upload attempt $attempt failed. Retrying..."
        free_upload_port
        sleep 1
        attempt=$((attempt + 1))
    done

    return 1
}

# Step 3: Flash
print_header "Step 3/3: Flashing to Device"
free_upload_port

echo ""
if upload_firmware; then
    :
else
    print_error "Flash failed after multiple attempts. Check USB connection and output above."
    cleanup_and_exit 1
fi

# Cleanup
print_header "Cleanup"
restore_backups
print_success "Original source files restored; repo baseline intact."

# Summary
print_header "✅ Build & Flash Complete!"
echo ""
echo "Next steps for testing:"
echo "1. Unplug USB cable"
echo "2. Power cycle the board"
echo "3. Connect to WiFi SSID: $WIFI_SSID"
echo "4. Use password: $WIFI_PASS"
echo "5. Open browser: http://192.168.4.1 or http://$MDNS_HOST.local"
echo ""
echo "For serial debug output, run:"
echo "  ~/.platformio/penv/bin/pio device monitor -p $UPLOAD_PORT -b 115200"
echo ""
