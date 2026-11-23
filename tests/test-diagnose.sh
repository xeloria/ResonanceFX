#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Xeloria
# Exercises the module's diagnosis decision tree: every failure must produce its own stable
# code, and the helpers that read a config entry, an ELF header and a factory error must agree
# with what the AIDL factory does.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SANDBOX="$(mktemp -d)"
trap 'rm -rf "$SANDBOX"' EXIT

mkdir -p "$SANDBOX/mod/engine/arm64-v8a" "$SANDBOX/data" "$SANDBOX/bin"
cp "$ROOT/module/common.sh" "$SANDBOX/mod/common.sh"
: > "$SANDBOX/mod/engine/arm64-v8a/libresonancefx_aidl_36.so"
printf '#!/bin/sh\necho test\n' > "$SANDBOX/bin/getprop"
chmod +x "$SANDBOX/bin/getprop"
export RFX_DATA="$SANDBOX/data" MODDIR="$SANDBOX/mod" RFX_LOGFILE=/dev/null
export PATH="$SANDBOX/bin:$PATH"
SHELL_CMD="${1:-bash}"

fail=0
check() { # name expected actual
    if [ "$2" = "$3" ]; then echo "PASS  $1"; else echo "FAIL  $1: expected '$2', got '$3'"; fail=1; fi
}

run() { $SHELL_CMD -c '. "$MODDIR/common.sh"; eval "$1"' _ "$1"; }

UUID=4daa8eaa-0842-4a8d-82e9-6eee5827fbe4
TYPE=ec7178ec-e5e1-4432-a3f4-4657e6795210
STATE="$SANDBOX/data/state"
mkdir -p "$STATE"

mkcfg() { # mkcfg <library> <path> [type]
    f="$SANDBOX/$1-$2.xml"
    {
        echo '<audio_effects_conf version="2.0">'
        echo '  <libraries>'
        [ -n "$2" ] && echo "    <library name=\"$1\" path=\"$2\"/>"
        echo '  </libraries>'
        echo '  <effects>'
        echo "    <effect name=\"resonancefx_standard\" library=\"$1\" uuid=\"$UUID\"${3:+ type=\"$3\"}/>"
        echo '  </effects>'
        echo '</audio_effects_conf>'
    } > "$f"
    echo "$f"
}

# The diagnosis tree ends in OK when everything is in place; individual cases override one
# function to move the failure one step earlier.
diag() { # diag <overrides>
    run "$1; rfx_diagnose; cut -d'|' -f1 \"\$RFX_STATE/diagnosis\""
}

base() { # base <config path>
    cat <<EOF
rfx_api() { echo 37; }
rfx_lib_targets() { echo 'lib64 arm64-v8a'; }
rfx_factory_mode() { echo aidl; }
rfx_lib_visible() { return 0; }
rfx_engine_registered() { return 0; }
rfx_factory_error_line() { :; }
rfx_resolve_library() { echo '/system/lib64/soundfx/libresonancefx_aidl_36.so'; }
rfx_elf_abi() { echo '64 arm64'; }
rfx_expected_elf_abi_for() { echo '64 arm64'; }
rfx_first_config() { echo '$1'; }
EOF
}

printf 'cfg|staged\n' > "$STATE/configs"
printf "cfg|$SANDBOX/src\n" > "$STATE/config_sources"

ok_cfg="$(mkcfg resonancefx_aidl libresonancefx_aidl_36.so "$TYPE")"
check "OK" OK "$(diag "$(base "$ok_cfg")")"

# E02: no build for this CPU.
check "E02" E02 "$(diag "rfx_lib_targets() { :; }; rfx_factory_mode() { echo aidl; }")"

# E07: AIDL factory, but this package has no AIDL build.
check "E07" E07 "$(diag "rfx_api() { echo 37; }; rfx_lib_targets() { echo 'lib64 arm64-v8a'; }; rfx_factory_mode() { echo aidl; }; rfx_aidl_lib() { :; }")"

# E15: factory generation unknown.
check "E15" E15 "$(diag "rfx_api() { echo 37; }; rfx_lib_targets() { echo 'lib64 arm64-v8a'; }; rfx_factory_mode() { echo unknown; }")"

# E03: the library exists in this shell but not in the effect process.
touch "$SANDBOX/staged.so"
check "E03: mount not shared" E03 "$(diag "$(base "$ok_cfg")
rfx_lib_visible() { return 1; }; rfx_expected_lib_path() { echo '$SANDBOX/staged.so'; }")"

# E03: the module never staged the library.
check "E03: not staged" E03 "$(diag "$(base "$ok_cfg")
rfx_lib_visible() { return 1; }; rfx_expected_lib_path() { echo '$SANDBOX/missing.so'; }")"

# E05: a config was found but none could be patched.
: > "$STATE/configs"
check "E05" E05 "$(diag "$(base "$ok_cfg")")"
printf 'cfg|staged\n' > "$STATE/configs"

# E04: no config was found at all.
: > "$STATE/configs"
: > "$STATE/config_sources"
check "E04: none found" E04 "$(diag "$(base "$ok_cfg")")"
printf 'cfg|staged\n' > "$STATE/configs"
printf "cfg|$SANDBOX/src\n" > "$STATE/config_sources"

# E04: configs exist but the factory's file is not where it reads.
check "E04: factory config missing" E04 "$(diag "$(base '')")"

# E06: the factory's config has no entry for us.
plain="$SANDBOX/plain.xml"
printf '<audio_effects_conf><libraries/><effects/></audio_effects_conf>\n' > "$plain"
check "E06" E06 "$(diag "$(base "$plain")")"

# E08: AIDL factory but the entry uses the legacy build and no type.
legacy_cfg="$(mkcfg resonancefx libresonancefx.so '')"
check "E08: AIDL got a legacy entry" E08 "$(diag "$(base "$legacy_cfg")")"
# E08: legacy factory but the entry uses the AIDL build.
check "E08: legacy got an AIDL entry" E08 "$(diag "$(base "$ok_cfg")
rfx_factory_mode() { echo legacy; }")"

# E09: the config names a library the factory cannot find.
check "E09" E09 "$(diag "$(base "$ok_cfg")
rfx_resolve_library() { :; }")"

# E10: the library resolves but is the wrong architecture.
check "E10: ABI" E10 "$(diag "$(base "$ok_cfg")
rfx_engine_registered() { return 1; }
rfx_elf_abi() { echo '32 arm'; }")"

# E10: the linker namespace rejects the path.
check "E10: namespace" E10 "$(diag "$(base "$ok_cfg")
rfx_engine_registered() { return 1; }
rfx_factory_error_line() { echo 'openEffectLibrary: dlopen failed, err: \"/vendor/lib64/soundfx/lib.so\" is not accessible for the namespace \"(default)\"'; }")"

# E09: the factory itself says it cannot find the library.
check "E09: factory missing" E09 "$(diag "$(base "$ok_cfg")
rfx_engine_registered() { return 1; }
rfx_factory_error_line() { echo \"Could not find library in effect directories: libresonancefx_aidl_36.so\"; }")"

# E11: the config parser rejected the patched file.
check "E11" E11 "$(diag "$(base "$ok_cfg")
rfx_engine_registered() { return 1; }
rfx_factory_error_line() { echo 'Failed to parse /vendor/etc/audio_effects.xml'; }")"

# E14: everything is in place but the factory has not registered us.
check "E14" E14 "$(diag "$(base "$ok_cfg")
rfx_engine_registered() { return 1; }")"

# E16: the shell cannot see the audio processes (private PID namespace).
check "E16" E16 "$(diag "$(base "$ok_cfg")
rfx_effect_pids() { :; }
getprop() { echo running; }")"

# Pre-Android 8: no XML at all, the legacy factory reads /vendor/etc/audio_effects.conf.
cat > "$SANDBOX/pre-o.conf" <<EOF
libraries {
  resonancefx {
    path /vendor/lib64/soundfx/libresonancefx.so
  }
}
effects {
  resonancefx_standard {
    library resonancefx
    uuid $UUID
  }
}
EOF
check "OK: pre-O .conf" OK "$(diag "$(base '')
rfx_factory_mode() { echo legacy; }
rfx_first_conf() { echo '$SANDBOX/pre-o.conf'; }")"

# ---- helpers ---------------------------------------------------------------------------------

check "entry: XML" "resonancefx_aidl|libresonancefx_aidl_36.so|$TYPE" "$(run "rfx_config_entry '$ok_cfg'")"

cat > "$SANDBOX/rfx.conf" <<EOF
libraries {
  resonancefx {
    path /vendor/lib64/soundfx/libresonancefx.so
  }
}
effects {
  resonancefx_standard {
    library resonancefx
    uuid $UUID
  }
}
EOF
check "entry: legacy .conf" "resonancefx|/vendor/lib64/soundfx/libresonancefx.so|" \
    "$(run "rfx_config_entry '$SANDBOX/rfx.conf'")"

printf '\177ELF\002\001\001\000\000\000\000\000\000\000\000\000\002\000\267\000' > "$SANDBOX/arm64.elf"
printf '\177ELF\001\001\001\000\000\000\000\000\000\000\000\000\002\000\050\000' > "$SANDBOX/arm.elf"
check "elf: arm64" "64 arm64" "$(run "rfx_elf_abi '$SANDBOX/arm64.elf'")"
check "elf: arm" "32 arm" "$(run "rfx_elf_abi '$SANDBOX/arm.elf'")"

# A 64-bit device with a 32-bit audio HAL: the resolver must find the lib/ copy, not only lib64.
check "resolve: 32-bit HAL" "/vendor/lib/soundfx/libresonancefx.so" \
    "$(run "rfx_lib_targets() { printf 'lib64 arm64-v8a\nlib armeabi-v7a\n'; }; rfx_factory_sees() { [ \"\$1\" = /vendor/lib/soundfx/libresonancefx.so ]; }; rfx_resolve_library libresonancefx.so")"

check "classify: namespace" NS "$(run "rfx_classify_error 'is not accessible for the namespace'")"
check "classify: symbol" SYM "$(run "rfx_classify_error 'cannot locate symbol'")"
check "classify: dependency" DEP "$(run "rfx_classify_error 'cannot open shared object file'")"
check "classify: abi" ABI "$(run "rfx_classify_error 'wrong ELF class'")"
check "classify: selinux" SELINUX "$(run "rfx_classify_error 'Permission denied'")"
check "classify: missing" MISSING "$(run "rfx_classify_error 'Could not find library in effect directories'")"
check "classify: parse" PARSE "$(run "rfx_classify_error 'Failed to parse'")"
check "classify: none" NONE "$(run "rfx_classify_error ''")"

# The app reads the verdict from its own files directory; publish it with a format/boot header.
mkdir -p "$SANDBOX/appfiles"
run "RFX_APP_FILES='$SANDBOX/appfiles'; rfx_diag OK 'Engine registered.' 'detail here' ''; rfx_publish_diagnosis"
check "publish: format header" 1 \
    "$(head -n 1 "$SANDBOX/appfiles/module-diagnosis.txt" | grep -c '^# rfx-diagnosis f=1 ')"
check "publish: body" "OK|Engine registered.|detail here|" \
    "$(tail -n 1 "$SANDBOX/appfiles/module-diagnosis.txt")"

exit $fail
