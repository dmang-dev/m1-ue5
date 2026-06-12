"""Generate the Marathon-1 HUD interface art lists + a slot->icon mapping header.

Two distinct asset families are emitted, with opposite transparency rules:

  * Panel chrome from Coll_10 (the M1 interface bezel). These are RAW bitmaps
    whose index 0 is a REAL opaque color (the chrome), so they MUST render
    OPAQUE -- forcing index-0 alpha would punch holes in the panel. Tagged
    "transparent": false.
  * Inventory icons (weapon + ammo) from Coll_07. Like the sprites, their
    index 0 is the implicit transparent background and DOES need index-0 alpha.
    Tagged "transparent": true.

The (collection, high_shape -> bitmap) mapping is the canonical M1 inventory
layout; it was verified against Reference/Shapes/Coll_07/shapes.json (each
high_shape resolves to a single low_shape whose bitmap_index is used here).

Outputs:
  Reference/Sprites/render_list_hud.json   (coll,clut,bitmap,out,transparent)
                                           consumed by render_sprites.py
  Reference/Sprites/hud_import_list.json   (png_path,asset_name)
                                           consumed by ue5_import_sprites.py
  Source/M1UE5/M1HudTable.h                weapon slot -> icon asset names,
                                           so the C++ HUD stays data-traceable.

Run with a real Windows python (UE's ThirdParty python) so the emitted PNG
paths are Windows paths the editor can open:
    "I:\\Epic Games\\UE_5.7\\Engine\\Binaries\\ThirdParty\\Python3\\Win64\\python.exe" Tools/gen_hud_assets.py
Order: THIS -> render_sprites.py -> ue5_import_sprites.py.
"""
import json
import os
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.path.join(ROOT, "Reference")
COLL10 = os.path.join(REF, "Shapes", "Coll_10")  # interface chrome (opaque)
COLL07 = os.path.join(REF, "Shapes", "Coll_07")  # inventory icons (keyed alpha)
PNG_OUT = os.path.join(REF, "Sprites", "png")
OUT_RENDER = os.path.join(REF, "Sprites", "render_list_hud.json")
OUT_IMPORT = os.path.join(REF, "Sprites", "hud_import_list.json")
OUT_HEADER = os.path.join(ROOT, "Source", "M1UE5", "M1HudTable.h")

# --- Panel chrome (Coll_10). bitmap 001 is the main 435x325 panel; 000 is the
#     490x93 header strip. We ship both -- the C++ HUD picks the main panel. ---
PANEL_CHROME = [
    (1, "T_hud_panel"),   # 001 = main interface panel (the bottom bezel)
    (0, "T_hud_header"),  # 000 = upper strip (available if we want it later)
]

# --- Inventory icons (Coll_07). Keyed by weapon SLOT (0..6 = Fist..Alien),
#     matching UM1WeaponComponent's Weapons order. Values are the bitmap index
#     in Coll_07 for the weapon icon and the ammo icon. None = no icon.
#     "sec_ammo" is the secondary-trigger ammo icon (AR grenade). ---
WEAPON_ICONS = {
    # slot:  (weapon_bmp, ammo_bmp, sec_ammo_bmp)
    0: (None, None, None),  # Fist -- no inventory art
    1: (0,    12,   None),  # Magnum    : pistol icon / pistol-ammo icon
    2: (1,    13,   None),  # Fusion    : plasma icon / plasma-cell icon
    3: (2,    5,    6),      # Assault Rifle: AR icon / AR ammo / grenade (sec)
    4: (3,    4,    None),  # Rocket    : launcher icon / missile icon
    5: (9,    10,   None),  # Flamethrower: flamer icon / canister icon
    6: (11,   None, None),  # Alien     : alien icon / (depletes, no ammo icon)
}


def png_size(path):
    with open(path, "rb") as fh:
        head = fh.read(33)
    if head[:8] != b"\x89PNG\r\n\x1a\n" or head[12:16] != b"IHDR":
        raise ValueError("not a PNG: " + path)
    w, h = struct.unpack(">II", head[16:24])
    return w, h


def main():
    renders = []   # de-duped (coll,clut,bitmap) render jobs
    imports = []   # png -> asset_name import jobs
    seen = set()   # (coll, bitmap) already queued for render

    def add(coll, bitmap, asset_name, transparent, src_dir):
        """Queue one chrome/icon bitmap for render + import, de-duping renders."""
        if bitmap is None:
            return None
        src = os.path.join(src_dir, "bitmaps", "%03d.png" % bitmap)
        if not os.path.isfile(src):
            print("  WARN missing source bitmap:", src)
            return None
        png = os.path.join(PNG_OUT, "c%02d_k0_%03d.png" % (coll, bitmap))
        key = (coll, bitmap)
        if key not in seen:
            seen.add(key)
            renders.append({"coll": coll, "clut": 0, "bitmap": bitmap,
                            "out": png, "transparent": transparent})
        imports.append({"png_path": png, "asset_name": asset_name})
        return asset_name

    # Panel chrome (opaque).
    for bmp, name in PANEL_CHROME:
        add(10, bmp, name, False, COLL10)

    # Weapon + ammo icons (keyed alpha). Build the slot->name table as we go.
    rows = []  # (slot, wpn_name, ammo_name, sec_ammo_name)
    for slot in range(7):
        wpn_bmp, ammo_bmp, sec_bmp = WEAPON_ICONS[slot]
        wpn_name = add(7, wpn_bmp, "T_hud_wpn%d" % slot, True, COLL07)
        ammo_name = add(7, ammo_bmp, "T_hud_ammo%d" % slot, True, COLL07)
        sec_name = add(7, sec_bmp, "T_hud_ammo%d_sec" % slot, True, COLL07)
        rows.append((slot, wpn_name, ammo_name, sec_name))

    os.makedirs(PNG_OUT, exist_ok=True)
    os.makedirs(os.path.dirname(OUT_RENDER), exist_ok=True)
    json.dump(renders, open(OUT_RENDER, "w", encoding="utf-8"), indent=1)
    json.dump(imports, open(OUT_IMPORT, "w", encoding="utf-8"), indent=1)

    # Generated slot->icon table consumed by M1HUD.cpp. Keeping the asset names
    # here (not hardcoded in C++) keeps the mapping data-traceable to this gen.
    def cstr(name):
        return ('TEXT("%s")' % name) if name else "nullptr"

    lines = [
        "#pragma once",
        "",
        "// GENERATED by Tools/gen_hud_assets.py -- do not edit by hand.",
        "// Per weapon-slot inventory icon asset names for the M1 HUD panel.",
        "// Icons live at /Game/Sprites/HUD/<name>. nullptr = no art for that slot",
        "// (Fist has no inventory icon; the Alien weapon depletes, no ammo icon).",
        "",
        "#include \"CoreMinimal.h\"",
        "",
        "struct FM1HudIconRow",
        "{",
        "\tconst TCHAR* Weapon;    // weapon inventory icon, or nullptr",
        "\tconst TCHAR* Ammo;      // primary ammo icon, or nullptr",
        "\tconst TCHAR* SecAmmo;   // secondary-trigger ammo icon (AR grenade), or nullptr",
        "};",
        "",
        "// Indexed by weapon slot 0..6 (Fist, Magnum, Fusion, Assault Rifle,",
        "// Rocket, Flamethrower, Alien) -- same order as UM1WeaponComponent.",
        "static const FM1HudIconRow GM1HudIcons[] =",
        "{",
    ]
    for slot, wpn, ammo, sec in rows:
        lines.append("\t{ %s, %s, %s }," % (cstr(wpn), cstr(ammo), cstr(sec)))
    lines += ["};", ""]
    open(OUT_HEADER, "w", encoding="utf-8", newline="\n").write("\n".join(lines))

    # Report.
    panels = sum(1 for r in renders if not r["transparent"])
    icons = sum(1 for r in renders if r["transparent"])
    print("HUD asset gen:")
    print("  render jobs : %d  (%d panel chrome opaque, %d icons keyed)"
          % (len(renders), panels, icons))
    print("  import jobs : %d" % len(imports))
    for r in renders:
        src = os.path.join(COLL10 if r["coll"] == 10 else COLL07,
                           "bitmaps", "%03d.png" % r["bitmap"])
        try:
            w, h = png_size(src)
            sz = "%dx%d" % (w, h)
        except Exception:
            sz = "?"
        print("    coll%02d bmp%03d %-6s transparent=%s"
              % (r["coll"], r["bitmap"], sz, r["transparent"]))
    print("  wrote:", OUT_RENDER)
    print("  wrote:", OUT_IMPORT)
    print("  wrote:", OUT_HEADER)


if __name__ == "__main__":
    main()
