DSHANCONTROL_IMX6ULL_VERSION = development
DSHANCONTROL_IMX6ULL_SITE = $(BR2_EXTERNAL_DSHANCONTROL_IMX6ULL_PATH)/app-source
DSHANCONTROL_IMX6ULL_SITE_METHOD = local
DSHANCONTROL_IMX6ULL_LICENSE = GPL-3.0-only, OFL-1.1, Apache-2.0
DSHANCONTROL_IMX6ULL_LICENSE_FILES = LICENSE components/hmi/src/fonts/OFL.txt components/hmi/third_party/pinyin/NOTICE
DSHANCONTROL_IMX6ULL_DEPENDENCIES = qt5base dejavu openssl zlib sqlite ca-certificates \
	iproute2 can-utils omnigate-ethercat alsa-utils ffmpeg

define DSHANCONTROL_IMX6ULL_USERS
	dshan-web 400 dshan-app 400 * - - - DshanControl_local_API
	dshan-hmi 401 dshan-app 400 * - - video,input,audio DshanControl_HMI
	dshan-desktop 402 dshan-app 400 * - - - DshanControl_desktop
endef

define DSHANCONTROL_IMX6ULL_CONFIGURE_CMDS
	cd $(@D)/components/hmi/src && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake omnigate-hmi.pro
	cd $(@D)/components/desktop && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake desktop.pro
	cd $(@D)/components/media && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake media.pro CONFIG+=software
	cd $(@D)/components/runtime && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake runtime.pro
	cd $(@D)/components/api && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake api.pro
	cd $(@D)/components/fieldbus && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake fieldbus.pro
	cd $(@D)/components/lifecycle && $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/qmake lifecycle.pro
endef

define DSHANCONTROL_IMX6ULL_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/hmi/src
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/desktop
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/media
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/runtime
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/api
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/fieldbus
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D)/components/lifecycle
endef

define DSHANCONTROL_IMX6ULL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/components/hmi/src/omnigate-hmi $(TARGET_DIR)/usr/bin/omnigate-hmi
	$(INSTALL) -D -m 0755 $(@D)/components/desktop/dshancontrol-desktop-service $(TARGET_DIR)/usr/bin/dshancontrol-desktop-service
	$(INSTALL) -D -m 0755 $(@D)/components/media/dshancontrol-media-worker $(TARGET_DIR)/usr/bin/dshancontrol-media-worker
	$(INSTALL) -D -m 0755 $(@D)/components/runtime/dshancontrol-runtime $(TARGET_DIR)/usr/bin/dshancontrol-runtime
	$(INSTALL) -D -m 0644 $(@D)/boards/imx6ull-pro/platform.json $(TARGET_DIR)/usr/share/dshancontrol/platform.json
	$(INSTALL) -D -m 0755 $(@D)/components/api/dshancontrol-api $(TARGET_DIR)/usr/bin/dshancontrol-api
	$(INSTALL) -D -m 0755 $(@D)/components/fieldbus/dshancontrol-fieldbus-worker $(TARGET_DIR)/usr/bin/dshancontrol-fieldbus-worker
	$(INSTALL) -D -m 0755 $(@D)/components/lifecycle/dshan-imx6ull-services $(TARGET_DIR)/usr/libexec/dshan-imx6ull-services
	mkdir -p $(TARGET_DIR)/usr/share/dshancontrol/web
	cp -a $(@D)/components/web/assets/. $(TARGET_DIR)/usr/share/dshancontrol/web/
	$(INSTALL) -D -m 0644 $(@D)/LICENSE $(TARGET_DIR)/usr/share/licenses/dshancontrol/LICENSE
	$(INSTALL) -m 0644 $(@D)/components/hmi/src/fonts/OFL.txt $(TARGET_DIR)/usr/share/licenses/dshancontrol/OFL.txt
	$(INSTALL) -m 0644 $(@D)/components/hmi/third_party/pinyin/NOTICE $(TARGET_DIR)/usr/share/licenses/dshancontrol/PINYIN-NOTICE
endef

define DSHANCONTROL_IMX6ULL_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 $(@D)/boards/imx6ull-pro/runtime/S35dshan-data $(TARGET_DIR)/etc/init.d/S35dshan-data
	$(INSTALL) -D -m 0755 $(@D)/boards/imx6ull-pro/runtime/S41wifi $(TARGET_DIR)/etc/init.d/S41wifi
	$(INSTALL) -D -m 0755 $(@D)/boards/imx6ull-pro/runtime/S99dshancontrol $(TARGET_DIR)/etc/init.d/S99dshancontrol
	$(INSTALL) -D -m 0755 $(@D)/boards/imx6ull-pro/runtime/S98dshan-runtime $(TARGET_DIR)/etc/init.d/S98dshan-runtime
	$(INSTALL) -D -m 0755 $(@D)/boards/imx6ull-pro/runtime/S97dshan-audio $(TARGET_DIR)/etc/init.d/S97dshan-audio
endef

$(eval $(generic-package))
