#!/bin/bash
# Batch flash script for Bakken Workshop droids
# Hero Props Inc. - December 2025

REPO=~/Documents/GitHub/hero-props-roboprojects/bakken-workshop
INO="$REPO/BB-R2-Workshop/BB-R2-Workshop.ino"
PORT="/dev/cu.usbserial-0001"
FQBN="esp32:esp32:esp32"

# Droid roster: number,name
DROIDS=(
  "01,Lucien Hayes"
  "02,Harvey Hansen"
  "03,Miriam Hansen"
  "04,Eleanor Hageman"
  "05,Willa Felton"
  "06,Eva Cich"
  "07,Ryan Carlson"
  "08,Penelope Brunner"
  "09,Isabelle Beissel"
  "10,Daelyn Baker"
  "11,Margot Zimmerman"
  "12,Adeline Wald"
  "13,Sasha Thao"
  "14,Nora Swenson"
  "15,Luke Wilde"
  "16,Greta Olson"
)

echo "========================================"
echo "  BAKKEN DROID BATCH FLASH"
echo "  16 droids to program"
echo "========================================"
echo ""

for entry in "${DROIDS[@]}"; do
  NUM="${entry%%,*}"
  NAME="${entry#*,}"
  
  echo ""
  echo "----------------------------------------"
  echo "  DROID BK-$NUM: $NAME"
  echo "----------------------------------------"
  echo ""
  echo ">>> Plug in ESP32 for BK-$NUM, then press ENTER"
  echo ">>> Or type 's' to skip, 'q' to quit"
  read -r response
  
  if [[ "$response" == "q" ]]; then
    echo "Quitting."
    exit 0
  fi
  
  if [[ "$response" == "s" ]]; then
    echo "Skipping BK-$NUM"
    continue
  fi
  
  # Update the .ino file with this droid's info
  echo "Updating firmware for $NAME..."
  
  # Replace DROID_ID
  sed -i '' "s/const char\* DROID_ID = \"BK-[0-9]*\";/const char* DROID_ID = \"BK-$NUM\";/" "$INO"
  
  # Replace DROID_OWNER
  sed -i '' "s/const char\* DROID_OWNER = \"[^\"]*\";/const char* DROID_OWNER = \"$NAME\";/" "$INO"
  
  # Replace AP_SSID
  sed -i '' "s/const char\* AP_SSID = \"R2-BK[0-9]*\";/const char* AP_SSID = \"R2-BK$NUM\";/" "$INO"
  
  # Replace AP_PASS
  sed -i '' "s/const char\* AP_PASS = \"BK[0-9]*droid\";/const char* AP_PASS = \"BK${NUM}droid\";/" "$INO"
  
  # Replace MDNS_NAME
  sed -i '' "s/const char\* MDNS_NAME = \"bk[0-9]*\";/const char* MDNS_NAME = \"bk$NUM\";/" "$INO"
  
  # Replace HTML title
  sed -i '' "s/<title>R2-BK[0-9]* - [^<]*<\/title>/<title>R2-BK$NUM - $NAME<\/title>/" "$INO"
  
  # Replace H1
  sed -i '' "s/<h1>R2-BK[0-9]*<\/h1>/<h1>R2-BK$NUM<\/h1>/" "$INO"
  
  # Replace owner line in HTML
  sed -i '' "s/<div class=\"owner\">[^<]*'s Droid<\/div>/<div class=\"owner\">$NAME's Droid<\/div>/" "$INO"
  
  # Clean build directory
  rm -rf "$REPO/BB-R2-Workshop/build"
  
  # Compile
  echo "Compiling..."
  if ! arduino-cli compile --fqbn "$FQBN" "$REPO/BB-R2-Workshop"; then
    echo "!!! COMPILE FAILED for BK-$NUM !!!"
    continue
  fi
  
  # Upload
  echo "Uploading to BK-$NUM..."
  if arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$REPO/BB-R2-Workshop"; then
    echo ""
    echo "✓✓✓ BK-$NUM ($NAME) COMPLETE ✓✓✓"
  else
    echo "!!! UPLOAD FAILED for BK-$NUM !!!"
  fi
  
done

echo ""
echo "========================================"
echo "  BATCH COMPLETE"
echo "========================================"
echo ""
echo "Don't forget to restore BK-00 if needed!"
