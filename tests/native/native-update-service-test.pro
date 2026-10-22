# Test-only TLS/proxy main around the unchanged real RuntimeService sources.
include(../../components/runtime/runtime.pro)
SOURCES -= src/main.cpp
runtime_sources =
for(source, SOURCES): runtime_sources += $$PWD/../../components/runtime/$$source
runtime_headers =
for(header, HEADERS): runtime_headers += $$PWD/../../components/runtime/$$header
SOURCES = $$runtime_sources native_update_service_test_driver.cpp
HEADERS = $$runtime_headers
TARGET = native-update-service-test-driver
