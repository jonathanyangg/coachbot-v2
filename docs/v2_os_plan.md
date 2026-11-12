# Coachbot v2 OS Plan

## BLUF
- v2 OS is a **C++ process** (`cocos2d`) that owns all hardware. User code is **Python** and calls it through a `usr(robot)` API.
- The two sides talk over **ZMQ REQ/REP** at `ipc:///tmp/cocos-api`, the same transport as v1. Only the message set grows.
- Most v1 cocos logic is ported as-is. The new work is the sensor drivers: UI button, Feather ESP32 co-processor, camera, two Vive trackers, charger strips, ToF.
- **Every Python method name is camelCase only, with no snake_case aliases.** This is a deliberate break from v1, and v1 user code must be renamed (table below).

## Architecture
```
 ┌──────────── Python user process ────────────┐
 │ usr(robot):  setVel getPose sendMsg ...      │
 │ coachbot_api (pyzmq REQ client, msgpack)     │
 └───────────────┬──────────────────────────────┘
                 │ ZMQ REQ/REP  ipc:///tmp/cocos-api
 ┌───────────────▼──────────── C++ cocos2d ─────────────────────┐
 │ ApiServer (REP) → Dispatcher → Services                        │
 │  Motion  Pose(fusion)  Comms(radio/UDP)  Clock  LED  Power     │
 │  Drivers: Feather ESP32 (UART: motors/LED/button/ToF/battery)  │
 │           Vive×2 (pose)  Camera (pose/frames)  Charger strips  │
 │ Supervisor: start/stop user code, watchdog, logging           │
 └────────────────────────────────────────────────────────────────┘
```

## Port from v1 cocos
- User-code lifecycle: start/stop/reload, plus the sandbox and watchdog.
- Velocity control and wheel safety limits.
- LED control.
- Pose from Vive, now extended to fuse the two trackers.
- Inter-robot messaging (`send_msg`/`recv_msg` semantics, buffers and range limits).
- Global clock sync, `delay`, and IDs (`id`, `virtual_id`).
- Experiment start sync (`send_start_time`).
- The `integrate` helper.
- Logging and the CLI hooks.

## IPC messages (msgpack `{op, args}` → `{ok, val|err}`)
**Existing ops, renamed:** `setVel`, `setLed`, `getPose`, `sendMsg`, `recvMsg`, `getClock`, `delay`, `getId`, `getVirtualId`, `sendStartTime`

**New ops:**

| op | reply |
|---|---|
| `isButtonPressed` | bool |
| `getPoseCamera` | `{x,y,theta,t,conf}` |
| `isCharging` | bool (from the charger strip contact or the ESP32 sense line) |
| `getDistanceTOF` | mm (float) or a list of floats if there are several sensors |
| `getBatteryVoltage` | V (float) |

Every reply carries a `t` timestamp. The OS returns `err:"unknown_op"` for any unknown op, including old snake_case names.

## Repo layout
```
coachbot-v2/
  os/            C++ (CMake): api/, services/, drivers/{esp32,vive,camera,charger}/, main.cpp
  firmware/esp32 Feather ESP32 (PlatformIO): motors, LED, button, ToF, ADC
  python/coachbot_api/   usr API client + stubs/sim
  proto/         op schema (single source → C++ + Python)
  tools/ cli/ docs/ tests/
```

## v1 → v2 rename table
| v1 | v2 |
|---|---|
| set_vel | setVel |
| set_led | setLed |
| get_pose | getPose |
| send_msg | sendMsg |
| recv_msg | recvMsg |
| get_clock | getClock |
| delay | delay |
| id | getId |
| virtual_id | getVirtualId |
| send_start_time | sendStartTime |
| integrate | integrate |
| *(new)* | isButtonPressed, getPoseCamera, isCharging, getDistanceTOF, getBatteryVoltage |

## Risks
- **The break with v1:** existing student and lab code fails until renamed. Mitigation: ship a one-shot `tools/migrate_v1.py` rewriter and give a clear error that names the camelCase replacement. No runtime aliases.
- **ESP32 UART link:** latency or dropped frames could stall motion. Mitigation: CRC on each frame, a heartbeat, and stopping the motors on timeout.
- **Pose fusion:** Vive×2 plus camera can drift or disagree. Expose raw `getPoseCamera` separately and keep `getPose` as the fused value.
- **Blocking ops:** REQ/REP is lockstep, so a slow op such as camera blocks the caller. Keep every handler non-blocking and serve cached values.
- **Power:** charger contact bounce. Debounce `isCharging`.

## Build / run
```
# OS
cmake -S os -B build && cmake --build build -j && sudo ./build/cocos2d --config /etc/coachbot.yaml
# firmware
cd firmware/esp32 && pio run -t upload
# Python API
pip install -e python/ && python -m coachbot_api.run usr_code.py   # def usr(robot): robot.setVel(50,50)
```
