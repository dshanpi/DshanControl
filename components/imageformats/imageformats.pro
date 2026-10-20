# Application-local codecs for the pinned S4 Qt build, not a global Qt replacement.
QT += core gui core-private gui-private
!equals(QT_VERSION, 5.11.3): error(This codec bundle requires the audited Qt 5.11.3 S4 sysroot)
TEMPLATE = lib
CONFIG += plugin c++11 release
TARGET = dshanimageformats
INCLUDEPATH += $$PWD/third_party/qt-5.11.3
SOURCES += plugin.cpp png-source.cpp third_party/qt-5.11.3/qjpeghandler.cpp
HEADERS += codec-config.h third_party/qt-5.11.3/qjpeghandler_p.h third_party/qt-5.11.3/private/qpnghandler_p.h
LIBS += -lpng -ljpeg
