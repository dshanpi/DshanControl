QT += core gui widgets network
CONFIG += c++11 release
TEMPLATE = app
TARGET = cloud-home-preview
SOURCES += cloud_home_preview.cpp ../../components/hmi/src/visuals.cpp
HEADERS += ../../components/hmi/src/visuals.h
RESOURCES += ../../components/hmi/src/icons.qrc
