#include <QApplication>        ///< Biblioteka do tworzenia aplikacji Qt.
#include <QQmlApplicationEngine> ///< Biblioteka do obsługi silnika QML.
#include <QQmlContext>         ///< Biblioteka do przekazywania danych do QML.
#include "mainwindow.h"        ///< Plik nagłówkowy dla klasy MainWindow.

/**
 * @file main.cpp
 * @brief Główny plik programu, inicjalizujący aplikację Qt i ładujący interfejs QML.
 */

/**
 * @brief Funkcja główna programu.
 * @param argc Liczba argumentów linii poleceń.
 * @param argv Tablica argumentów linii poleceń.
 * @return Kod wyjścia programu (0 oznacza sukces).
 */
int main(int argc, char *argv[])
{
    /// Tworzy obiekt aplikacji Qt z argumentami linii poleceń.
    QApplication app(argc, argv);

    /// Ustawia nazwę organizacji dla aplikacji.
    app.setOrganizationName("JPOGIOS");
    /// Ustawia domenę organizacji dla ustawień aplikacji.
    app.setOrganizationDomain("jpo.example.com");
    /// Ustawia nazwę aplikacji.
    app.setApplicationName("MonitorJakosciPowietrza");

    /// Tworzy silnik do obsługi plików QML.
    QQmlApplicationEngine engine;

    /// Tworzy obiekt MainWindow do zarządzania logiką aplikacji.
    MainWindow mainWindow;

    /// Przekazuje obiekt MainWindow do kontekstu QML jako "mainWindow".
    engine.rootContext()->setContextProperty("mainWindow", &mainWindow);

    /// Definiuje adres głównego pliku QML z zasobów qrc.
    const QUrl url(QStringLiteral("qrc:/main.qml"));

    /// Sprawdza, czy QML został załadowany; jeśli nie, kończy program.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    /// Ładuje główny plik QML.
    engine.load(url);

    /// Kończy program, jeśli QML nie utworzył żadnych obiektów.
    if (engine.rootObjects().isEmpty())
        return -1;

    /// Uruchamia pętlę zdarzeń aplikacji Qt.
    return app.exec();
}
