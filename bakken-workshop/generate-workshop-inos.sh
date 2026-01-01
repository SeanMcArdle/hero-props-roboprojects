#!/bin/bash
# ============================================================================
# BB-R2 Workshop INO Generator
# ============================================================================
#
# WHAT THIS DOES:
#   Generates Arduino sketch files for a fleet of BB-R2 droids, each with
#   unique WiFi credentials based on your chosen prefix.
#
# USAGE:
#   ./generate-workshop-inos.sh PREFIX COUNT
#
# EXAMPLES:
#   ./generate-workshop-inos.sh BK 16     # Bakken Museum: BK01-BK16
#   ./generate-workshop-inos.sh SC 12     # Science Center: SC01-SC12
#   ./generate-workshop-inos.sh LIB 8     # Library: LIB01-LIB08
#   ./generate-workshop-inos.sh Q7 4      # Custom: Q701-Q704
#
# OUTPUT:
#   Creates a folder called "Workshop-[PREFIX]" containing:
#     BB-R2-[PREFIX]01/BB-R2-[PREFIX]01.ino
#     BB-R2-[PREFIX]02/BB-R2-[PREFIX]02.ino
#     ...etc
#
#   Each droid gets:
#     - DROID_ID: [PREFIX][NUM]        (e.g., "BK05")
#     - WiFi SSID: BB-[PREFIX][NUM]    (e.g., "BB-BK05")
#     - WiFi Password: [PREFIX][NUM]droid  (e.g., "BK05droid")
#     - mDNS name: [prefix][num]       (e.g., "bk05")
#
# REQUIREMENTS:
#   - BB-R2-Workshop.ino template in same directory (or BB-R2-Workshop/ subfolder)
#   - Template must use XXXX and xxxx as placeholders
#
# AFTER GENERATING:
#   Kids can further customize their droid by editing the .ino file
#   and changing "Your Name Here" to their name.
#
# ============================================================================

# Check arguments
if [ $# -ne 2 ]; then
    echo "Usage: $0 PREFIX COUNT"
    echo "Example: $0 BK 16"
    exit 1
fi

PREFIX="$1"
COUNT="$2"

# Validate count is a number
if ! [[ "$COUNT" =~ ^[0-9]+$ ]]; then
    echo "Error: COUNT must be a number"
    exit 1
fi

# Find template
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEMPLATE="$SCRIPT_DIR/BB-R2-Workshop/BB-R2-Workshop.ino"

if [ ! -f "$TEMPLATE" ]; then
    TEMPLATE="$SCRIPT_DIR/BB-R2-Workshop.ino"
fi

if [ ! -f "$TEMPLATE" ]; then
    echo "Error: Cannot find BB-R2-Workshop.ino template"
    echo "Expected at: $SCRIPT_DIR/BB-R2-Workshop/BB-R2-Workshop.ino"
    exit 1
fi

# Create output directory
OUTPUT_DIR="$SCRIPT_DIR/Workshop-${PREFIX}"
mkdir -p "$OUTPUT_DIR"

echo "Generating $COUNT workshop .ino files with prefix '$PREFIX'..."
echo ""

# Generate files
for i in $(seq 1 $COUNT); do
    NUM=$(printf "%02d" $i)
    DESIGNATION="${PREFIX}${NUM}"
    DESIGNATION_LOWER=$(echo "$DESIGNATION" | tr '[:upper:]' '[:lower:]')
    
    FOLDER="$OUTPUT_DIR/BB-R2-${DESIGNATION}"
    mkdir -p "$FOLDER"
    OUTFILE="$FOLDER/BB-R2-${DESIGNATION}.ino"
    
    sed -e "s/XXXX/${DESIGNATION}/g" \
        -e "s/xxxx/${DESIGNATION_LOWER}/g" \
        "$TEMPLATE" > "$OUTFILE"
    
    echo "✓ ${DESIGNATION}"
done

echo ""
echo "Done! Files in $OUTPUT_DIR"
echo ""
echo "Each droid's WiFi:"
echo "  SSID: BB-[PREFIX][NUM]  (e.g., BB-${PREFIX}01)"
echo "  Password: [PREFIX][NUM]droid  (e.g., ${PREFIX}01droid)"
