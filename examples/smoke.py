# Smoke test user script: LED on (dim red), drive forward 2 s, stop.
# Run on the Pi with cocos2d already running:
#   cd ~/coachbot-v2/python && python3 -m coachbot_api.run ../examples/smoke.py
def usr(robot):
    robot.setLed(15, 0, 0)
    robot.setVel(30, 30)
    robot.delay(2000)
    robot.setVel(0, 0)
    robot.setLed(0, 0, 0)
