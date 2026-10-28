#!/usr/bin/env bash
# Run a private PipeWire session with the engine exposed on 0.0.0.0:1884 and a few channels playing
# looping test tones, for driving the UI during development (run inside the dev image):
#   docker run -d --name wavelink-demo -p 127.0.0.1:1884:1884 -v <repo>:/w -w /w/wavelink-linux \
#     wavelink-dev:trixie bash engine/tests/demo.sh build-linux
# Effects load from build-plugins/<arch>/ when present (WAVELINK_PLUGIN_DIR overrides).
set -euo pipefail
BUILD=${1:?build dir}
HERE=$(cd "$(dirname "$0")" && pwd)
# Effect plugins for this machine's architecture, when present (wavelink-linux/build-plugins/<arch>/)
if [ -z "${WAVELINK_PLUGIN_DIR:-}" ] && [ -d "$HERE/../../build-plugins/$(uname -m)" ]; then
  WAVELINK_PLUGIN_DIR=$(cd "$HERE/../../build-plugins/$(uname -m)" && pwd)
  export WAVELINK_PLUGIN_DIR
fi
WORK=$(mktemp -d)
export XDG_RUNTIME_DIR="$WORK/run" HOME="$WORK/home"
mkdir -p "$XDG_RUNTIME_DIR" "$HOME" && chmod 700 "$XDG_RUNTIME_DIR"
dbus-daemon --session --address="unix:path=$XDG_RUNTIME_DIR/bus" --nofork --nopidfile >/dev/null 2>&1 &
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
pipewire >"$WORK/pipewire.log" 2>&1 &
sleep 1
wireplumber >"$WORK/wireplumber.log" 2>&1 &
sleep 2
# A stand-in for real hardware: a null sink named like the Wave:3 headphones.
pw-cli create-node adapter '{ factory.name=support.null-audio-sink node.name=alsa_output.usb-Elgato_Systems_Elgato_Wave_3-00.analog-stereo node.description="Elgato Wave:3 Analog Stereo" media.class=Audio/Sink audio.position=[FL FR] object.linger=true }' >/dev/null
# The Wave:3 mic: a loopback whose source side is named like the device's capture node (the engine
# adds it as the "Elgato Wave:3" channel); tones played into wave3.feed come out of it.
pw-loopback -n wave3-mic --capture-props='media.class=Audio/Sink node.name=wave3.feed audio.position=[FL FR]' \
  --playback-props='media.class=Audio/Source node.name=alsa_input.usb-Elgato_Systems_Elgato_Wave_3_BS0000000000-00.analog-stereo node.description="Elgato Wave:3" audio.position=[FL FR]' \
  >/dev/null 2>&1 &
sleep 1
WAVELINK_FAKE_WAVE3=1 "$BUILD/engine/wavelinkd" --host 0.0.0.0 --port 1884 --config "$WORK/config.json" >"$WORK/engine.log" 2>&1 &
sleep 2
api() { python3 "$HERE/wsclient.py" 1884 "$@" >/dev/null; }
for n in Music Browser Game Voice; do api addInput "{\"kind\": \"software\", \"name\": \"$n\"}"; done
api addMix '{"name": "Record Mix"}'
python3 - <<'EOF'
import json, subprocess
state = json.loads(subprocess.check_output(["python3", "/w/wavelink-linux/engine/tests/wsclient.py", "1884", "getState"]))
mixes = {m["name"]: m["id"] for m in state["model"]["mixes"]}
inputs = {i["name"]: i["id"] for i in state["model"]["inputs"]}
def call(method, **p):
    subprocess.check_call(["python3", "/w/wavelink-linux/engine/tests/wsclient.py", "1884", method, json.dumps(p)], stdout=subprocess.DEVNULL)
for name in ("Music", "Browser", "Game", "Voice"):  # new channels start in no mix
    for mix in mixes.values():
        call("setCell", input=inputs[name], mix=mix, active=True)
call("setMix", mix=mixes["Personal Mix"], outputs=["alsa_output.usb-Elgato_Systems_Elgato_Wave_3-00.analog-stereo"])
call("setCell", input=inputs["Music"], mix=mixes["Stream Mix"], volume=0.62)
call("setCell", input=inputs["Game"], mix=mixes["Chat Mix"], active=False)
call("setCell", input=inputs["Browser"], mix=mixes["Record Mix"], active=False)
call("setCell", input=inputs["Voice"], mix=mixes["Personal Mix"], muted=True)
call("setCell", input=inputs["System"], mix=mixes["Record Mix"], volume=0.4)
EOF
python3 - "$WORK" <<'EOF'
import math, struct, sys, wave
rate = 48000
for name, freq, amp in [("a", 220, 0.25), ("b", 440, 0.5), ("c", 330, 0.12), ("d", 660, 0.35), ("e", 550, 0.06)]:
    with wave.open(f"{sys.argv[1]}/{name}.wav", "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(rate)
        frames = bytearray()
        for n in range(rate * 4):
            env = 0.55 + 0.45 * math.sin(2 * math.pi * 0.7 * n / rate)  # slow swell so meters move
            v = int(32767 * amp * env * math.sin(2 * math.pi * freq * n / rate))
            frames += struct.pack("<hh", v, int(v * 0.8))
        w.writeframes(bytes(frames))
EOF
loop() { while true; do pw-play --target "$1" "$2" >/dev/null 2>&1 || sleep 1; done; }
loop wavelink.input.1 "$WORK/a.wav" &
loop wavelink.input.2 "$WORK/b.wav" &
loop wavelink.input.3 "$WORK/c.wav" &
loop wavelink.input.4 "$WORK/d.wav" &
loop wavelink.input.5 "$WORK/e.wav" &
loop wave3.feed "$WORK/d.wav" &
echo "demo running"
wait
