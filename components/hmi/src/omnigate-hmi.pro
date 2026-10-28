QT += core gui widgets network
CONFIG += c++11 release
TARGET = omnigate-hmi
TEMPLATE = app
SOURCES += main.cpp mainwindow.cpp updatepanel.cpp
HEADERS += mainwindow.h updatepanel.h
target.path = /usr/bin
INSTALLS += target

RESOURCES += icons.qrc

SOURCES += visuals.cpp
HEADERS += visuals.h

SOURCES += applicationheartbeat.cpp
HEADERS += applicationheartbeat.h
SOURCES += desktopclient.cpp cloudpanel.cpp familypanel.cpp mediasession.cpp mediapanel.cpp
HEADERS += desktopclient.h cloudpanel.h familypanel.h mediasession.h mediapanel.h

SOURCES += desktop/desktopcanvas.cpp desktop/desktopdrawing.cpp desktop/desktoppages.cpp \
           desktop/desktopmodel.cpp desktop/desktopshell.cpp desktop/desktopfixtures.cpp
HEADERS += desktop/desktopcanvas.h desktop/desktopshell.h
SOURCES += displayprobe.cpp
HEADERS += displayprobe.h
RESOURCES += desktop/desktop.qrc

HEADERS += oneshotupload.h

HEADERS += desktop/desktoplayout.h
SOURCES += desktop/desktoplayout.cpp

SOURCES += desktop/desktopediting.cpp

HEADERS += desktop/desktopimages.h
SOURCES += desktop/desktopimages.cpp

HEADERS += desktop/desktoppoints.h
SOURCES += desktop/desktoppoints.cpp

HEADERS += desktop/desktopkeyboard.h
SOURCES += desktop/desktopkeyboard.cpp
include(../third_party/pinyin/pinyin.pri)

HEADERS += desktop/desktoptrendpanel.h

HEADERS += desktop/applicationregistry.h

HEADERS += applicationshutdown.h

HEADERS += touchfiledialog.h videofilepicker.h
