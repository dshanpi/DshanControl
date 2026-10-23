DSHANCONTROL_VERSION = 0.1.0-dev
DSHANCONTROL_SITE = $(BR2_EXTERNAL_DSHANCONTROL_PATH)/app-source
DSHANCONTROL_SITE_METHOD = local
DSHANCONTROL_LICENSE = GPL-3.0-only OFL-1.1
DSHANCONTROL_LICENSE_FILES = LICENSE components/hmi/src/fonts/OFL.txt
DSHANCONTROL_DEPENDENCIES = busybox-init-base-files qt5base qt5charts python3 dshancontrol-python openssl zlib sqlite ca-certificates host-python3 wqy-zenhei

# Optional component desktop needs a matching S4 Qt/Cedar runtime. Legacy image
# reproduction keeps its original payload and two application identities.
ifeq ($(BR2_PACKAGE_DSHANCONTROL_COMPONENT_DESKTOP),y)
DSHANCONTROL_DEPENDENCIES += tplayer libpng jpeg
DSHANCONTROL_DESKTOP_USER = dshan-desktop 402 dshan-app 400 * - - - DshanControl_desktop
DSHANCONTROL_DESKTOP_INSTALL_ARGS = --desktop-service $(@D)/components/desktop/dshancontrol-desktop-service --media-worker $(@D)/components/media/dshancontrol-media-worker --imageformats $(@D)/components/imageformats/libdshanimageformats.so
define DSHANCONTROL_DESKTOP_CONFIGURE
	cd $(@D)/components/desktop && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake desktop.pro
	cd $(@D)/components/media && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake media.pro CONFIG+=cedar
	cd $(@D)/components/imageformats && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake imageformats.pro
endef
define DSHANCONTROL_DESKTOP_BUILD
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/desktop
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/media
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/imageformats
endef
endif

# Fixed nonlogin identities. No home directory is created; no root groups.
define DSHANCONTROL_USERS
	dshan-web 400 dshan-app 400 * - - - DshanControl_web
	dshan-hmi 401 dshan-app 400 * - - - DshanControl_HMI
	$(DSHANCONTROL_DESKTOP_USER)
endef

# Buildroot 201902 has no qmake-package infrastructure. Keep T153's recipe intact.
define DSHANCONTROL_CONFIGURE_CMDS
	cd $(@D)/components/hmi/src && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake omnigate-hmi.pro
	cd $(@D)/components/runtime && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake runtime.pro
	$(DSHANCONTROL_DESKTOP_CONFIGURE)
endef

define DSHANCONTROL_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/hmi/src
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/runtime
	$(DSHANCONTROL_DESKTOP_BUILD)
endef

define DSHANCONTROL_INSTALL_TARGET_CMDS
	$(HOST_DIR)/bin/python3 $(@D)/scripts/install_s4_runtime.py \
		--source $(@D) --target $(TARGET_DIR) --hmi $(@D)/components/hmi/src/omnigate-hmi \
		--runtime $(@D)/components/runtime/dshancontrol-runtime --readelf "$(TARGET_READELF)" $(DSHANCONTROL_DESKTOP_INSTALL_ARGS)
endef

$(eval $(generic-package))
