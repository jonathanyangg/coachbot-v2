"""camelCase-only usr(robot) client.

Wire format: proto/ops.md. msgpack over ZMQ REQ to ipc:///tmp/cocos-api.
There are no snake_case aliases. v1 names raise AttributeError and name the
replacement. integrate does not send an op.
"""
import math
import time

DEFAULT_ENDPOINT = "ipc:///tmp/cocos-api"

V1_NAMES = {
    "set_vel": "setVel",
    "set_led": "setLed",
    "get_pose": "getPose",
    "send_msg": "sendMsg",
    "recv_msg": "recvMsg",
    "get_clock": "getClock",
    "id": "getId",
    "virtual_id": "getVirtualId",
    "send_start_time": "sendStartTime",
}


class CoachbotError(RuntimeError):
    pass


def _number(value, what):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise CoachbotError("invalid_args: %s expects a number" % what)
    if isinstance(value, float) and not math.isfinite(value):
        raise CoachbotError("invalid_args: %s expects a finite number" % what)
    return value


class Coachbot:
    def __init__(self, endpoint=DEFAULT_ENDPOINT, timeout_ms=3000):
        import zmq

        self._endpoint = endpoint
        self._timeout_ms = int(timeout_ms)
        self._ctx = zmq.Context.instance()
        self._sock = None
        self._open()

    def _open(self):
        import zmq

        self._sock = self._ctx.socket(zmq.REQ)
        self._sock.setsockopt(zmq.LINGER, 0)
        self._sock.setsockopt(zmq.SNDTIMEO, self._timeout_ms)
        self._sock.setsockopt(zmq.RCVTIMEO, self._timeout_ms)
        self._sock.connect(self._endpoint)

    def _reset(self):
        self._sock.close(linger=0)
        self._open()

    def _call(self, op, **args):
        import msgpack
        import zmq

        try:
            self._sock.send(msgpack.packb({"op": op, "args": args}, use_bin_type=True))
            raw = self._sock.recv()
        except zmq.error.Again as exc:
            self._reset()
            raise CoachbotError("%s: timeout" % op) from exc
        rep = msgpack.unpackb(raw, raw=False)
        if not isinstance(rep, dict) or "ok" not in rep:
            raise CoachbotError("%s: bad_reply" % op)
        if not rep["ok"]:
            raise CoachbotError("%s: %s" % (op, rep.get("err")))
        return rep.get("val")

    def __getattr__(self, name):
        if name in V1_NAMES:
            raise AttributeError(
                "'%s' was removed in v2; use '%s' (run tools/migrate_v1.py)"
                % (name, V1_NAMES[name])
            )
        raise AttributeError(name)

    def setVel(self, left, right):
        """Wheel percents. v1 truncates with int() then requires [-100, 100]
        (cocos_py2/api/coachbot.py, ipc_requests.rs)."""
        left_i = int(math.trunc(_number(left, "setVel")))
        right_i = int(math.trunc(_number(right, "setVel")))
        if left_i < -100 or left_i > 100 or right_i < -100 or right_i > 100:
            raise CoachbotError("invalid_args: setVel left and right must be in [-100, 100]")
        return self._call("setVel", left=left_i, right=right_i)

    def setLed(self, r, g, b):
        """Integer channel percents in [0, 100] (IPCSendLedMessage)."""

        def channel(value):
            number = _number(value, "setLed")
            if isinstance(number, float) and not number.is_integer():
                raise CoachbotError("invalid_args: setLed r, g, b must be integers in [0, 100]")
            as_int = int(number)
            if as_int < 0 or as_int > 100:
                raise CoachbotError("invalid_args: setLed r, g, b must be integers in [0, 100]")
            return as_int

        return self._call("setLed", r=channel(r), g=channel(g), b=channel(b))

    def getPose(self):
        """Fused pose {x, y, theta}, or None when no tracker has a fix."""
        return self._call("getPose")

    def sendMsg(self, msg):
        """str or bytes. The OS truncates to the configured byte cap (64)."""
        if isinstance(msg, str):
            payload = msg.encode("utf-8")
        elif isinstance(msg, (bytes, bytearray)):
            payload = bytes(msg)
        else:
            raise CoachbotError("invalid_args: sendMsg expects str or bytes")
        return self._call("sendMsg", msg=payload)

    def recvMsg(self):
        """Messages queued since the last call. Empty inbox is []."""
        return self._call("recvMsg")

    def getClock(self):
        """Seconds since cocos2d started (monotonic). Not a swarm-wide clock."""
        return self._call("getClock")

    def delay(self, ms=200):
        """Wait ms milliseconds. Default 200 matches cocos_py2 coachbot.delay.

        The OS op returns immediately. The sleep is local, as in v1.
        The swarm user guide's default of 20 ms is a different codebase.
        """
        ms_i = int(math.trunc(_number(ms, "delay")))
        if ms_i < 0:
            raise CoachbotError("invalid_args: delay ms must be >= 0")
        self._call("delay", ms=ms_i)
        time.sleep(ms_i / 1000.0)
        return None

    def getId(self):
        return self._call("getId")

    def getVirtualId(self):
        return self._call("getVirtualId")

    def sendStartTime(self, t):
        """Record an experiment start time. No radio broadcast in this build."""
        return self._call("sendStartTime", t=float(_number(t, "sendStartTime")))

    def isButtonPressed(self):
        return self._call("isButtonPressed")

    def getPoseCamera(self):
        return self._call("getPoseCamera")

    def isCharging(self):
        return self._call("isCharging")

    def getDistanceTOF(self):
        return self._call("getDistanceTOF")

    def getBatteryVoltage(self):
        return self._call("getBatteryVoltage")

    def integrate(self, f, x0, dt, steps=1):
        """Euler step, client side. Public cocos_py2 has no integrate method.

        x <- x + f(x) * dt, repeated `steps` times. Scalar only.
        The simulator's Robot.integrate is internal unicycle dynamics and is
        not this helper.
        """
        if steps < 1:
            raise CoachbotError("invalid_args: integrate steps must be >= 1")
        x = x0
        for _ in range(steps):
            x = x + f(x) * dt
        return x
