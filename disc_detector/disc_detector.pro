QT += core gui widgets
CONFIG += c++17
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
TARGET = disc_detector
TEMPLATE = app

SOURCES += main.cpp            mainwindow.cpp

HEADERS += mainwindow.h
FORMS   += mainwindow.ui

INCLUDEPATH += /usr/include/opencv4
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio
