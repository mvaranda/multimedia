QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ../libffwplayer/ffwplayer.c \
    ../libffwplayer/msg_thread.c \
    log.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    ../libffwplayer/ffwplayer.h \
    ../libffwplayer/log.h \
    ../libffwplayer/msg_thread.h \
    mainwindow.h

FORMS += \
    mainwindow.ui

LIBS += -lpthread \
    -lavutil \
    -lavformat \
    -lavcodec \
    -lswscale \
    -lswresample \
    -lz \
    -lm -lSDL2

INCLUDEPATH += ../libffwplayer
INCLUDEPATH += /usr/include/SDL2
DEFINES += _REENTRANT QT_PLATF

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#unix: CONFIG += link_pkgconfig
#unix: PKGCONFIG += pthread

RESOURCES += \
  resource.qrc
