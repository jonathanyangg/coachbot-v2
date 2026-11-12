#!/usr/bin/env python3
"""Rewrite v1 snake_case robot API calls to v2 camelCase. Dry run by default; --write to modify."""
import re, sys, pathlib
RENAMES = {"set_vel": "setVel", "set_led": "setLed", "get_pose": "getPose", "send_msg": "sendMsg",
           "recv_msg": "recvMsg", "get_clock": "getClock", "send_start_time": "sendStartTime",
           "virtual_id": "getVirtualId", "id": "getId"}
# attribute access only: robot.set_vel( ... ; v1 `id`/`virtual_id` were attributes -> become calls
PAT = re.compile(r"\.(%s)\b(\s*\()?" % "|".join(sorted(RENAMES, key=len, reverse=True)))

def fix(src):
    def rep(m):
        name, call = m.group(1), m.group(2)
        new = RENAMES[name]
        if name in ("id", "virtual_id") and not call:
            return "." + new + "()"
        return "." + new + (call or "")
    return PAT.sub(rep, src)

def main(argv):
    write = "--write" in argv
    paths = [pathlib.Path(a) for a in argv if a != "--write"] or [pathlib.Path(".")]
    for p in paths:
        for f in ([p] if p.is_file() else p.rglob("*.py")):
            s = f.read_text(); n = fix(s)
            if n != s:
                print(("rewrote " if write else "would rewrite ") + str(f))
                if write: f.write_text(n)

if __name__ == "__main__":
    main(sys.argv[1:])
