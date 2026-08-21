#!/usr/bin/env bash
set -euo pipefail

# Deterministic ARM64 guest -> x86_64 host source import.
# Digitalis is pinned so the ISA implementation cannot silently drift.
DIGITALIS_REPO="${DIGITALIS_REPO:-https://github.com/DigitalisX64/platform_frameworks_libs_binary_translation.git}"
DIGITALIS_REF="${DIGITALIS_REF:-6f199667f039ff5f33c1d80fe52b59300ec1b663}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

printf '%s\n' "Importing ARM64 guest -> x86_64 host from $DIGITALIS_REF"

git clone --filter=blob:none --no-checkout "$DIGITALIS_REPO" "$TMP/digitalis" >/dev/null 2>&1
git -C "$TMP/digitalis" fetch --depth=1 origin "$DIGITALIS_REF" >/dev/null 2>&1
git -C "$TMP/digitalis" checkout --detach FETCH_HEAD >/dev/null 2>&1

ACTUAL_SHA="$(git -C "$TMP/digitalis" rev-parse HEAD)"
test "$ACTUAL_SHA" = "$DIGITALIS_REF"

# Replace the CPU-emulation implementation with the ARM64-capable Digitalis tree.
# Repository metadata and this repository's CI policy remain local.
rsync -a --delete \
  --exclude='.git/' \
  --exclude='.github/' \
  --exclude='AGENTS.md' \
  "$TMP/digitalis/" "$ROOT/"

# ---------------------------------------------------------------------------
# Remove the RISC-V guest from the source tree, not merely from PRODUCT_PACKAGES.
# Digitalis contains both guest implementations; this branch intentionally does
# not retain a second guest ISA. Paths are removed first, then Soong dependency
# references are pruned from the remaining build files.
# ---------------------------------------------------------------------------
find "$ROOT" -type d -not -path '*/.git/*' \( \
    -iname '*riscv64*' -o -iname '*riscv*' \
  \) -prune -exec rm -rf {} +

python3 - "$ROOT" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
needle = re.compile(r'(?i)riscv64|riscv')

# Files that define build graphs need structural pruning. We remove complete
# Soong blocks whose module name is RISC-V and individual dependency/arch lines
# that point at the removed guest. This deliberately does not rename RISC-V
# identifiers into ARM64: ARM64 must use ARM64 implementations, not renamed code.
bp_files = list(root.rglob('Android.bp')) + list(root.rglob('*.mk'))


def remove_named_soong_blocks(text: str) -> str:
    out = []
    i = 0
    n = len(text)
    while i < n:
        # Find a likely top-level module declaration.
        m = re.search(r'(?m)^(cc_[a-zA-Z0-9_]+|cc_defaults|filegroup|genrule|phony_rule_defaults|ndk_translation_package|prebuilt_[a-zA-Z0-9_]+)\s*\{', text[i:])
        if not m:
            out.append(text[i:])
            break
        start = i + m.start()
        brace = i + m.end() - 1
        out.append(text[i:start])
        depth = 0
        j = brace
        in_str = False
        esc = False
        while j < n:
            c = text[j]
            if in_str:
                if esc:
                    esc = False
                elif c == '\\':
                    esc = True
                elif c == '"':
                    in_str = False
            else:
                if c == '"':
                    in_str = True
                elif c == '{':
                    depth += 1
                elif c == '}':
                    depth -= 1
                    if depth == 0:
                        j += 1
                        break
            j += 1
        block = text[start:j]
        if re.search(r'(?im)^\s*name\s*:\s*["\']([^"\']*(?:riscv64|riscv)[^"\']*)["\']', block):
            # Drop the entire RISC-V module.
            i = j
            continue
        out.append(block)
        i = j
    return ''.join(out)

for p in bp_files:
    if not p.is_file():
        continue
    s = p.read_text()
    if p.suffix == '.bp':
        s = remove_named_soong_blocks(s)
    lines = s.splitlines(keepends=True)
    cleaned = []
    for line in lines:
        # Remove individual dependency entries and arch clauses belonging to
        # the deleted guest. Do not perform textual substitution to ARM64.
        if needle.search(line):
            stripped = line.strip()
            if stripped.startswith('//'):
                continue
            if any(x in stripped for x in (
                'riscv64', 'RISCV64', 'riscv', 'RISCV',
            )):
                continue
        cleaned.append(line)
    p.write_text(''.join(cleaned))

# Remove any stale RISC-V product variables left by the imported tree.
p = root / 'berberis_config.mk'
s = p.read_text() if p.exists() else ''
for name in ('RISCV64', 'riscv64', 'RISCV', 'riscv'):
    s = re.sub(rf'(?m)^.*{re.escape(name)}.*(?:\n|$)', '', s)
p.write_text(s)
PY

# Re-install the branch-specific product configuration after the source import.
cat > "$ROOT/berberis_config.mk" <<'EOF'
# ARM64 guest -> x86_64 host translation product configuration.
include frameworks/libs/native_bridge_support/native_bridge_support.mk

BERBERIS_PRODUCT_PACKAGES_ARM64_TO_X86_64 := \
    libberberis_exec_region \
    libberberis_proxy_libEGL \
    libberberis_proxy_libGLESv1_CM \
    libberberis_proxy_libGLESv2 \
    libberberis_proxy_libGLESv3 \
    libberberis_proxy_libOpenMAXAL \
    libberberis_proxy_libOpenSLES \
    libberberis_proxy_libaaudio \
    libberberis_proxy_libamidi \
    libberberis_proxy_libandroid \
    libberberis_proxy_libandroid_runtime \
    libberberis_proxy_libbinder_ndk \
    libberberis_proxy_libc \
    libberberis_proxy_libcamera2ndk \
    libberberis_proxy_libjnigraphics \
    libberberis_proxy_libm \
    libberberis_proxy_libmediandk \
    libberberis_proxy_libnativehelper \
    libberberis_proxy_libnativewindow \
    libberberis_proxy_libneuralnetworks \
    libberberis_proxy_libvulkan \
    libberberis_proxy_libwebviewchromium_plat_support \
    berberis_prebuilt_arm64 \
    berberis_program_runner_binfmt_misc_arm64 \
    berberis_program_runner_arm64 \
    libberberis_arm64 \
    libgui_digitalis_guest_stub.native_bridge

BERBERIS_PRODUCT_PACKAGES_ARM64_TO_X86_64 += $(NATIVE_BRIDGE_PRODUCT_PACKAGES)

BERBERIS_DISTRIBUTION_ARTIFACTS_ARM64 := \
    system/bin/arm64/app_process64 \
    system/bin/arm64/linker64 \
    system/bin/berberis_program_runner_binfmt_misc_arm64 \
    system/bin/berberis_program_runner_arm64 \
    system/etc/binfmt_misc/arm64_dyn \
    system/etc/binfmt_misc/arm64_exe \
    system/etc/init/berberis.rc \
    system/etc/ld.config.arm64.txt \
    system/lib64/libberberis_arm64.so
EOF

cat > "$ROOT/enable_arm64_to_x86_64.mk" <<'EOF'
# ARM64 guest -> x86_64 host Berberis configuration.
include frameworks/libs/binary_translation/berberis_config.mk

PRODUCT_PACKAGES += $(BERBERIS_PRODUCT_PACKAGES_ARM64_TO_X86_64)
PRODUCT_SYSTEM_PROPERTIES += \
    ro.dalvik.vm.native.bridge=libberberis_arm64.so \
    ro.dalvik.vm.isa.arm64=x86_64 \
    ro.enable.native.bridge.exec=1
PRODUCT_SOONG_NAMESPACES += frameworks/libs/native_bridge_support/android_api/libc
PRODUCT_ARTIFACT_PATH_REQUIREMENT_ALLOWED_LIST += $(BERBERIS_DISTRIBUTION_ARTIFACTS_ARM64)
BUILD_BERBERIS := true
BUILD_BERBERIS_ARM64_TO_X86_64 := true
$(call soong_config_set,berberis,translation_arch,arm64_to_x86_64)
EOF

cat > "$ROOT/ARM64_PORT.md" <<EOF
# ARM64 guest -> x86_64 host

Source: $DIGITALIS_REPO
Pinned commit: $ACTUAL_SHA

This branch contains one guest ISA: AArch64/ARM64.
Host ISA: x86_64.
Native bridge: libberberis_arm64.so.
Android ISA mapping: ro.dalvik.vm.isa.arm64=x86_64.

The importer removes the alternate guest implementation and its build graph
instead of renaming it. A successful import must pass tools/validate_arm64_port.sh.
EOF

printf '%s\n' "ARM64 source import and RISC-V purge complete: $ACTUAL_SHA"
