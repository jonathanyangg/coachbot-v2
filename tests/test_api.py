#!/usr/bin/env python3
"""Client checks plus a live cocos2d round trip."""
import os
import pathlib
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
sys.path.insert(0, str(ROOT / "tools"))

import migrate_v1  # noqa: E402
from coachbot_api import Coachbot, CoachbotError  # noqa: E402


def fail(message):
    raise SystemExit("FAIL " + message)


def test_migrate():
    src = "robot.set_vel(1, 2)\nrobot.virtual_id\nrobot.id\nrobot.get_pose()\nrobot.set_led(0, 1, 2)\n"
    out = migrate_v1.fix(src)
    expect = "robot.setVel(1, 2)\nrobot.getVirtualId()\nrobot.getId()\nrobot.getPose()\nrobot.setLed(0, 1, 2)\n"
    if out != expect:
        fail("migrate_v1\n" + out)
    with tempfile.TemporaryDirectory() as tmp:
        path = pathlib.Path(tmp) / "usr.py"
        path.write_text(src)
        migrate_v1.main([str(path)])
        if path.read_text() != src:
            fail("dry run wrote the file")
        migrate_v1.main([str(path), "--write"])
        if path.read_text() != expect:
            fail("--write did not rewrite")


def test_client_local():
    bot = Coachbot("ipc:///tmp/cocos-api-unused")
    try:
        bot.set_vel(1, 1)
        fail("snake_case set_vel was accepted")
    except AttributeError as exc:
        if "setVel" not in str(exc):
            fail(str(exc))
    try:
        bot.setVel(101, 0)
        fail("setVel 101 should fail locally")
    except CoachbotError as exc:
        if "invalid_args" not in str(exc):
            fail(str(exc))
    try:
        bot.setLed(1.5, 0, 0)
        fail("fractional led should fail locally")
    except CoachbotError as exc:
        if "invalid_args" not in str(exc):
            fail(str(exc))
    if bot.integrate(lambda x: 2, 1, 0.5, steps=2) != 3:
        fail("integrate")
    try:
        bot.delay(-1)
        fail("negative delay")
    except CoachbotError:
        pass


def raw_call(endpoint, payload):
    import msgpack
    import zmq

    ctx = zmq.Context()
    sock = ctx.socket(zmq.REQ)
    sock.setsockopt(zmq.LINGER, 0)
    sock.setsockopt(zmq.RCVTIMEO, 3000)
    sock.setsockopt(zmq.SNDTIMEO, 3000)
    sock.connect(endpoint)
    sock.send(msgpack.packb(payload, use_bin_type=True))
    reply = msgpack.unpackb(sock.recv(), raw=False)
    sock.close(linger=0)
    ctx.term()
    return reply


def test_ipc(binary):
    import msgpack  # noqa: F401

    endpoint = "ipc:///tmp/cocos-api-test-%d" % os.getpid()
    conf = tempfile.NamedTemporaryFile("w", delete=False)
    conf.write("id=12\nvirtual_id=9\nendpoint=%s\ngpio=memory\n" % endpoint)
    conf.close()
    proc = subprocess.Popen([binary, "--config", conf.name, "--id", "4", "--virtual-id", "8"])
    try:
        time.sleep(0.3)
        if proc.poll() is not None:
            fail("cocos2d exited early: %s" % proc.returncode)
        bot = Coachbot(endpoint)
        if bot.getId() != 4 or bot.getVirtualId() != 8:
            fail("cli id override")
        clock = bot.getClock()
        if not isinstance(clock, float) or clock < 0:
            fail("clock %r" % (clock,))
        later = bot.getClock()
        if later < clock:
            fail("clock went backwards")
        if bot.setVel(-100, 100) is not None or bot.setVel(100.9, -0.2) is not None:
            fail("setVel return")
        try:
            bot.setLed(0, 100, 50)
        except CoachbotError as exc:
            fail("setLed " + str(exc))
        if bot.getPose() is not None:
            fail("pose should be None without a tracker")
        if bot.sendMsg(b"abc") is not None or bot.sendMsg("hi") is not None:
            fail("sendMsg return")
        got = bot.recvMsg()
        if got != [b"abc", b"hi"]:
            fail("recvMsg %r" % (got,))
        if bot.recvMsg() != []:
            fail("inbox should drain")
        bot.sendMsg(b"x" * 80)
        truncated = bot.recvMsg()
        if truncated != [b"x" * 64]:
            fail("truncation %r" % (truncated,))
        if bot.sendStartTime(1.25) is not None:
            fail("sendStartTime")
        if bot.delay(0) is not None:
            fail("delay")
        for method in ("getPoseCamera", "isButtonPressed", "isCharging", "getDistanceTOF", "getBatteryVoltage"):
            try:
                getattr(bot, method)()
                fail(method + " should be unavailable")
            except CoachbotError as exc:
                if "unavailable" not in str(exc):
                    fail(method + " " + str(exc))
        unknown = raw_call(endpoint, {"op": "set_vel", "args": {"left": 1, "right": 1}})
        if unknown.get("ok") or unknown.get("err") != "unknown_op" or not isinstance(unknown.get("t"), float):
            fail("unknown_op %r" % (unknown,))
        too_fast = raw_call(endpoint, {"op": "setVel", "args": {"left": 200, "right": 0}})
        if too_fast.get("ok") or not str(too_fast.get("err", "")).startswith("invalid_args"):
            fail("range %r" % (too_fast,))
        led = raw_call(endpoint, {"op": "setLed", "args": {"r": 100.9, "g": 0, "b": 0}})
        if led.get("ok"):
            fail("fractional led accepted %r" % (led,))
        truncated_vel = raw_call(endpoint, {"op": "setVel", "args": {"left": 100.9, "right": -100.2}})
        if not truncated_vel.get("ok"):
            fail("truncated vel %r" % (truncated_vel,))
        clock_rep = raw_call(endpoint, {"op": "getClock", "args": {}})
        if abs(clock_rep["val"] - clock_rep["t"]) > 1e-6:
            fail("clock t mismatch %r" % (clock_rep,))
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=2)
        os.unlink(conf.name)


def test_pi_without_chip(binary):
    if os.path.exists("/dev/gpiochip0"):
        return
    conf = tempfile.NamedTemporaryFile("w", delete=False)
    conf.write("gpio=pi\ngpiochip=/dev/gpiochip0\n")
    conf.close()
    try:
        proc = subprocess.run([binary, "--config", conf.name], timeout=3, capture_output=True, text=True)
    finally:
        os.unlink(conf.name)
    if proc.returncode == 0 or "gpiod_chip_open" not in (proc.stderr + proc.stdout):
        fail("gpio=pi should fail closed without a chip: %s %s" % (proc.returncode, proc.stderr))


def find_binary():
    env = os.environ.get("COCOS2D")
    if env and os.path.isfile(env) and os.access(env, os.X_OK):
        return env
    candidates = [pathlib.Path.cwd() / "cocos2d", ROOT / "build" / "cocos2d"]
    for path in candidates:
        if path.is_file() and os.access(path, os.X_OK):
            return str(path)
    fail("cocos2d binary not found; cmake --build build first")


def main():
    binary = find_binary()
    test_migrate()
    test_client_local()
    test_pi_without_chip(binary)
    test_ipc(binary)
    print("api ok")


if __name__ == "__main__":
    main()
