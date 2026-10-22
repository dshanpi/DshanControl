DSHANCONTROL_IMX6ULL_VERSION ?= 0584604f5c038fb21040a1cd6c90ab2f5b5e847a
DSHANCONTROL_IMX6ULL_SITE = $(BR2_EXTERNAL_DSHAN6ULL_PATH)/../../..
DSHANCONTROL_IMX6ULL_SITE_METHOD = local
DSHANCONTROL_IMX6ULL_LICENSE = GPL-3.0-only OFL-1.1
DSHANCONTROL_IMX6ULL_LICENSE_FILES = LICENSE components/hmi/src/fonts/OFL.txt
DSHANCONTROL_IMX6ULL_DEPENDENCIES = qt5base openssl zlib sqlite python-flask host-python3 gst1-plugins-base gst1-plugins-good gst1-libav
DSHANCONTROL_IMX6ULL_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = --exclude=dist --exclude=.test-venv --exclude=__pycache__
define DSHANCONTROL_IMX6ULL_CONFIGURE_CMDS
	cd $(@D)/components/hmi/src && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake omnigate-hmi.pro
	cd $(@D)/components/runtime && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake runtime.pro
	cd $(@D)/components/desktop && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake desktop.pro
	cd $(@D)/components/media && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake media.pro CONFIG+=gstsoftware
endef
define DSHANCONTROL_IMX6ULL_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/hmi/src
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/runtime
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/desktop
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/media
endef
define DSHANCONTROL_IMX6ULL_INSTALL_TARGET_CMDS
	$(HOST_DIR)/bin/python3 $(@D)/boards/imx6ull-pro/install.py --source $(@D) --target $(TARGET_DIR) --identity "$(DSHANCONTROL_IMX6ULL_BUILD_IDENTITY)"
endef
$(eval $(generic-package))
