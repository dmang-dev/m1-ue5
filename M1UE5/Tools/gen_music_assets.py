"""Gen stage: decode the Marathon music OGGs to WAV for UE import.

M1SOURCE/Music/00.ogg .. 15.ogg -> Reference/Music/A_music_NN.wav (16-bit PCM,
native rate/channels). UE5 imports WAV reliably (OGG import support is patchy),
and this matches the rest of the audio pipeline (the 104 SFX are WAV too).

Run with any python that has `soundfile`:
    python Tools/gen_music_assets.py
Then import with Tools/ue5_import_music.py (editor python).

Emits Reference/Music/music_manifest.json (track -> duration/rate/channels).
"""
import json
import os

import soundfile as sf

SRC = r"C:\m1-ue5\M1SOURCE\Marathon-20250829\Music"
OUT = r"C:\m1-ue5\M1UE5\Reference\Music"

os.makedirs(OUT, exist_ok=True)
manifest = {}
for i in range(16):
    stem = f"{i:02d}"
    ogg = os.path.join(SRC, f"{stem}.ogg")
    if not os.path.isfile(ogg):
        print(f"[music] missing {ogg}; skipping")
        continue
    data, rate = sf.read(ogg)  # float, shape (n,) mono or (n, ch)
    name = f"A_music_{stem}"
    wav = os.path.join(OUT, f"{name}.wav")
    sf.write(wav, data, rate, subtype="PCM_16")
    ch = data.shape[1] if data.ndim > 1 else 1
    dur = len(data) / float(rate)
    manifest[name] = {"src": f"{stem}.ogg", "rate": rate, "channels": ch,
                      "seconds": round(dur, 1)}
    print(f"[music] {stem}.ogg -> {name}.wav  ({rate} Hz, {ch}ch, {dur:.1f}s)")

with open(os.path.join(OUT, "music_manifest.json"), "w", encoding="utf-8") as fh:
    json.dump(manifest, fh, indent=2)
print(f"[music] wrote {len(manifest)} tracks to {OUT}")
