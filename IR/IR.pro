QT += widgets
CONFIG += c++17
TEMPLATE = app
TARGET = tcrt_counter

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# pigpio daemon client (no root for the app)
LIBS += -lpigpiod_if2 -lpthread -lrt
