#!/usr/bin/env bash
# End-to-end engine test against a private PipeWire + WirePlumber session (dev image or native Linux).
#   engine/tests/integration.sh <build-dir>
set -euo pipefail
BUILD=${1:?build dir}
HERE=$(cd "$(dirname "$0")" && pwd)
# Effect plugins for this machine's architecture, when present (wavelink-linux/build-plugins/<arch>/)
if [ -z "${WAVELINK_PLUGIN_DIR:-}" ] && [ -d "$HERE/../../build-plugins/$(uname -m)" ]; then
  WAVELINK_PLUGIN_DIR=$(cd "$HERE/../../build-plugins/$(uname -m)" && pwd)
  export WAVELINK_PLUGIN_DIR
fi
WORK=$(mktemp -d)
export XDG_RUNTIME_DIR="$WORK/run" HOME="$WORK/home" PIPEWIRE_RUNTIME_DIR="$WORK/run"
mkdir -p "$XDG_RUNTIME_DIR" "$HOME" && chmod 700 "$XDG_RUNTIME_DIR"
PORT=18840
pids=()
cleanup() { kill "${pids[@]}" 2>/dev/null || true; wait 2>/dev/null || true; rm -rf "${WORK:?}"; }
trap cleanup EXIT

dbus-daemon --session --address="unix:path=$XDG_RUNTIME_DIR/bus" --nofork --nopidfile >/dev/null 2>&1 & pids+=($!)
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
pipewire >"$WORK/pipewire.log" 2>&1 & pids+=($!)
sleep 1
wireplumber >"$WORK/wireplumber.log" 2>&1 & pids+=($!)
sleep 2
# Speakers the user listened to before the engine: they become the default sink, then the Personal Mix output.
pw-cli create-node adapter '{ factory.name=support.null-audio-sink node.name=alsa_output.test-speakers media.class=Audio/Sink audio.position="FL,FR" object.linger=true }' >/dev/null
# The Wave:3 is connected at first start. pw-cli-created null sinks get no ports with a mono position
# (PipeWire 1.4), so the stand-in is stereo.
wave3_node=alsa_input.usb-Elgato_Systems_Elgato_Wave_3_BS0000000000-00.analog-stereo
pw-cli create-node adapter "{ factory.name=support.null-audio-sink node.name=$wave3_node node.description=\"Elgato Wave:3\" media.class=Audio/Source audio.position=\"FL,FR\" object.linger=true }" >/dev/null
sleep 1
"$BUILD/engine/wavelinkd" --port $PORT --config "$WORK/config.json" >"$WORK/engine.log" 2>&1 & pids+=($!)
sleep 2

api() { python3 "$HERE/wsclient.py" $PORT "$@"; }
fail() { echo "FAIL: $*"; echo "--- engine log"; cat "$WORK/engine.log"; echo "--- links"; pw-link -l || true; exit 1; }

echo "== virtual devices"
nodes=$(pw-cli ls Node | grep -E 'node.(name|description)' || true)
for want in 'Wave Link System' 'Wave Link Personal Mix' 'Wave Link Stream Mix' 'Wave Link Chat Mix'; do
  grep -q "$want" <<<"$nodes" || fail "missing node '$want'"
  echo "ok  $want"
done

echo "== first-run output"
personal_out=$(api getState | python3 -c 'import json,sys; s=json.load(sys.stdin); print(",".join([m for m in s["model"]["mixes"] if m["name"]=="Personal Mix"][0]["outputs"]))')
default_sink=$(pw-metadata 0 default.configured.audio.sink 2>/dev/null | grep -o "value:'[^']*'" | head -1 || true)
echo "Personal Mix plays on '$personal_out', default sink $default_sink"
[ "$personal_out" = alsa_output.test-speakers ] || fail "Personal Mix did not take over the previous default sink"
echo "$default_sink" | grep -q wavelink.input.1 || fail "Wave Link System is not the default sink"
echo "ok  Personal Mix on the previous default output"
api setSettings '{"manageDefaultSink": false}' >/dev/null
sleep 0.5
restored=$(pw-metadata 0 default.configured.audio.sink 2>/dev/null | grep -o "value:'[^']*'" | head -1 || true)
echo "default sink after turning management off: $restored"
echo "$restored" | grep -q alsa_output.test-speakers || fail "previous default sink not restored"
api setSettings '{"manageDefaultSink": true}' >/dev/null
sleep 0.5
pw-metadata 0 default.configured.audio.sink 2>/dev/null | grep -q wavelink.input.1 || fail "Wave Link System not default again"
echo "ok  default sink handed back and taken again"

echo "== links"
links=$(pw-link -l)
grep -q 'wavelink.input.1:monitor_FL' <<<"$links" || fail "System monitor not linked to the engine"
grep -q 'wavelink.engine:out_1_FL' <<<"$links" || fail "Stream mix output not linked"
echo "ok  engine wired"

# 1 s 1 kHz sine at -6 dBFS into System, recorded from the Stream Mix device (mix slot 1).
python3 - "$WORK/tone.wav" <<'EOF'
import math, struct, sys, wave
rate = 48000
with wave.open(sys.argv[1], "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(rate)
    frames = b"".join(struct.pack("<hh", *(2 * [int(16384 * math.sin(2 * math.pi * 1000 * n / rate))])) for n in range(rate * 2))
    w.writeframes(frames)
EOF

level() {  # prints the RMS in dBFS of what the Stream Mix device carries while the tone plays
  rm -f "${WORK:?}/rec.wav"
  pw-record --target wavelink.mix.2 --rate 48000 --channels 2 --format s16 "$WORK/rec.wav" & local rec=$!
  sleep 0.3
  pw-play --target wavelink.input.1 "$WORK/tone.wav"
  kill -INT $rec; wait $rec 2>/dev/null || true
  python3 - "$WORK/rec.wav" <<'EOF'
import math, struct, sys, wave
with wave.open(sys.argv[1]) as w:
    data = w.readframes(w.getnframes())
samples = struct.unpack("<%dh" % (len(data) // 2), data)
peak_window = max(range(0, max(1, len(samples) - 9600), 4800), key=lambda i: sum(abs(s) for s in samples[i:i + 9600]), default=0)
window = samples[peak_window:peak_window + 9600] or [0]
rms = math.sqrt(sum(s * s for s in window) / len(window)) / 32768
print("%.1f" % (20 * math.log10(rms) if rms > 0 else -120))
EOF
}

tail_level() {  # RMS in dBFS of the last second the Stream Mix carried while the 2 s signal played
  rm -f "${WORK:?}/rec.wav"
  pw-record --target wavelink.mix.2 --rate 48000 --channels 2 --format s16 "$WORK/rec.wav" & local rec=$!
  sleep 0.3
  pw-play --target wavelink.input.1 "$WORK/tone.wav"
  kill -INT $rec; wait $rec 2>/dev/null || true
  python3 - "$WORK/rec.wav" <<'EOF'
import math, struct, sys, wave
with wave.open(sys.argv[1]) as w:
    data = w.readframes(w.getnframes())
s = struct.unpack("<%dh" % (len(data) // 2), data)
end = len(s)
while end > 0 and s[end - 1] == 0:
    end -= 1
seg = s[max(0, end - 96000):end] or [0]
rms = math.sqrt(sum(v * v for v in seg) / len(seg)) / 32768
print("%.1f" % (20 * math.log10(rms) if rms > 0 else -120))
EOF
}

state=$(api getState)
system=$(python3 -c 'import json,sys; s=json.loads(sys.argv[1]); print([i["id"] for i in s["model"]["inputs"] if i["name"]=="System"][0])' "$state")
stream=$(python3 -c 'import json,sys; s=json.loads(sys.argv[1]); print([m["id"] for m in s["model"]["mixes"] if m["name"]=="Stream Mix"][0])' "$state")

echo "== audio path"
full=$(level); echo "full volume: $full dBFS (tone is -9.0 dBFS RMS)"
python3 -c "import sys; sys.exit(0 if abs(float('$full') + 9.0) < 1.5 else 1)" || fail "unexpected level at full volume"

api setCell "{\"input\": \"$system\", \"mix\": \"$stream\", \"volume\": 0.5}" >/dev/null
sleep 0.3
half=$(level); echo "volume 0.5: $half dBFS (expect about -39: Wave Link maps 0.5 to -30 dB)"
python3 -c "import sys; sys.exit(0 if abs(float('$half') + 39.0) < 1.5 else 1)" || fail "volume law not applied"

api setCell "{\"input\": \"$system\", \"mix\": \"$stream\", \"muted\": true}" >/dev/null
sleep 0.3
muted=$(level); echo "muted: $muted dBFS"
python3 -c "import sys; sys.exit(0 if float('$muted') < -90 else 1)" || fail "mute did not silence the mix"

echo "== channel volume (PipeWire node Props, slider^3)"
node_volume() {  # prints the System sink's average linear channel volume and mute flag
  pw-dump | python3 -c '
import json, sys
for o in json.load(sys.stdin):
    props = (o.get("info") or {}).get("props") or {}
    if props.get("node.name") == "wavelink.input.1":
        p = (o["info"].get("params") or {}).get("Props") or [{}]
        v = p[0].get("channelVolumes") or [1]
        print("%.4f %s" % (sum(v) / len(v), str(p[0].get("mute", False)).lower()))
'
}
api setInput "{\"input\": \"$system\", \"volume\": 0.5, \"muted\": true}" >/dev/null
sleep 0.5
read vol mute <<<"$(node_volume)"
echo "node volume $vol mute $mute (expect 0.1250 true)"
python3 -c "import sys; sys.exit(0 if abs(float('$vol') - 0.125) < 0.002 else 1)" && [ "$mute" = true ] || fail "channel volume not applied to the sink"
node_id=$(pw-dump | python3 -c 'import json,sys; print([o["id"] for o in json.load(sys.stdin) if ((o.get("info") or {}).get("props") or {}).get("node.name") == "wavelink.input.1"][0])')
pw-cli s "$node_id" Props '{ channelVolumes: [ 0.512, 0.512 ], mute: false }' >/dev/null
sleep 0.5
reported=$(api getState | python3 -c 'import json,sys; s=json.load(sys.stdin); i=[i for i in s["model"]["inputs"] if i["name"]=="System"][0]; print("%.3f %s" % (i["volume"], str(i["muted"]).lower()))')
echo "after external change engine reports $reported (expect 0.800 false)"
[ "$reported" = "0.800 false" ] || fail "external volume change not picked up"
api setInput "{\"input\": \"$system\", \"volume\": 1.0, \"muted\": false}" >/dev/null

echo "== app routing"
music=$(api addInput '{"kind": "software", "name": "Music"}' | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')
python3 - "$WORK/long.wav" <<'EOF2'
import struct, sys, wave
with wave.open(sys.argv[1], "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(48000)
    w.writeframes(b"\0\0\0\0" * 48000 * 20)
EOF2
pw-play "$WORK/long.wav" & player=$!
sleep 1
before=$(pw-link -l | grep -A3 '^pw-play' | grep -o 'wavelink.input.[0-9]' | head -1 || true)
api routeApp "{\"app\": \"pw-play\", \"input\": \"$music\"}" >/dev/null
sleep 1
after=$(pw-link -l | grep -A3 '^pw-play' | grep -o 'wavelink.input.[0-9]' | head -1 || true)
kill $player 2>/dev/null || true
echo "pw-play played to ${before:-?} and now to ${after:-?} (expect wavelink.input.2)"
[ "$after" = "wavelink.input.2" ] || fail "app was not moved to the Music channel"
api routeApp '{"app": "pw-play", "input": ""}' >/dev/null  # later checks play into System again

if [ -n "${WAVELINK_PLUGIN_DIR:-}" ]; then
  echo "== effects (white noise into System, measured on the Stream Mix)"
  api setCell "{\"input\": \"$system\", \"mix\": \"$stream\", \"volume\": 1.0, \"muted\": false}" >/dev/null
  python3 - "$WORK/tone.wav" <<'EOF2'
import random, struct, sys, wave
random.seed(1)
with wave.open(sys.argv[1], "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(48000)
    frames = bytearray()
    for _ in range(48000 * 2):
        v = int(random.gauss(0, 0.1) * 32767)
        v = max(-32768, min(32767, v))
        frames += struct.pack("<hh", v, v)
    w.writeframes(bytes(frames))
EOF2
  api getEffectTypes | python3 -c 'import json,sys; print("available:", ", ".join(t["type"] for t in json.load(sys.stdin)))'
  dry=$(tail_level); echo "no effect: $dry dBFS"
  fx=$(api addEffect "{\"input\": \"$system\", \"type\": \"VoiceFocus\"}" | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')
  sleep 0.3
  vf=$(tail_level); echo "Voice Focus: $vf dBFS"
  python3 -c "import sys; sys.exit(0 if float('$vf') < float('$dry') - 15 else 1)" || fail "Voice Focus did not suppress noise"
  api removeEffect "{\"input\": \"$system\", \"effect\": \"$fx\"}" >/dev/null
  fx=$(api addEffect "{\"input\": \"$system\", \"type\": \"ElgatoNoiseRemoval\"}" | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')
  sleep 0.3
  nr=$(tail_level); echo "Elgato Noise Removal: $nr dBFS"
  python3 -c "import sys; sys.exit(0 if float('$nr') < float('$dry') - 15 else 1)" || fail "Noise Removal did not suppress noise"
  api removeEffect "{\"input\": \"$system\", \"effect\": \"$fx\"}" >/dev/null
fi

echo "== Sound Check"
python3 - "$WORK/tone.wav" <<'EOF2'
import math, struct, sys, wave
with wave.open(sys.argv[1], "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(48000)
    w.writeframes(b"".join(struct.pack("<hh", *(2 * [int(16384 * math.sin(2 * math.pi * 1000 * n / 48000))])) for n in range(48000 * 2)))
EOF2
pw-play --target wavelink.input.1 "$WORK/tone.wav" & player=$!
sleep 0.3
api soundCheck "{\"action\": \"record\", \"input\": \"$system\"}" >/dev/null
sleep 1.2
api soundCheck '{"action": "stop"}' >/dev/null
wait $player 2>/dev/null || true
sc=$(api getState | python3 -c 'import json,sys; s=json.load(sys.stdin)["soundCheck"]; print(s["state"], s["hasRecording"], "%.3f" % s["length"], sum(1 for b in s["bars"] if b > 0.5))')
echo "after recording: $sc (expect idle True, ~0.12 of 10 s, 3-4 loud bars)"
read st has len loud <<<"$sc"
[ "$st" = idle ] && [ "$has" = True ] && [ "$loud" -ge 2 ] || fail "Sound Check did not record"
api soundCheck '{"action": "play", "output": "wavelink.mix.2"}' >/dev/null
sleep 0.3
playing=$(api getState | python3 -c 'import json,sys; print(json.load(sys.stdin)["soundCheck"]["state"])')
pw-link -l | grep -q '^wavelink.soundcheck' || fail "Sound Check playback stream not linked"
sleep 1.5
done_state=$(api getState | python3 -c 'import json,sys; print(json.load(sys.stdin)["soundCheck"]["state"])')
echo "playback: $playing then $done_state (expect playing then idle)"
[ "$playing" = playing ] && [ "$done_state" = idle ] || fail "Sound Check playback did not run to the end"
api soundCheck '{"action": "output", "output": ""}' >/dev/null
echo "ok  Sound Check"

echo "== Wave:3 channel"
wave3_mixes() {  # mixes the Wave:3 channel is active in, comma-separated
  api getState | python3 -c 'import json,sys; s=json.load(sys.stdin); names={m["id"]: m["name"] for m in s["model"]["mixes"]}; print(",".join(sorted(names[c["mix"]] for c in s["model"]["cells"] if c["input"].startswith("hw:") and c["active"])))'
}
first=$(api getState | python3 -c 'import json,sys; s=json.load(sys.stdin); i=s["model"]["inputs"][0]; print(i["id"] + "|" + i["name"])')
echo "first input: $first"
[ "$first" = "hw:$wave3_node|Elgato Wave:3" ] || fail "Wave:3 was not added as the first input"
in_mixes=$(wave3_mixes)
echo "connected at first start, plays in: $in_mixes (expect Chat Mix,Stream Mix; never the Personal Mix)"
[ "$in_mixes" = "Chat Mix,Stream Mix" ] || fail "first-run mix membership wrong"
wave3_linked() {  # both engine inputs of hardware slot 0 are fed by the device
  local links; links=$(pw-link -l)
  grep -A1 '^wavelink.engine:in_8_FL$' <<<"$links" | grep -q "$wave3_node:capture_" &&
    grep -A1 '^wavelink.engine:in_8_FR$' <<<"$links" | grep -q "$wave3_node:capture_"
}
for _ in $(seq 20); do wave3_linked && break; sleep 0.5; done
wave3_linked || fail "Wave:3 capture not linked to the engine"
echo "ok  Wave:3 input linked"
has_wave3() { api getState | python3 -c 'import json,sys; s=json.load(sys.stdin); sys.exit(0 if any(i["id"].startswith("hw:") for i in s["model"]["inputs"]) else 1)'; }
api removeInput "{\"input\": \"hw:$wave3_node\"}" >/dev/null
sleep 1
! has_wave3 || fail "removed Wave:3 came back while still connected"
echo "ok  removed channel stays removed while the mic stays connected"
pw-cli destroy "$(pw-dump | python3 -c 'import json,sys; print([o["id"] for o in json.load(sys.stdin) if ((o.get("info") or {}).get("props") or {}).get("node.name") == sys.argv[1]][0])' "$wave3_node")" >/dev/null
sleep 0.5
pw-cli create-node adapter "{ factory.name=support.null-audio-sink node.name=$wave3_node media.class=Audio/Source audio.position=\"FL,FR\" object.linger=true }" >/dev/null
sleep 1
has_wave3 || fail "Wave:3 not added again after reconnecting"
in_mixes=$(wave3_mixes)
echo "reconnected, plays in: '$in_mixes' (expect no mix)"
[ -z "$in_mixes" ] || fail "a reconnected Wave:3 must join no mix"
echo "ok  reconnected mic is a channel in no mix"

echo "== persistence"
sleep 1
python3 - "$WORK/config.json" <<'EOF' || fail "config not saved"
import json, sys
c = json.load(open(sys.argv[1]))
assert any(i["name"] == "Music" for i in c["inputs"]), "Music channel missing"
EOF
echo "ok  config saved"
echo "ALL PASSED"
