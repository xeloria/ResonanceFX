# Translating ResonanceFX

The app ships English (`values/`) and six translations: Japanese, Russian, Simplified Chinese,
Spanish, Korean and French. Android picks the device language automatically; **Settings →
Language** in the app overrides it.

## Where the strings live

| File | Contents |
| --- | --- |
| `app/app/src/main/res/values/strings.xml` | App name, notification |
| `.../values/strings_effects.xml` | Effect sections, control labels, choice options, value formats |
| `.../values/strings_ui.xml` | Screens, dialogs, toasts, accessibility labels, plurals |
| `.../values/strings_tables.xml` | Dynamic-system device types and equalizer preset names |
| `.../values/measurement_strings.xml` | **Generated** from `assets/measurements.json` — do not edit |
| `.../values/donottranslate.xml` | Language names, shown in their own language |

Translations go in `values-ja/`, `values-ru/`, `values-zh/`, `values-es/`, `values-ko/`,
`values-fr/`, using the same file names and keys.

## Rules

1. **Never change a key or reorder choice options.** A choice's index is what gets saved in
   user profiles; reordering silently changes their settings.
2. **Keep every format specifier** (`%1$s`, `%2$d`, `%1$.1f`, `%%`) and its argument order.
   `scripts/check-translations.py` verifies this.
3. **Plurals**: Russian needs `one`, `few`, `many` and `other`; Japanese, Chinese and Korean only
   need `other`. Add the quantities your language uses.
4. **Escape** `'` as `\'` and `"` as `&quot;`, as in the English files.
5. **Numbers** in audio values use a dot (`0.8 dB`), matching the published measurements.
   Translate the surrounding words, not the number format.
6. Keep English for: `AutoEQ`, `KEMAR`, `ISO 226`, `FET`, `LUFS`, `THD`, `dB`, `Hz`, `kHz`,
   `phon`, `Q`, product and device names, and technical log/report text.

## Checking

```sh
python scripts/check-translations.py            # must pass
python scripts/check-translations.py --allow-missing   # during work
```

The engine measurement page reads its text from resources keyed by the English effect name and
falls back to `measurements.json` if a key is missing. Regenerate the English side with
`scripts/gen-measurements.py` after running `tests/measure-engine.sh`; existing translations are
left untouched, and the checker reports any new keys.

## Trying a language

Settings → **Language** → pick one. Below Android 13 the activity restarts with the chosen locale;
Android 13+ uses the platform per-app language, which also covers the notification.
