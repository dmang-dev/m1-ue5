"""Gen stage: decode the Marathon music OGGs to WAV.

<source>/00.ogg .. 15.ogg -> <dest>/A_music_NN.wav (16-bit PCM, native rate/
channels). UE5 imports WAV reliably (OGG import support is patchy), and the runtime
loader (AM1RuntimeLevel::LoadRuntimeMusic) reads these from the per-user cache too.

Run:  python Tools/gen_music_assets.py                  # M1SOURCE/Music -> Reference/Music
      python Tools/gen_music_assets.py --source <dir> --dest <dir>
The Asset Setup wizard calls main([...]) to convert the user's own Music/ into the
per-user cache. Emits <dest>/music_manifest.json (track -> duration/rate/channels).
"""
import argparse
import json
import os

import soundfile as sf

# Paths derive from this file's location (no hardcoded drive paths).
_TOOLS = os.path.dirname(os.path.abspath(__file__))
_M1UE5 = os.path.dirname(_TOOLS)
_REPO = os.path.dirname(_M1UE5)
DEFAULT_SRC = os.path.join(_REPO, "M1SOURCE", "Marathon-20250829", "Music")
DEFAULT_OUT = os.path.join(_M1UE5, "Reference", "Music")


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--source", default=DEFAULT_SRC, help="folder of NN.ogg music tracks")
    p.add_argument("--dest", default=DEFAULT_OUT, help="output folder for A_music_NN.wav")
    args = p.parse_args(argv)

    if not os.path.isdir(args.source):
        print(f"[music] no music folder at {args.source}; skipping (game runs without music)")
        return 0
    os.makedirs(args.dest, exist_ok=True)
    manifest = {}
    for i in range(16):
        stem = f"{i:02d}"
        ogg = os.path.join(args.source, f"{stem}.ogg")
        if not os.path.isfile(ogg):
            continue
        data, rate = sf.read(ogg)  # float, shape (n,) mono or (n, ch)
        name = f"A_music_{stem}"
        sf.write(os.path.join(args.dest, f"{name}.wav"), data, rate, subtype="PCM_16")
        ch = data.shape[1] if data.ndim > 1 else 1
        manifest[name] = {"src": f"{stem}.ogg", "rate": rate, "channels": ch,
                          "seconds": round(len(data) / float(rate), 1)}
    with open(os.path.join(args.dest, "music_manifest.json"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2)
    print(f"[music] wrote {len(manifest)} tracks to {args.dest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
