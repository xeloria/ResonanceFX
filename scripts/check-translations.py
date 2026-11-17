#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
"""Checks that every locale matches the English source.

    scripts/check-translations.py [--allow-missing]

Fails when a translated string is missing, unknown, has different format specifiers,
or a plural is missing a quantity the language needs.
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "app/app/src/main/res"
BASE = "values"
ALLOW_MISSING = "--allow-missing" in sys.argv

FORMAT = re.compile(r"%(?:\d+\$)?[-+#0-9.,]*[a-zA-Z%]")


def load(folder):
    strings, plurals, skipped = {}, {}, set()
    for path in sorted((RES / folder).glob("*.xml")):
        tree = ET.parse(path)
        for node in tree.getroot():
            if node.tag == "string":
                name = node.get("name")
                if node.get("translatable") == "false":
                    skipped.add(name)
                else:
                    strings[name] = "".join(node.itertext())
            elif node.tag == "plurals":
                name = node.get("name")
                plurals[name] = {
                    item.get("quantity"): "".join(item.itertext()) for item in node
                }
    return strings, plurals, skipped


def formats(text):
    return sorted(FORMAT.findall(text.replace("%%", "")))


base_strings, base_plurals, base_skipped = load(BASE)
locales = sorted(
    p.name for p in RES.glob("values-*")
    if p.is_dir() and re.fullmatch(r"values-(?:[a-z]{2,3}(?:-r[A-Z]{2})?|b\+[A-Za-z+]+)", p.name)
)
failures = 0
for locale in locales:
    strings, plurals, _ = load(locale)
    missing = sorted(set(base_strings) - set(strings))
    extra = sorted(set(strings) - set(base_strings) - base_skipped)
    bad_format = sorted(
        key for key in set(base_strings) & set(strings)
        if formats(base_strings[key]) != formats(strings[key])
    )
    bad_plural = {}
    for key, quantities in base_plurals.items():
        translated = plurals.get(key)
        if translated is None:
            missing.append(key)
        elif "other" not in translated:
            bad_plural[key] = sorted(translated)
    if missing and not ALLOW_MISSING:
        failures += len(missing)
        print(f"{locale}: missing {len(missing)}: {', '.join(missing[:8])}"
              + (" …" if len(missing) > 8 else ""))
    if extra:
        failures += len(extra)
        print(f"{locale}: unknown keys {len(extra)}: {', '.join(extra[:8])}")
    if bad_format:
        failures += len(bad_format)
        print(f"{locale}: format mismatch: {', '.join(bad_format)}")
    if bad_plural:
        failures += len(bad_plural)
        print(f"{locale}: plural quantities differ: {', '.join(bad_plural)}")

if not locales:
    print("no locales yet")
if failures:
    print(f"{failures} problem(s)")
    sys.exit(1)
print(f"all {len(locales)} locale(s) complete: {len(base_strings)} strings, {len(base_plurals)} plurals")
