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
