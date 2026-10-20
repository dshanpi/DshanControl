PYTHON ?= python3
TEST_PYTHON ?= .test-venv/bin/python
RELEASE_DIR ?= dist

.PHONY: test-bootstrap test package

test-bootstrap:
	$(PYTHON) -m venv .test-venv
	.test-venv/bin/pip install --disable-pip-version-check -r tests/requirements.txt

test: build-native build-native-fixtures build-desktop
	TEST_PYTHON="$(TEST_PYTHON)" ./scripts/test_all.sh

package:
	./scripts/package_release.sh "$(RELEASE_DIR)"

HMI_QMAKE ?= qmake
.PHONY: build-native test-native
build-native:
	mkdir -p dist/native-runtime
	cd dist/native-runtime && $(HMI_QMAKE) ../../components/runtime/runtime.pro && $(MAKE)

test-native: build-native build-native-fixtures
	$(TEST_PYTHON) -m unittest discover -s tests -p 'test_native_*.py' -v

.PHONY: build-native-fixtures
build-native-fixtures:
	mkdir -p dist/native-storage-tests
	cd dist/native-storage-tests && $(HMI_QMAKE) ../../tests/native/storage-test.pro && $(MAKE)
	mkdir -p dist/native-credentials-tests
	cd dist/native-credentials-tests && $(HMI_QMAKE) ../../tests/native/credentials-test.pro && $(MAKE)
	mkdir -p dist/native-launch-tests
	$(CC) -std=c99 -Wall -Wextra -Werror tests/native/launch_process_fixture.c -o dist/native-launch-tests/launch-process-fixture
	mkdir -p dist/native-development-release-tests
	cd dist/native-development-release-tests && $(HMI_QMAKE) ../../tests/native/development-release-test.pro && $(MAKE)
.PHONY: test-hmi
test-hmi:
	mkdir -p dist/ui-test
	cd dist/ui-test && $(HMI_QMAKE) ../../tests/hmi/hmi-ui.pro && $(MAKE)
	cd dist/ui-test && QT_QPA_PLATFORM=offscreen ./hmi-ui-test
	cd dist/ui-test && $(HMI_QMAKE) ../../tests/hmi/desktop-fixture-isolation.pro && $(MAKE)
	cd dist/ui-test && QT_QPA_PLATFORM=offscreen ./desktop-fixture-isolation-test
	cd dist/ui-test && $(HMI_QMAKE) ../../tests/hmi/cloud-reporting-boundaries.pro && $(MAKE)
	cd dist/ui-test && QT_QPA_PLATFORM=offscreen ./cloud-reporting-boundaries-test

.PHONY: build-desktop test-desktop
build-desktop:
	mkdir -p dist/desktop-native dist/desktop-model-tests dist/media-native
	cd dist/desktop-native && $(HMI_QMAKE) ../../components/desktop/desktop.pro && $(MAKE)
	cd dist/desktop-model-tests && $(HMI_QMAKE) ../../tests/desktop/desktop-model.pro && $(MAKE)
	cd dist/media-native && $(HMI_QMAKE) ../../components/media/media.pro && $(MAKE)

test-desktop: build-desktop
	$(TEST_PYTHON) -m unittest discover -s tests -p 'test_desktop_service.py' -v
