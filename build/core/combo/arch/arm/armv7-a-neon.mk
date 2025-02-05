# Configuration for Linux on ARM.
# Generating binaries for the ARMv7-a architecture and higher with NEON
#
ARCH_ARM_HAVE_ARMV7A            := true
ARCH_ARM_HAVE_VFP               := true
ARCH_ARM_HAVE_VFP_D32           := true
ARCH_ARM_HAVE_NEON              := true

ifneq (,$(filter cortex-a15 krait denver,$(TARGET_$(combo_2nd_arch_prefix)CPU_VARIANT)))
	arch_variant_cflags := -mcpu=cortex-a15
else
ifeq ($(strip $(TARGET_$(combo_2nd_arch_prefix)CPU_VARIANT)),cortex-a8)
	arch_variant_cflags := -mcpu=cortex-a8
else
ifeq ($(strip $(TARGET_$(combo_2nd_arch_prefix)CPU_VARIANT)),cortex-a7)
	arch_variant_cflags := -mcpu=cortex-a7
else
ifeq ($(strip $(TARGET_$(combo_2nd_arch_prefix)CPU_VARIANT)),cortex-a17.cortex-a7)
	arch_variant_cflags := -mcpu=cortex-a12.cortex-a7
else
ifeq ($(strip $(TARGET_$(combo_2nd_arch_prefix)CPU_VARIANT)),cortex-a53)
	arch_variant_cflags := -mcpu=cortex-a53
else
	arch_variant_cflags := -march=armv7-a
endif
endif
endif
endif
endif

arch_variant_cflags += \
    -mfloat-abi=softfp \
    -mfpu=neon

arch_variant_ldflags := \
	-Wl,--no-fix-cortex-a8
