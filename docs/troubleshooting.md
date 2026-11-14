# Troubleshooting

## Save the serial log

Run `make log` to append the serial log to a file.

- Set the file with `LOG=<file>`. The default is `device.log`.
- Set the port with `PORT=<port>`. The default is `/dev/ttyACM0`.
- The script opens the port with DTR and RTS low.
- The script reconnects after a reset or a USB disconnect.

```bash
make log LOG=boot.log
```

## The port is busy

Close `make monitor`, `make log` and other serial programs before you run `make upload`.

Check which process uses the port:

```bash
fuser /dev/ttyACM0
```

## The device stays in download mode

The screen is black. The log shows `boot:0x23 (DOWNLOAD(USB/UART0))` and `waiting for download`.

Run this command to start the app:

```bash
esptool --chip esp32s3 -p /dev/ttyACM0 --before no-reset --after no-reset run
```

## No serial port

- Use a USB cable that carries data.
- List the serial ports:

```bash
ls /dev/ttyACM*
```

- On Linux, add your user to the `uucp` or `dialout` group. Then log in again.

## Black screen after a cold power-on

The firmware restarts once after a power-on reset. This restart starts the display.

If the screen stays black, connect USB and reset the chip:

```bash
esptool --chip esp32s3 -p /dev/ttyACM0 run
```
