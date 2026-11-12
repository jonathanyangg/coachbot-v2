# CLI

`cocos2d` does not spawn user code. v1 did, from `coachbots/cocos` `src/controllers/api/mod.rs` (`python2 -m cocos_py2`). cctl starts user code from the control daemon, which is a different process.

Run the OS, then the client. The example config is the bottom-board BCM map. On a Pi, `gpio=auto` opens libgpiod. A bench `setVel` smoke test is in the main README.

```bash
./build/cocos2d --config os/coachbot.example.conf
python -m coachbot_api.run usr_code.py
```

`--id` and `--virtual-id` override the config file. `--help` prints the flags. Set `gpio=memory` in the config for a dry run that does not claim the header.
