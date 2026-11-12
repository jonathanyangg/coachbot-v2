# All-APIs test user script for Coachbot v2.
#
# Calls every op the v2 Python client exposes (python/coachbot_api/coachbot.py)
# and prints what each one returns. Every call is wrapped on its own, so an
# error (for example "unavailable: ...") is printed and the script moves on.
#
# Run on the Pi with cocos2d already running:
#   cd ~/coachbot-v2/python && python3 -m coachbot_api.run ../examples/all_apis.py
#
# Motion is minimal: both wheels at 20% for 0.5 s, then stop. LED stays dim (15).
#
# v1 example calls (Coachbot-Swarm/Examples API_Calls/Individual_Calls) and
# their v2 names: delay->delay, get_clock->getClock, get_pose->getPose,
# recv_msg->recvMsg, send_msg->sendMsg, set_led->setLed, set_vel->setVel,
# virtual_id->getVirtualId. All are covered below. v1 cocos_py2 also had
# robot.id (v2: getId) and robot.logger (no v2 equivalent).
import struct
import time

RESULTS = []


def call(label, fn, *args):
    """Run one API call, print its result or error, never raise."""
    try:
        val = fn(*args)
        print("[OK]   %-22s -> %r" % (label, val))
        RESULTS.append((label, "OK", val))
        return True, val
    except Exception as exc:  # noqa: BLE001 - report every failure, keep going
        print("[FAIL] %-22s -> %s: %s" % (label, type(exc).__name__, exc))
        RESULTS.append((label, "FAIL", str(exc)))
        return False, exc


def section(title):
    print("\n=== %s ===" % title)


def usr(robot):
    section("Identity and clock")
    call("getId()", robot.getId)
    call("getVirtualId()", robot.getVirtualId)
    ok, t0 = call("getClock()", robot.getClock)
    call("delay(200)", robot.delay, 200)
    ok1, t1 = call("getClock() again", robot.getClock)
    if ok and ok1:
        print("       clock advanced %.3f s over delay(200)" % (t1 - t0))
    if ok:
        call("sendStartTime(t)", robot.sendStartTime, t0)

    section("LED (dim)")
    for name, rgb in (("red", (15, 0, 0)), ("green", (0, 15, 0)), ("blue", (0, 0, 15))):
        call("setLed %s 15" % name, robot.setLed, *rgb)
        robot.delay(700)
    call("setLed off", robot.setLed, 0, 0, 0)

    section("Motion (20% for 0.5 s, then stop)")
    try:
        call("setVel(20, 20)", robot.setVel, 20, 20)
        robot.delay(500)
    finally:
        call("setVel(0, 0)", robot.setVel, 0, 0)

    section("Messaging (local inbox, no radio)")
    call("recvMsg() drain", robot.recvMsg)
    call("sendMsg(pack)", robot.sendMsg, struct.pack("if", 7, 1.5))
    robot.delay(50)
    ok, msgs = call("recvMsg()", robot.recvMsg)
    if ok and msgs:
        try:
            print("       first message unpacked: %r" % (struct.unpack("if", msgs[0][:8]),))
        except Exception as exc:  # noqa: BLE001
            print("       could not unpack: %s" % exc)

    section("Pose")
    call("getPose()", robot.getPose)
    call("getPoseCamera()", robot.getPoseCamera)

    section("Sensors")
    call("getBatteryVoltage()", robot.getBatteryVoltage)
    call("isCharging()", robot.isCharging)
    for i in range(3):
        call("getDistanceTOF() #%d" % (i + 1), robot.getDistanceTOF)
        robot.delay(200)

    section("Button: press and hold the user button within 5 s")
    ok, first = call("isButtonPressed()", robot.isButtonPressed)
    if ok:
        changed = False
        end = time.time() + 5.0
        while time.time() < end:
            try:
                now = robot.isButtonPressed()
            except Exception as exc:  # noqa: BLE001
                print("[FAIL] isButtonPressed poll -> %s" % exc)
                break
            if now != first:
                print("[OK]   button changed: %r -> %r" % (first, now))
                changed = True
                break
            robot.delay(50)
        if not changed:
            print("       no change seen in 5 s (still %r)" % (first,))
    else:
        print("       skipping 5 s poll: button op returned an error")

    section("Client-side helper")
    call("integrate(x'=1, 0, 0.1, 10)", robot.integrate, lambda x: 1.0, 0.0, 0.1, 10)

    section("Summary")
    for label, status, val in RESULTS:
        print("  %-4s %-22s %r" % (status, label, val))
    fails = sum(1 for _, s, _ in RESULTS if s == "FAIL")
    print("%d calls, %d failed" % (len(RESULTS), fails))
    robot.setVel(0, 0)
    robot.setLed(0, 0, 0)
