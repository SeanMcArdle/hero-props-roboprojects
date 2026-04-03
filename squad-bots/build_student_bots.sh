#!/bin/bash
# build_student_bots.sh
# 
# Usage:
#   ./build_student_bots.sh CRUST-P0
#   ./build_student_bots.sh B00-B0 /dev/cu.usbserial-0002
#
# This script:
# 1. Patches config.h & main.cpp with custom bot names
# 2. Builds firmware
# 3. Flashes to the specified USB port
# 4. Restores files to git baseline

set -e

BOT_NAME="${1:-}"
UPLOAD_PORT="${2:-/dev/cu.usbserial-0001}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

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
    print_warning "Restoring config files to baseline..."
    cd "$REPO_ROOT"
    git checkout src/config.h src/main.cpp 2>/dev/null || true
    exit "$1"
}

# Validation
if [ -z "$BOT_NAME" ]; then
    print_header "L0-0N Student Bot Build Helper"
    echo ""
    echo "Usage: ./build_student_bots.sh <BOT_NAME> [UPLOAD_PORT]"
    echo ""
    echo "Examples:"
    echo "  ./build_student_bots.sh CRUST-P0"
    echo "  ./build_student_bots.sh B00-B0 /dev/cu.usbserial-0002"
    echo "  ./build_student_bots.sh Sp0tty-M33P /dev/cu.usbserial-0003"
    echo ""
    echo "Available student bots:"
    echo "  - CRUST-P0"
    echo "  - B00-B0"
    echo "  - Sp0tty-M33P"
    echo "  - RJ-BD3"
    echo "  - KP-71"
    echo "  - LU-CA"
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
WIFI_SSID="${BOT_NAME}-Net"
MDNS_HOST=$(echo "$BOT_NAME" | tr '[:upper:]' '[:lower:]' | sed 's/-//g')

# Header
print_header "L0-0N Student Bot: $BOT_NAME"
echo "WiFi SSID:     $WIFI_SSID"
echo "mDNS Hostname: $MDNS_HOST.local"
echo "Upload Port:   $UPLOAD_PORT"
echo ""

# Navigate to repo
cd "$REPO_ROOT"

# Step 1: Patch config files
print_header "Step 1/3: Patching Configuration Files"

print_warning "Updating src/config.h..."
# Patch BOT_NAME (inside guards)
sed -i '' "s|#define BOT_NAME \"L0-0N\"|#define BOT_NAME \"$BOT_NAME\"|g" src/config.h
# Patch WIFI_SSID_NAME (inside guards)
sed -i '' "s|#define WIFI_SSID_NAME \"L0-0N-Net\"|#define WIFI_SSID_NAME \"$WIFI_SSID\"|g" src/config.h
# Patch WIFI_SSID (at bottom of file, fallback for any code that uses it)
sed -i '' "s|#define WIFI_SSID \"L0-0N-Net\"|#define WIFI_SSID \"$WIFI_SSID\"|g" src/config.h
print_success "src/config.h updated"

print_warning "Updating src/main.cpp..."
# Handle both quoted and unquoted MDNS_HOST patterns
sed -i '' "s|static const char \\*MDNS_HOST = \"l00n\"|static const char *MDNS_HOST = \"$MDNS_HOST\"|g" src/main.cpp
print_success "src/main.cpp updated"

# Verify patches
echo ""
echo "Verification:"
grep -E "^#define BOT_NAME|^#define WIFI_SSID" src/config.h | sort
grep "MDNS_HOST" src/main.cpp | head -1

# Step 2: Build
print_header "Step 2/3: Building Firmware"
PIO_CMD="~/.platformio/penv/bin/platformio"

# Expand ~ in PIO_CMD
PIO_CMD=$(eval echo "$PIO_CMD")

if [ ! -f "$PIO_CMD" ]; then
    print_error "PlatformIO not found at $PIO_CMD"
    cleanup_and_exit 1
fi

echo "Running: $PIO_CMD run -e l0-0n"
if "$PIO_CMD" run -e l0-0n; then
    print_success "Build succeeded"
else
    print_error "Build failed! Check output above."
    cleanup_and_exit 1
fi

# Step 3: Flash
print_header "Step 3/3: Flashing to Device"
echo "Running: $PIO_CMD run -e l0-0n -t upload --upload-port $UPLOAD_PORT"
echo ""

if "$PIO_CMD" run -e l0-0n -t upload --upload-port "$UPLOAD_PORT"; then
    print_success "Flash completed!"
else
    print_error "Flash failed! Check USB connection and output above."
    cleanup_and_exit 1
fi

# Cleanup
print_header "Cleanup"
print_warning "Restoring config files to baseline..."
if git checkout src/config.h src/main.cpp; then
    print_success "Files restored to git baseline"
else
    print_error "Could not restore files—please run: git checkout src/config.h src/main.cpp"
fi

# Summary
print_header "✅ Build & Flash Complete!"
echo ""
echo "Next steps for testing:"
echo "1. Unplug USB cable"
echo "2. Power cycle the board"
echo "3. Connect to WiFi SSID: $WIFI_SSID"
echo "4. Open browser: http://192.168.4.1 or http://$MDNS_HOST.local"
echo ""
echo "For serial debug output, run:"
echo "  ~/.platformio/penv/bin/pio device monitor -p $UPLOAD_PORT -b 115200"
echo ""
