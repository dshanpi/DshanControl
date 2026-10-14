include $(sort $(wildcard $(BR2_EXTERNAL_DSHANCONTROL_PATH)/package/*/*.mk))

# These vendor libraries/examples use C++11 and GNU typeof, while the S4
# GCC 5 toolchain defaults to GNU C++98. Set only their C++ compiler commands;
# private prevents this setting from leaking into prerequisite packages.
# Camera examples are compiled by the vendor staging-install recipe.
$(SDK_MEMORY_DIR)/.stamp_built \
$(SDK_ENDEC_DIR)/.stamp_staging_installed \
$(SDK_CAMERA_DIR)/.stamp_built \
$(SDK_CAMERA_DIR)/.stamp_staging_installed: private TARGET_CXX := $(TARGET_CXX) -std=gnu++11

# The multiscreen example calls atoi without including its declaration.
$(SDK_COMMON_DIR)/.stamp_built: private TARGET_CXX := $(TARGET_CXX) -include cstdlib

# The vendor recipe selects this directory only for its downloaded Linaro
# preset. Our pinned preinstalled softfp toolchain uses the same ARM libraries.
$(TPLAYER_DIR)/.stamp_staging_installed \
$(TPLAYER_DIR)/.stamp_target_installed: private AUDIO_LIBS = $(@D)/arm-linux-gnueabi

# SITE_METHOD=local bypasses Buildroot's normal patch phase. Apply the
# preinstalled-toolchain selector to the copied build tree after rsync.
define DSHANCONTROL_PATCH_LIBCEDARSE
	$(APPLY_PATCHES) $(@D) $(BR2_EXTERNAL_DSHANCONTROL_PATH)/patches/libcedarse \*.patch
endef
LIBCEDARSE_POST_RSYNC_HOOKS += DSHANCONTROL_PATCH_LIBCEDARSE

# Match build/mkcmd.sh:pack_buildroot_rootfs for the no-ramdisk S4 board.
# Buildroot 'all' bypasses the SDK packer; retain unknown entries for review.
define DSHANCONTROL_INSTALL_BUSYBOX_INIT
	test -d "$(TARGET_DIR)" && test ! -L "$(TARGET_DIR)" && test "$$(readlink -f "$(TARGET_DIR)")" != /
	test -d "$(TARGET_DIR)/bin" && test ! -L "$(TARGET_DIR)/bin"
	test -f "$(TARGET_DIR)/bin/busybox" && test ! -L "$(TARGET_DIR)/bin/busybox" && test -x "$(TARGET_DIR)/bin/busybox"
	grep -qx 'CONFIG_INIT=y' "$(BUSYBOX_DIR)/.config"
	if test -e "$(TARGET_DIR)/init" || test -L "$(TARGET_DIR)/init"; then \
		test -L "$(TARGET_DIR)/init" && test "$$(readlink "$(TARGET_DIR)/init")" = bin/busybox; \
	else \
		ln -s bin/busybox "$(TARGET_DIR)/init"; \
	fi
endef
TARGET_FINALIZE_HOOKS += DSHANCONTROL_INSTALL_BUSYBOX_INIT
