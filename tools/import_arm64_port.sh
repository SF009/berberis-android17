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

# Replace the CPU-emulation implementation with the complete ARM64-capable
# Digitalis tree. Repository metadata and CI policy remain local.
rsync -a --delete \
  --exclude='.git/' \
  --exclude='.github/' \
  --exclude='AGENTS.md' \
  "$TMP/digitalis/" "$ROOT/"

# ARM64 is the only selected guest ISA on this branch.
python3 - "$ROOT/berberis_config.mk" <<'PY'
from pathlib import Path
import re
p = Path(__import__('sys').argv[1])
s = p.read_text()
# Keep the upstream shared package definitions, remove the RISC-V product
# selection and prepend the ARM64 product/artifact definitions from Digitalis.
s = re.sub(r'BERBERIS_PRODUCT_PACKAGES_RISCV64_TO_X86_64 := \\\n(?:    .*\\\n)+\n', '', s)
s = re.sub(r'BERBERIS_PRODUCT_PACKAGES_RISCV64_TO_X86_64 \+= .*\n', '', s)
s = re.sub(r'BERBERIS_DISTRIBUTION_ARTIFACTS_RISCV64 := \\\n(?:    .*\\\n)+', '', s)
arm64 = '''BERBERIS_PRODUCT_PACKAGES_ARM64_TO_X86_64 := \\
    libberberis_exec_region \\
    libberberis_proxy_libEGL \\
    libberberis_proxy_libGLESv1_CM \\
    libberberis_proxy_libGLESv2 \\
    libberberis_proxy_libGLESv3 \\
    libberberis_proxy_libOpenMAXAL \\
    libberberis_proxy_libOpenSLES \\
    libberberis_proxy_libaaudio \\
    libberberis_proxy_libamidi \\
    libberberis_proxy_libandroid \\
    libberberis_proxy_libandroid_runtime \\
    libberberis_proxy_libbinder_ndk \\
    libberberis_proxy_libc \\
    libberberis_proxy_libcamera2ndk \\
    libberberis_proxy_libjnigraphics \\
    libberberis_proxy_libm \\
    libberberis_proxy_libmediandk \\
    libberberis_proxy_libnativehelper \\
    libberberis_proxy_libnativewindow \\
    libberberis_proxy_libneuralnetworks \\
    libberberis_proxy_libvulkan \\
    libberberis_proxy_libwebviewchromium_plat_support \\
    berberis_prebuilt_arm64 \\
    berberis_program_runner_binfmt_misc_arm64 \\
    berberis_program_runner_arm64 \\
    libberberis_arm64 \\
    libgui_digitalis_guest_stub.native_bridge

BERBERIS_PRODUCT_PACKAGES_ARM64_TO_X86_64 += $(NATIVE_BRIDGE_PRODUCT_PACKAGES)

BERBERIS_DISTRIBUTION_ARTIFACTS_ARM64 := \\
    system/bin/arm64/app_process64 \\
    system/bin/arm64/linker64 \\
    system/bin/berberis_program_runner_binfmt_misc_arm64 \\
    system/bin/berberis_program_runner_arm64 \\
    system/etc/binfmt_misc/arm64_dyn \\
    system/etc/binfmt_misc/arm64_exe \\
    system/etc/init/berberis.rc \\
    system/etc/ld.config.arm64.txt \\
    system/lib64/libberberis_arm64.so

'''
if 'BERBERIS_PRODUCT_PACKAGES_ARM64_TO_X86_64 :=' not in s:
    marker = 'include frameworks/libs/native_bridge_support/native_bridge_support.mk\n\n'
    s = s.replace(marker, marker + arm64, 1)
p.write_text(s)
PY

cat > "$ROOT/enable_arm64_to_x86_64.mk" <<'EOF'
# ARM64 guest -> x86_64 host Berberis configuration.
include frameworks/libs/binary_translation/berberis_config.mk

PRODUCT_PACKAGES += $(BERBERIS_PRODUCT_PACKAGES_ARM64_TO_X86_64)
PRODUCT_SYSTEM_PROPERTIES += ro.dalvik.vm.native.bridge=libberberis_arm64.so
PRODUCT_SYSTEM_PROPERTIES += \
    ro.dalvik.vm.isa.arm64=x86_64 \
    ro.enable.native.bridge.exec=1
PRODUCT_SOONG_NAMESPACES += frameworks/libs/native_bridge_support/android_api/libc
PRODUCT_ARTIFACT_PATH_REQUIREMENT_ALLOWED_LIST += $(BERBERIS_DISTRIBUTION_ARTIFACTS_ARM64)
BUILD_BERBERIS := true
BUILD_BERBERIS_ARM64_TO_X86_64 := true
$(call soong_config_set,berberis,translation_arch,arm64_to_x86_64)
EOF
rm -f "$ROOT/enable_riscv64_to_x86_64.mk"

cat > "$ROOT/ARM64_PORT.md" <<EOF
# ARM64 guest -> x86_64 host

Source: $DIGITALIS_REPO
Pinned commit: $ACTUAL_SHA

The branch uses ARM64/AArch64 as guest ISA and x86_64 as host ISA.
The native bridge library is libberberis_arm64.so.
The Android ISA mapping is ro.dalvik.vm.isa.arm64=x86_64.
EOF

printf '%s\n' "ARM64 source import complete: $ACTUAL_SHA"
