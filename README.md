# Coachbot v2 OS

C++ process `cocos2d` owns the robot API. Student code is Python `usr(robot)` and talks to it over **ZMQ REQ/REP** at `ipc:///tmp/cocos-api`, msgpack `{op, args}` → `{ok, val|err, t}`. Names are **camelCase only**. Snake_case ops return `unknown_op`.

The locked plan is [docs/v2_os_plan.md](docs/v2_os_plan.md). Where this tree disagrees with that plan, this README is the status of the code: the Feather ESP32 is **not** the motor path.

## What actually runs

| Piece | Status |
|---|---|
| ApiServer ZMQ REP loop, all ops in [proto/ops.md](proto/ops.md) | Implemented |
| `setVel` −100..100, truncate toward zero like v1 `int()` | Implemented. Duty is `abs(percent)/100`. Quadrants and STBY/IN pins follow v1 `motor.rs` / `motor_driver.rs`. The v1 bug that wrote both speeds to the left driver is fixed. |
| `setLed` integers 0..100 | Implemented as percent duty (`value/100`) at 120 Hz. v1 Rust divided those integers by 255; this build does not. |
| GPIO | **libgpiod** when `gpio=pi`, or when `gpio=auto` and the gpiochip device exists. Otherwise the in-memory backend (what tests use). Software PWM, about a 200 µs tick. |
| `getPose` | `null` / Python `None` until a tracker exists. No Vive or nucifera bytes are invented. v1 `nucifera_driver.rs` never defined a frame. |
| `sendMsg` / `recvMsg` | Loopback inbox on this process. Cap 64 bytes (published user guide), then drop the oldest. **No radio, no UDP, no range filter.** v1 `MSG_LEN` is not in the public cocos tree. |
| `getClock` | Monotonic seconds since `cocos2d` started. Not synced to a swarm master. |
| `delay` | OS returns immediately. Python sleeps locally (v1 did this in `cocos_py2`). Default **200 ms** from cocos, not the user-guide 20 ms. |
| `getId` / `getVirtualId` | Configured integers. Default **−1**, matching the unset id in `cocos_py2`. |
| `sendStartTime` | Stored and logged. Not broadcast. |
| `integrate` | Client-side Euler step. Not an op. Public cocos has no `integrate`. |
| `isButtonPressed`, `getPoseCamera`, `isCharging`, `getDistanceTOF`, `getBatteryVoltage` | `err` starting with `unavailable`. No fake samples. Charging is not debounced because there is no contact signal. |
| User-code sandbox, watchdog, spawning `usr()` | **Not ported.** v1 spawned `python2 -m cocos_py2` from `ApiController`. Start the client yourself. |
| ESP32 UART motors | **Not implemented.** Default build uses the Pi GPIO logic above. `-DCOACHBOT_MOTORS_ON_ESP32=ON` makes `setVel` return an error. No frame format. |

## Bottom-board BCM pins

Confirmed on `bottom_board_v2.sch` nets into TB6612FNG `TB1`. These are the defaults in `Config` and in `os/coachbot.example.conf`. They match `coachbots/cocos` `src/config.rs`.

| | in1 | in2 | pwm | stdby |
|---|---|---|---|---|
| left (A) | 19 | 16 | 26 | 20 |
| right (B) | 5 | 6 | 12 | 20 |

LED API, same order as v1: red 22, green 23, blue 24. On the schematic those nets land on connector SV1 pins 6, 5, and 4. The sheet does not label colors. GPIO22 is also switch S2. Motor PWM is 600 Hz. Both motors share STBY on BCM 20.

`cocos2d` comes up with the drivers blocked (STBY low) until the first `setVel`. `setVel(0, 0)` coasts (STBY high, duty 0). It does not brake.

UART settings in cocos `config.rs` (19200 8E1) are still the unimplemented nucifera port. There is no packet layout.

## Smoke test on a Pi

Prop the robot up so the wheels are off the ground. Install `libgpiod-dev` (1.x or 2.x; Raspbian 13 ships 2.2), build, and confirm the 40-pin header's chip with `gpiodetect`. On some Pi 5 images that chip is `gpiochip4` rather than `gpiochip0`; set `gpiochip=` in the config if so. The process needs access to that device (gpio group or root).

```bash
sudo apt-get install cmake g++ pkg-config libzmq3-dev libmsgpack-cxx-dev libgpiod-dev python3-zmq python3-msgpack
cmake -S os -B build && cmake --build build -j
gpiodetect
./build/cocos2d --config os/coachbot.example.conf
```

In another shell, after `pip install -e python/`:

```bash
python3 - << 'PY'
from coachbot_api import Coachbot
robot = Coachbot()
robot.setLed(0, 80, 0)
robot.setVel(30, 30)
robot.delay(500)
robot.setVel(0, 0)
robot.setLed(0, 0, 0)
PY
```

Startup should log `gpio: libgpiod /dev/gpiochip0`. `gpio=memory` records the same pin writes and does not touch the header. `gpio=pi` exits if the chip cannot be opened.

## Build

Dependencies: CMake, a C++17 compiler, **libzmq** (pkg-config `libzmq`), **msgpack-cxx** headers (`msgpack.hpp`, Ubuntu package `libmsgpack-cxx-dev`). Python: `pyzmq`, `msgpack`.

```bash
sudo apt-get install cmake g++ pkg-config libzmq3-dev libmsgpack-cxx-dev libgpiod-dev python3-zmq python3-msgpack
# If /usr/bin/c++ is clang and it selects GCC 14, also install libstdc++-14-dev.
cmake -S os -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
# Reconfigure after a libgpiod upgrade. CMake prints "libgpiod 2.x: using API v2" on Raspbian 13.
./build/cocos2d --config os/coachbot.example.conf
```

`ctest` runs the motor/LED pin test and a live Python round trip.

ESP32 flag (still does not drive motors):

```bash
cmake -S os -B build -DCOACHBOT_MOTORS_ON_ESP32=ON && cmake --build build -j
```

Firmware under `firmware/esp32` is a PlatformIO stub. It does not speak a protocol. `pio run` is not part of the default build.

## Python

```bash
pip install -e python/
python -m coachbot_api.run usr_code.py
python tools/migrate_v1.py path/to/code.py          # dry run
python tools/migrate_v1.py path/to/code.py --write
```

`usr_code.py` defines `usr(robot)`. The OS process must already be listening. `robot.set_vel` raises `AttributeError` and names `setVel`.

## Sources checked

- https://github.com/coachbots/cocos — `src/config.rs`, `src/controllers/motor.rs`, `src/drivers/motor_driver.rs`, `src/drivers/led_driver.rs`, `src/controllers/api/messager.rs`, `src/controllers/api/ipc_requests.rs`, `cocos_py2/api/coachbot.py`, `cocos_py2/cocos/__init__.py`, `src/controllers/api/mod.rs` (how v1 launched user code), `src/drivers/nucifera_driver.rs` (no frames).
- https://github.com/coachbots/cctl — user code is started by the control daemon, not by this OS.
- https://coachbotswarm.github.io/User-Guide/ — 64-byte message cap. Its wheel range (−50..50) and 1-second stop are the deployed coach-os guide, not cocos. This port follows cocos: −100..100, held until the next `setVel`.
