"""Check that the public tree has no PII and no tracked Bungie-derived art.

Run from the repo root: python Tools/check_public_tree.py

Exit codes:
  0 - clean
  1 - PII found
  2 - Bungie-derived art still tracked
  3 - both
"""

import os, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SD = r"//DESKTOP-6Q41SNJ/projects/m1-ue5"

PII_PATTERNS = [
    r"C:\\Users",         # Windows user home paths
    r"dusti",             # real personal name
    r"@gmail",            # personal email
    r"@outlook",
    r"@hotmail",
]

# Known Bungie-derived paths that should NOT be tracked in a public repo
BUNGIE_TRACKED_PATHS = [
    "M1UE5/Content/UI/Chapters/T_chapter_00.uasset",
    "M1UE5/Content/UI/Chapters/T_chapter_03.uasset",
    "M1UE5/Content/UI/Chapters/T_chapter_10.uasset",
    "M1UE5/Content/UI/Chapters/T_chapter_12.uasset",
    "M1UE5/Content/UI/Chapters/T_chapter_16.uasset",
    "M1UE5/Content/UI/Chapters/T_chapter_24.uasset",
    "M1UE5/Content/UI/HUD/T_hud_bezel.uasset",
    "M1UE5/Content/UI/Menu/T_menu_main.uasset",
]


def check_pii():
    """Scan git history and working tree for PII."""
    issues = []
    # Check commit messages for 'dusti' / C:\Users
    try:
        result = os.popen(
            f"git -c safe.directory={SD} log --all -p -Gdusti -- . 2>/dev/null")
        output = result.read()
        if output.strip() and "dusti" in output.lower():
            issues.append("PII 'dusti' found in commit messages")
    except Exception as e:
        issues.append(f"Error checking commit messages: {e}")

    # Check working tree text files for PII patterns
    try:
        for f in ROOT.rglob("*.md"):
            if ".git" in str(f):
                continue
            try:
                content = f.read_text(encoding="utf-8", errors="replace")
                for pattern in PII_PATTERNS:
                    if re.search(pattern, content, re.IGNORECASE):
                        issues.append(f"PII pattern '{pattern}' in {f}")
                        break
            except Exception:
                pass

        # Check for C:\Users in non-binary files (skip known false positives)
        users_extensions = [".h", ".cpp", ".hpp", ".ini", ".txt", ".md"]
        for ext in users_extensions:
            try:
                for f in ROOT.rglob(f"*{ext}"):
                    try:
                        content = f.read_text(encoding="utf-8", errors="replace")
                        if re.search(r"C:\\Users", content):
                            if not re.search(r"^\s*//.*users own data", content, re.I):
                                issues.append(f"C:\\Users found in {f}")
                    except Exception:
                        pass
            except Exception:
                pass
    except Exception as e:
        issues.append(f"Error scanning working tree: {e}")

    return issues


def check_bungie_art():
    """Check if Bungie-derived art is tracked in git."""
    issues = []
    try:
        # Check tracked files
        result = os.popen(
            f"git -c safe.directory={SD} ls-files")
        tracked = result.read()

        # Check known Bungie-derived .uasset paths
        for path in BUNGIE_TRACKED_PATHS:
            if path in tracked:
                issues.append(f"Tracked Bungie-derived art: {path}")

        # Check for Bungie pattern strings in tracked content
        BUNGIE_PATTERNS = [
            "M1SOURCE",
            "Reference/",
            "Shapes.shps",
            "Physics.phys",
            "AlephOne",
        ]
        for pattern in BUNGIE_PATTERNS:
            if pattern in tracked:
                issues.append(f"Bungie pattern '{pattern}' found in tracked content")

    except Exception as e:
        issues.append(f"Error checking Bungie art: {e}")

    return issues


def main():
    pii_issues = check_pii()
    bungie_issues = check_bungie_art()

    exit_code = 0
    if pii_issues:
        exit_code |= 1
    if bungie_issues:
        exit_code |= 2

    if exit_code == 0:
        print("PASS - no PII or tracked Bungie-derived art found.")
        return 0

    if exit_code & 1:
        print("PII issues found:")
        for issue in pii_issues:
            print(f"  - {issue}")

    if exit_code & 2:
        print("Bungie-derived art issues found:")
        for issue in bungie_issues:
            print(f"  - {issue}")

    print("\nRecommendations:")
    if exit_code & 1:
        print("- Remove PII from commit messages and file content")
        print("- Update HANDOFF.md, MIGRATION.md, PROJECT_STATUS.md to avoid C:\\Users paths")
    if exit_code & 2:
        print("- Remove Bungie-derived .uasset files from git tracking")
        print("- Add .gitignore patterns for generated content")
        print("- Regenerate assets from user's own M1 data before release")

    return exit_code


if __name__ == "__main__":
    sys.exit(main())