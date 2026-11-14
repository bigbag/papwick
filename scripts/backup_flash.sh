#!/bin/bash
# Read the full 32 MB TriviaPOD flash into backup/ and verify it on the device.
# The esptool stub fails on some 1 MB regions over USB Serial/JTAG ("Packet content transfer stopped").
# Those regions are read again in 64 KB pieces, with the ROM loader as the fallback.

set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
MB=1048576
PIECE=65536
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/backup"
PARTS="$OUT_DIR/parts"
IMAGE="$OUT_DIR/triviapod_original_32MB.bin"
# --after no-reset keeps the chip in the loader, so the stock app does not boot and write NVS between sessions.
ESPTOOL=(esptool --chip esp32s3 -p "$PORT" --after no-reset)

if [ -f "$OUT_DIR/SHA256SUMS" ]; then
  echo "backup/ holds a verified backup. The flash can hold custom firmware now. Move backup/ away first."
  exit 1
fi

mkdir -p "$PARTS"

read_region() {
  local offset=$1 size=$2 file=$3
  for _ in 1 2 3; do
    "${ESPTOOL[@]}" read-flash "$offset" "$size" "$file" >/dev/null 2>&1 && return 0
    sleep 2
  done
  "${ESPTOOL[@]}" --no-stub read-flash "$offset" "$size" "$file" >/dev/null 2>&1
}

for i in $(seq 0 31); do
  chunk="$PARTS/$(printf %02d "$i").bin"
  [ -f "$chunk" ] && [ "$(stat -c%s "$chunk")" -eq $MB ] && continue
  if read_region $((i * MB)) $MB "$chunk"; then
    echo "chunk $i ok"
    continue
  fi
  for j in $(seq 0 15); do
    read_region $((i * MB + j * PIECE)) $PIECE "$PARTS/$(printf %02d_%02d "$i" "$j").bin" ||
      { echo "chunk $i piece $j FAILED"; exit 1; }
  done
  cat "$PARTS/$(printf %02d "$i")"_*.bin > "$chunk"
  rm "$PARTS/$(printf %02d "$i")"_*.bin
  echo "chunk $i ok in pieces"
done

cat "$PARTS"/{00..31}.bin > "$IMAGE"

# The stock app writes NVS (0x9000) and SPIFFS (0x1290000) at each boot. If a boot occurs between sessions, the
# check shows a mismatch there. Delete those chunks from backup/parts and run this script again until it passes.
"${ESPTOOL[@]}" verify-flash 0 "$IMAGE"
# SHA256SUMS marks a complete backup. make restore needs it, and this script stops when it exists.
(cd "$OUT_DIR" && sha256sum "$(basename "$IMAGE")" > SHA256SUMS.tmp && mv SHA256SUMS.tmp SHA256SUMS)
