# Moving this project to another system

Goal: bring the repo, the (gitignored) source data, the regenerable build output,
and — optionally — the Claude Code chat history to a new machine, and verify the
headless loop still runs.

Notation: `%USERPROFILE%` = your Windows home folder (`C:\Users\<you>`).
`<PROJECT>` = wherever you put the repo on the new machine (e.g. `C:\m1-ue5`).

---

## 1. What actually needs to travel

| Item | In git? | How it moves |
|------|---------|--------------|
| Tooling + docs (`M1UE5/Tools`, `M1UE5/docs`, `CLAUDE.md`, `.gitignore`) | ✅ | `git clone` or folder copy |
| `M1SOURCE/` `M2SOURCE/` `MISOURCE/` (Bungie data) | ❌ gitignored | **must copy manually** — not in the repo |
| `M1UE5/Reference/` (extracted + generated) | ❌ gitignored | regenerate, or copy to skip re-extraction |
| `M1UE5/Content/{Meshes,Levels,Textures,Materials}/M1` etc. | ❌ gitignored | regenerate via the headless build |
| UE `Binaries/Intermediate/Saved/DerivedDataCache` | ❌ gitignored | do **not** copy — let UE rebuild |
| Chat history (transcripts) | n/a | copy `~/.claude` transcript files (section 5) |

The cleanest move: **clone the repo + copy the three `*SOURCE/` folders**, then
**regenerate everything else** on the new machine (sections 3–4). Copying
`Reference/` and the generated `Content/` is optional and only saves time.

---

## 2. Prerequisites on the new machine

- **Unreal Engine 5.7** (matches `M1UE5.uproject` → `"EngineAssociation": "5.7"`).
  The editor CLI lives at:
  `C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe`
- **Python 3.10+** on PATH as `py` (developed on 3.12).
- **Git**, and optionally **GitHub CLI (`gh`)** if you'll push.
- **Pipeline deps:** `pip install -r M1UE5\Tools\requirements.txt`
  (pulls `py-marathon-utils` from PyPI).
- Set the git author so history stays PII-free:
  ```
  git config user.name  dmang-dev
  git config user.email dmang-dev@users.noreply.github.com
  ```

---

## 3. Get the files

Either clone (if pushed to GitHub) **or** copy the whole `<PROJECT>` folder. Then
place the Bungie data — copy `M1SOURCE\`, `M2SOURCE\`, `MISOURCE\` into the repo
root (they are gitignored and never travel through git).

> If you copied the folder wholesale, **delete** `M1UE5\Binaries`,
> `M1UE5\Intermediate`, `M1UE5\Saved`, `M1UE5\DerivedDataCache` before first
> launch — stale derived data from another machine causes shader/asset churn.

---

## 4. Regenerate and verify

From `<PROJECT>\M1UE5`:

```bat
py Tools\extract_reference.py --shapes
py Tools\scen_to_obj.py --all Reference\Maps --out Reference\Geometry

REM headless build — validate on one level first
set M1_LIMIT=1
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "<PROJECT>\M1UE5\M1UE5.uproject" ^
  -ExecutePythonScript="<PROJECT>\M1UE5\Tools\ue5_headless_build.py" ^
  -RenderOffScreen -unattended -nosplash -nopause -stdout

REM then the full 37 (clear the limit)
set M1_LIMIT=
... (same command) ...
```

Success = `Reference\Geometry\ue5_build_result.json` shows `"built": 37` and
`"errors": {}`. Then render one level (section *The pipeline* in
`PROJECT_STATUS.md`) and open the PNG.

> The `Tools\*.py` scripts hardcode `REPO = r"C:\m1-ue5\M1UE5"`. If `<PROJECT>`
> differs, update that constant in `ue5_headless_build.py` and `ue5_mrq_render.py`
> (and the `OUT_DIR`/`RESULT` paths).

---

## 5. Keeping the Claude Code chat history

Claude Code stores each session as a JSONL transcript under:

```
%USERPROFILE%\.claude\projects\<ENCODED-CWD>\<session-id>.jsonl
```

`<ENCODED-CWD>` is the working directory Claude was launched from, with every
path separator, colon, and space replaced by a dash. Examples:

| Launch cwd | Encoded folder |
|------------|----------------|
| `C:\m1-ue5` | `C--m1-ue5` |
| `C:\Users\Me\Desktop\m1ue5` | `C--Users-Me-Desktop-m1ue5` |

To preserve history on the new machine:

1. Decide the cwd you'll launch `claude` from on the new machine (e.g.
   `<PROJECT>`), and compute its encoded folder name with the rule above.
2. Create `%USERPROFILE%\.claude\projects\<NEW-ENCODED-CWD>\` on the new machine.
3. Copy **all** `*.jsonl` transcript files (and any sibling `memory\` folder and
   per-session subfolders) from the **old** machine's encoded folder into the new
   one.
4. Launch `claude` from that same cwd and use `--resume` (or `/resume`) — the
   copied sessions appear in the list.

Notes / gotchas:
- The session **filename is the session id** — keep it unchanged.
- Transcripts can be large (this project's main session is ~13 MB).
- **Transcripts contain PII** (your home paths, machine names, anything typed).
  They are personal data — keep them out of the public repo; never commit them.
- If `--resume` doesn't list a copied session, the encoded folder name doesn't
  match your launch cwd — recompute it and move the file accordingly.
