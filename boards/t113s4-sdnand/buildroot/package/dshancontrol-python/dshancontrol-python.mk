DSHANCONTROL_PYTHON_VERSION = 1
DSHANCONTROL_PYTHON_SOURCE = Flask-2.1.2-py3-none-any.whl
DSHANCONTROL_PYTHON_SITE = https://files.pythonhosted.org/packages/ba/76/e9580e494eaf6f09710b0f3b9000c9c0363e44af5390be32bb0394165853
DSHANCONTROL_PYTHON_EXTRA_DOWNLOADS = https://files.pythonhosted.org/packages/c4/44/f50f2d22cdfb6d56c03d1b4cc3cfa03ebee2f21b59a7768f151e43415ba5/Werkzeug-2.1.2-py3-none-any.whl \
	https://files.pythonhosted.org/packages/20/9a/e5d9ec41927401e41aea8af6d16e78b5e612bca4699d417f646a9610a076/Jinja2-3.0.3-py3-none-any.whl \
	https://files.pythonhosted.org/packages/68/5f/447e04e828f47465eeab35b5d408b7ebaaaee207f48b7136c5a7267a30ae/itsdangerous-2.1.2-py3-none-any.whl \
	https://files.pythonhosted.org/packages/4a/a8/0b2ced25639fb20cc1c9784de90a8c25f9504a7f18cd8b5397bd61696d7d/click-8.0.4-py3-none-any.whl \
	https://files.pythonhosted.org/packages/a0/a1/b153a0a4caf7a7e3f15c2cd56c7702e2cf3d89b1b359d1f1c5e59d68f4ce/importlib_metadata-4.8.3-py3-none-any.whl \
	https://files.pythonhosted.org/packages/bd/df/d4a4974a3e3957fd1c1fa3082366d7fff6e428ddb55f074bf64876f8e8ad/zipp-3.6.0-py3-none-any.whl \
	https://files.pythonhosted.org/packages/45/6b/44f7f8f1e110027cf88956b59f2fad776cca7e1704396d043f89effd3a0e/typing_extensions-4.1.1-py3-none-any.whl \
	https://files.pythonhosted.org/packages/bf/10/ff66fea6d1788c458663a84d88787bae15d45daa16f6b3ef33322a51fc7e/MarkupSafe-2.0.1.tar.gz
DSHANCONTROL_PYTHON_LICENSE = BSD-3-Clause, MIT, PSF-2.0
DSHANCONTROL_PYTHON_DEPENDENCIES = host-python3 python3

define DSHANCONTROL_PYTHON_EXTRACT_CMDS
	mkdir -p $(@D)
endef

define DSHANCONTROL_PYTHON_INSTALL_TARGET_CMDS
	$(HOST_DIR)/bin/python3 $(BR2_EXTERNAL_DSHANCONTROL_PATH)/app-source/scripts/install_python_runtime.py \
		$(BR2_EXTERNAL_DSHANCONTROL_PATH)/app-source/boards/t113s4-sdnand/python-runtime.lock.json \
		$(DSHANCONTROL_PYTHON_DL_DIR) $(TARGET_DIR)/usr/lib/dshancontrol-python
endef

$(eval $(generic-package))
