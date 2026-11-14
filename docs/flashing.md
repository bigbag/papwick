# Flashing

The TriviaPOD has a native USB Serial/JTAG port (VID 303a, PID 1001). esptool and PlatformIO reset the chip into download mode through this port. Buttons are not necessary.

## Security check

Read the eFuses before the first write:

```bash
espefuse --chip esp32s3 -p /dev/ttyACM0 summary
```

On the test unit, `SECURE_BOOT_EN` is False, `SPI_BOOT_CRYPT_CNT` is 0 and `DIS_DOWNLOAD_MODE` is False. Thus the chip boots unsigned firmware, and the flash contents are not encrypted.

## Back up the stock firmware

The stock Etermax firmware has no public download. Make a backup before the first write.

```bash
make backup
```

`scripts/backup_flash.sh` reads the 32 MB flash in 1 MB chunks into `backup/parts/`. Then it joins them into `backup/triviapod_original_32MB.bin` and runs `esptool verify-flash`. When the check passes, it writes `backup/SHA256SUMS`. The script stops at the start when `backup/SHA256SUMS` exists. Thus it cannot overwrite the stock backup with custom firmware.

Known problems:

- The esptool stub stops with "Packet content transfer stopped" on some regions (0x100000, 0x700000, 0xB00000 and 0xD00000 on the test unit). The script reads these regions again in 64 KB pieces. If the stub still fails, it uses the ROM loader (`--no-stub`).
- The script uses `--after no-reset`, so the stock app does not boot between sessions. A boot writes NVS (0x9000) and SPIFFS (0x1290000). If the verify step shows a mismatch for chunks 0 and 18, delete those chunks from `backup/parts/` and run the script again.

Keep `backup/` out of git. It holds the device firmware and the device NVS data.

## Restore the stock firmware

```bash
make restore
```

This command checks `backup/SHA256SUMS` and writes the full image at 0x0.

## Flash the firmware

```bash
make upload ENV=probe
make monitor
```

Open the serial port with DTR and RTS low. `make monitor` does this. The USB Serial/JTAG port uses DTR and RTS as the boot and reset lines. A program that raises them can reset the chip into download mode. Then the screen stays black, and the log shows `boot:0x23 (DOWNLOAD(USB/UART0))` and `waiting for download`. To start the app again, run `esptool --chip esp32s3 -p /dev/ttyACM0 --before no-reset --after no-reset run`. Then open the port with DTR and RTS low.

The upload writes the bootloader (0x0), the partition table (0x8000), `boot_app0` (0xE000) and the app (0x10000). The partition table is `default_16MB.csv`, so the upper 16 MB of the flash keep the old stock data.

## PlatformIO and Python

The pioarduino platform 55.03.312-1 refuses Python 3.14. The project uses PlatformIO 6.2.0 in a local `.venv` with Python 3.13:

```bash
uv sync
```

The `Makefile` puts `.venv/bin` first in `PATH`. Do not use PlatformIO Core 6.1.x with this platform: it removes the installed platform package and stops with `IncompatiblePlatform`.
