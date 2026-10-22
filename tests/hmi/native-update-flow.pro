QT += core gui widgets network testlib
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = native-update-flow-test
SOURCES += native_update_flow_test.cpp \
           ../../components/hmi/src/updatepanel.cpp \
           ../../components/hmi/src/visuals.cpp
HEADERS += ../../components/hmi/src/updatepanel.h ../../components/hmi/src/visuals.h
# This protocol integration has no screenshot/font assertions; do not embed fonts.
