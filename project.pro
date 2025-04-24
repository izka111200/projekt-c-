QT += quick qml network charts

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

RESOURCES += qml.qrc

# Quick compiler for better QML performance
CONFIG += qtquickcompiler

# Włącz debugowanie QML, aby lepiej zidentyfikować problem
CONFIG += qml_debug

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

