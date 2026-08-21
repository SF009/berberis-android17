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
