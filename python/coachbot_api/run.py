"""python -m coachbot_api.run usr_code.py [endpoint]"""
import runpy, sys
from .coachbot import Coachbot, DEFAULT_ENDPOINT

def main():
    ns = runpy.run_path(sys.argv[1])
    ns["usr"](Coachbot(sys.argv[2] if len(sys.argv) > 2 else DEFAULT_ENDPOINT))

if __name__ == "__main__":
    main()
