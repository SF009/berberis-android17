# ARM64 guest -> x86_64 host

Source: https://github.com/DigitalisX64/platform_frameworks_libs_binary_translation.git
Pinned commit: 6f199667f039ff5f33c1d80fe52b59300ec1b663

The branch uses ARM64/AArch64 as guest ISA and x86_64 as host ISA.
The native bridge library is libberberis_arm64.so.
The Android ISA mapping is ro.dalvik.vm.isa.arm64=x86_64.
