QT += core gui widgets
CONFIG += c++11 release
TEMPLATE = app
TARGET = video-player-preview
SOURCES += video_player_preview.cpp ../../components/hmi/src/visuals.cpp
HEADERS += ../../components/hmi/src/visuals.h
RESOURCES += ../../components/hmi/src/icons.qrc
