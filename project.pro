## @file project.pro
## @brief Konfiguracja projektu Qt dla aplikacji QML z modułami sieciowymi i wykresami.

## @brief Moduły Qt: Quick, QML, Network, Charts.
QT += quick qml network charts

## @brief Standard C++17 dla kompilacji.
CONFIG += c++17

## @brief Pliki źródłowe C++.
SOURCES += \
    main.cpp \
    mainwindow.cpp

## @brief Pliki nagłówkowe.
HEADERS += \
    mainwindow.h

## @brief Zasoby QML projektu.
RESOURCES += qml.qrc

## @brief Włącza kompilator Qt Quick dla lepszej wydajności QML.
CONFIG += qtquickcompiler

## @brief Włącza debugowanie QML do śledzenia błędów.
CONFIG += qml_debug

## @brief Ścieżka instalacji dla QNX.
qnx: target.path = /tmp/$${TARGET}/bin

## @brief Ścieżka instalacji dla Unix (poza Androidem).
else: unix:!android: target.path = /opt/$${TARGET}/bin

## @brief Aktywuje reguły instalacji, jeśli ścieżka jest zdefiniowana.
!isEmpty(target.path): INSTALLS += target
