PYTHON ?= python3
TEST_PYTHON ?= .test-venv/bin/python
RELEASE_DIR ?= dist

.PHONY: test-bootstrap test package

test-bootstrap:
	$(PYTHON) -m venv .test-venv
	.test-venv/bin/pip install --disable-pip-version-check -r tests/requirements.txt

test:
	TEST_PYTHON="$(TEST_PYTHON)" ./scripts/test_all.sh

package:
	./scripts/package_release.sh "$(RELEASE_DIR)"
