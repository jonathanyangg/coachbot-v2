# Button + motor speed test for Coachbot v2.
# Part 1: motor speed sweep (forward, backward, spin), printed step by step.
# Part 2: 20 s button test - LED green while the button is held, off when released.
# Run on the Pi with cocos2d already running:
#   cd ~/coachbot-v2/python && python3 -m coachbot_api.run ../examples/button_motor_test.py

SPEEDS = [10, 20, 30, 40, 60]   # setVel range is -100..100; kept moderate (low-voltage warnings seen)
STEP_MS = 1500
PAUSE_MS = 700

def drive(robot, left, right, label):
    print(f"[motor] {label}: setVel({left}, {right})", flush=True)
    robot.setVel(left, right)
    robot.delay(STEP_MS)
    robot.setVel(0, 0)
    robot.delay(PAUSE_MS)

def usr(robot):
    robot.setLed(0, 0, 15)  # dim blue = motor test running
    print("== Motor speed sweep (lift the robot or give it room) ==", flush=True)
    robot.delay(2000)
    for s in SPEEDS:
        drive(robot, s, s, f"forward {s}")
    for s in SPEEDS:
        drive(robot, -s, -s, f"backward {s}")
    drive(robot, 30, -30, "spin right 30")
    drive(robot, -30, 30, "spin left 30")
    robot.setVel(0, 0)
    robot.setLed(0, 0, 0)

    print("== Button test: 20 s. Press and hold the button; LED turns green while held ==", flush=True)
    last = None
    presses = 0
    start = robot.getClock()
    while robot.getClock() - start < 20:
        pressed = robot.isButtonPressed()
        if pressed != last:
            if pressed:
                presses += 1
                robot.setLed(0, 15, 0)
                print(f"[button] PRESSED (#{presses})", flush=True)
            else:
                robot.setLed(0, 0, 0)
                if last is not None:
                    print("[button] released", flush=True)
            last = pressed
        robot.delay(50)
    robot.setLed(0, 0, 0)
    print(f"== Done. Button presses seen: {presses} ==", flush=True)
