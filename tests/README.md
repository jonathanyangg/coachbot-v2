# Tests

```bash
cmake -S os -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
```

`cocos2d_logic_test` checks v1 motor quadrants, standby, PWM duty, LED percent, and the message cap. `tests/test_api.py` checks `migrate_v1.py` and a live ZMQ round trip. The round trip needs `python3-zmq` and `python3-msgpack`.
