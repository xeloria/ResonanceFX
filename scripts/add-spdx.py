# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
"""Adds GPL-3.0-or-later SPDX headers to ResonanceFX's own source files (idempotent).

Usage: python scripts/add-spdx.py [--check] <files...>
"""
import sys

TAG = "SPDX-License-Identifier: GPL-3.0-or-later"
COPYRIGHT = "Copyright (C) 2026 Xeloria"


def header_for(path):
    if path.endswith((".kt", ".kts", ".cpp", ".h", ".c")):
        return f"// {TAG}\n// {COPYRIGHT}\n"
    if path.endswith((".sh", ".py", ".txt")) or path.endswith("CMakeLists.txt") or "update-binary" in path:
        return f"# {TAG}\n# {COPYRIGHT}\n"
    return None


check = "--check" in sys.argv
missing = []
for path in [a for a in sys.argv[1:] if a != "--check"]:
    head = header_for(path)
    if head is None:
        continue
    text = open(path, encoding="utf-8").read()
    if TAG in text.split("\n", 6)[-1] or TAG in "\n".join(text.split("\n")[:5]):
        continue
    if check:
        missing.append(path)
        continue
    if text.startswith("#!"):
        first, rest = text.split("\n", 1)
        text = f"{first}\n{head}{rest}"
    else:
        text = head + ("\n" if not text.startswith("\n") and path.endswith((".kt", ".kts", ".cpp", ".h")) else "") + text
    open(path, "w", encoding="utf-8", newline="\n").write(text)

if check and missing:
    print("missing SPDX header:\n  " + "\n  ".join(missing))
    sys.exit(1)
