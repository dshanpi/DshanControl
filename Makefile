PYTHON ?= python3
TEST_PYTHON ?= .test-venv/bin/python
RELEASE_DIR ?= dist

.PHONY: test-bootstrap test package

test-bootstrap:
	$(PYTHON) -m venv .test-venv
	.test-venv/bin/pip install --disable-pip-version-check -r tests/requirements.txt

test: build-native build-native-fixtures build-desktop
	TEST_PYTHON="$(TEST_PYTHON)" ./scripts/test_all.sh

package: check-ui-pages
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
test-hmi: check-ui-pages
	mkdir -p dist/ui-test
	cd dist/ui-test && $(HMI_QMAKE) ../../tests/hmi/hmi-ui.pro && $(MAKE)
	cd dist/ui-test && QT_QPA_PLATFORM=offscreen ./hmi-ui-test
	cd dist/ui-test && $(HMI_QMAKE) ../../tests/hmi/desktop-fixture-isolation.pro && $(MAKE)
	cd dist/ui-test && QT_QPA_PLATFORM=offscreen ./desktop-fixture-isolation-test
	cd dist/ui-test && $(HMI_QMAKE) ../../tests/hmi/cloud-reporting-boundaries.pro && $(MAKE)
	cd dist/ui-test && QT_QPA_PLATFORM=offscreen ./cloud-reporting-boundaries-test

.PHONY: test-hmi-native-update-bridge
test-hmi-native-update-bridge: build-native
	mkdir -p dist/native-update-bridge
	cd dist/native-update-bridge && $(HMI_QMAKE) ../../tests/hmi/native-update-bridge.pro && $(MAKE)
	$(TEST_PYTHON) tests/hmi/run_native_update_bridge.py --qt-binary dist/native-update-bridge/native-update-bridge-test --native-binary dist/native-runtime/dshancontrol-runtime --output "dist/native-update-bridge/evidence-$$(date -u +%Y%m%dT%H%M%S)-$$$$"

test-hmi: test-hmi-native-update-bridge

.PHONY: test-hmi-native-update-flow
test-hmi-native-update-flow: build-native
	mkdir -p dist/native-update-service-test dist/native-update-flow-test
	cd dist/native-update-service-test && $(HMI_QMAKE) ../../tests/native/native-update-service-test.pro && $(MAKE)
	cd dist/native-update-flow-test && $(HMI_QMAKE) ../../tests/hmi/native-update-flow.pro && $(MAKE)
	$(TEST_PYTHON) tests/hmi/run_native_update_flow.py --qt dist/native-update-flow-test/native-update-flow-test --cli dist/native-runtime/dshancontrol-runtime --service dist/native-update-service-test/native-update-service-test-driver --output "dist/native-update-flow-test/evidence-$$(date -u +%Y%m%dT%H%M%S)-$$$$"

test-hmi: test-hmi-native-update-flow

.PHONY: build-desktop test-desktop
build-desktop:
	mkdir -p dist/desktop-native dist/desktop-model-tests dist/media-native
	cd dist/desktop-native && $(HMI_QMAKE) ../../components/desktop/desktop.pro && $(MAKE)
	cd dist/desktop-model-tests && $(HMI_QMAKE) ../../tests/desktop/desktop-model.pro && $(MAKE)
	cd dist/media-native && $(HMI_QMAKE) ../../components/media/media.pro && $(MAKE)

test-desktop: build-desktop
	$(TEST_PYTHON) -m unittest discover -s tests -p 'test_desktop_service.py' -v

.PHONY: test-application-registry
test-application-registry:
	mkdir -p dist/application-registry-tests
	cd dist/application-registry-tests && $(HMI_QMAKE) ../../tests/hmi/application-registry-test.pro && $(MAKE)
	cd dist/application-registry-tests && ./application-registry-test

test-hmi: test-application-registry

.PHONY: check-ui-pages
check-ui-pages:
	$(PYTHON) scripts/check_ui_page_gates.py
