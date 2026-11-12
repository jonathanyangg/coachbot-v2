# IPC ops (ZMQ REQ/REP, ipc:///tmp/cocos-api)
Request: msgpack map `{"op": str, "args": map}`. Reply: `{"ok": true, "val": ..., "t": float}` or `{"ok": false, "err": str, "t": float}`.
Unknown ops (incl. v1 snake_case names) → `err: "unknown_op"`.

| op | args | val |
|---|---|---|
| setVel | {left, right} | null |
| setLed | {r, g, b} | null |
| getPose | {} | {x, y, theta} (fused) or null |
| sendMsg | {msg: bytes} | null |
| recvMsg | {} | list[bytes] |
| getClock | {} | float s |
| delay | {ms} | null |
| getId | {} | int |
| getVirtualId | {} | int |
| sendStartTime | {t} | null |
| isButtonPressed | {} | bool |
| getPoseCamera | {} | {x, y, theta, t, conf} |
| isCharging | {} | bool (debounced) |
| getDistanceTOF | {} | mm float, or list[float] |
| getBatteryVoltage | {} | V float |

`integrate` is a client-side Euler helper. It is not an op.

## Semantics this tree implements

- `setVel` `left` and `right`: truncate toward zero, then require [-100, 100]. Duty is `abs/100`. Held until the next call. See `os/services/motion.cpp`.
- `setLed` `r`,`g`,`b`: integers in [0, 100]. Duty is `value/100` (percent). v1 Rust divided by 255; this OS does not.
- `getPose`: `null` when no tracker sample exists. Shape when one exists is `{x, y, theta}`.
- `getPoseCamera`: `{x, y, theta, t, conf}` when a camera exists. Until then, `err` begins with `unavailable`.
- `sendMsg`: bytes, truncated to `msg_max_bytes` (default 64). `recvMsg` drains the local inbox as a list. No radio.
- `getClock`: float seconds. `delay`: `ms` >= 0, and the handler does not sleep.
- `getId` / `getVirtualId`: ints from config (default -1).
- `sendStartTime`: stores `t`.
- Unknown `op`, including v1 snake_case, is exactly `err: "unknown_op"`.
- Bad envelopes are `bad_request`. Bad numbers are `invalid_args: ...`.

Op name strings are duplicated in `os/api/dispatcher.cpp` and `python/coachbot_api/coachbot.py`. There is no generator.
