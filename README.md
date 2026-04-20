# rc433_brennenstuhl_comfort_mini

Record and playback 433 MHz signals for Brennenstuhl Comfort-Line Mini 1507070 remote sockets on a Raspberry Pi.

These sockets use a non-standard 433 MHz protocol that isn't compatible with rc-switch or similar libraries. This project takes the raw record/playback approach: capture the exact signal from the original remote, then replay it to control the sockets.

## Hardware

- Raspberry Pi (any model with GPIO)
- 433 MHz receiver module — data pin connected to **BCM 27** (WiringPi pin 2)
- 433 MHz transmitter module — data pin connected to **BCM 17** (WiringPi pin 0)

## Dependencies

- [WiringPi](https://github.com/WiringPi/WiringPi)

Install on Raspberry Pi OS:

```bash
sudo apt install wiringpi
```

On newer Raspberry Pi OS versions where the package is unavailable, build from source:

```bash
git clone https://github.com/WiringPi/WiringPi.git
cd WiringPi
./build
```

## Usage

build the binaries using

```bash
make
```

### 1. Record signals from the remote

```bash
sudo ./record socket1_on
```

This records raw 433 MHz pulses to `socket1_on.rf`. Press the desired button on the remote and hold it for a second. Recording stops on Enter, Ctrl+C, or after the timeout (default 5 seconds).

To set a custom timeout:

```bash
sudo ./record socket1_on 10   # 10 second timeout
```

Record each button you want to control:

```bash
sudo ./record socket1_on
# press button for ~1 second and then enter to stop recording
sudo ./record socket1_off
# press button for ~1 second and then enter to stop recording
sudo ./record socket2_on
# press button for ~1 second and then enter to stop recording
sudo ./record socket2_off
# press button for ~1 second and then enter to stop recording
# etc.
```

### 2. Play back signals to control sockets

```bash
sudo ./playback socket1_on
```

This replays the recorded signal from `socket1_on.rf`, triggering the socket.

To replay multiple times (for reliability):

```bash
sudo ./playback socket1_on 3   # replay 3 times
```

### File format

The `.rf` files are plain text, one pulse per line:

```
# 433 MHz raw capture, 7843 pulses (trimmed)
# Format: level(1=HIGH,0=LOW) duration_us
1 471
0 7035
1 1462
0 535
...
```

## Notes

- Both tools require `sudo` for GPIO access and real-time scheduling priority.
- If interrupted with Ctrl+C during playback, the transmitter GPIO is forced LOW to avoid jamming the 433 MHz band.
- The recorder auto-trims leading/trailing noise, keeping only the signal between the first and last sync gaps.
- Recording quality depends on proximity to the remote. Hold the remote close to the receiver antenna for best results.

## License

MIT
