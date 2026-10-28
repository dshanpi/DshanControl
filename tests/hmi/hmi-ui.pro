QT += core gui widgets network testlib
CONFIG += c++11 testcase console
TARGET = hmi-ui-test
TEMPLATE = app
SOURCES += hmi_ui_test.cpp ../../components/hmi/src/mainwindow.cpp ../../components/hmi/src/updatepanel.cpp
HEADERS += ../../components/hmi/src/mainwindow.h ../../components/hmi/src/updatepanel.h

RESOURCES += ../../components/hmi/src/icons.qrc

SOURCES += ../../components/hmi/src/visuals.cpp
HEADERS += ../../components/hmi/src/visuals.h

SOURCES += ../../components/hmi/src/applicationheartbeat.cpp
HEADERS += ../../components/hmi/src/applicationheartbeat.h
SOURCES += ../../components/hmi/src/desktopclient.cpp ../../components/hmi/src/cloudpanel.cpp \
           ../../components/hmi/src/familypanel.cpp ../../components/hmi/src/mediasession.cpp ../../components/hmi/src/mediapanel.cpp
HEADERS += ../../components/hmi/src/desktopclient.h ../../components/hmi/src/cloudpanel.h \
           ../../components/hmi/src/familypanel.h ../../components/hmi/src/mediasession.h ../../components/hmi/src/mediapanel.h

SOURCES += ../../components/hmi/src/desktop/desktopcanvas.cpp \
           ../../components/hmi/src/desktop/desktopdrawing.cpp \
           ../../components/hmi/src/desktop/desktoppages.cpp \
           ../../components/hmi/src/desktop/desktopmodel.cpp
HEADERS += ../../components/hmi/src/desktop/desktopcanvas.h
SOURCES += ../../components/hmi/src/displayprobe.cpp
HEADERS += ../../components/hmi/src/displayprobe.h
RESOURCES += ../../components/hmi/src/desktop/desktop.qrc

HEADERS += ../../components/hmi/src/oneshotupload.h

HEADERS += ../../components/hmi/src/desktop/desktoplayout.h
SOURCES += ../../components/hmi/src/desktop/desktoplayout.cpp

SOURCES += ../../components/hmi/src/desktop/desktopediting.cpp

HEADERS += ../../components/hmi/src/desktop/desktopimages.h
SOURCES += ../../components/hmi/src/desktop/desktopimages.cpp

HEADERS += ../../components/hmi/src/desktop/desktoppoints.h
SOURCES += ../../components/hmi/src/desktop/desktoppoints.cpp

HEADERS += ../../components/hmi/src/desktop/desktopkeyboard.h
SOURCES += ../../components/hmi/src/desktop/desktopkeyboard.cpp
include(../../components/hmi/third_party/pinyin/pinyin.pri)

HEADERS += ../../components/hmi/src/desktop/desktoptrendpanel.h

SOURCES += ../../components/hmi/src/desktop/desktopshell.cpp
HEADERS += ../../components/hmi/src/desktop/desktopshell.h

# This override is confined to the test executable, never the production HMI.
DEFINES += DSHANCONTROL_HMI_UI_TEST

HEADERS += ../../components/hmi/src/desktop/applicationregistry.h

HEADERS += ../../components/hmi/src/touchfiledialog.h ../../components/hmi/src/videofilepicker.h
