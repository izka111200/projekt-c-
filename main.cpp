#include <QApplication> // Biblioteka do tworzenia aplikacji Qt
#include <QQmlApplicationEngine> // Biblioteka do obsługi QML
#include <QQmlContext> // Biblioteka do przekazywania danych do QML
#include "mainwindow.h" // Plik nagłówkowy dla klasy MainWindow

int main(int argc, char *argv[])
{
    // Tworzenie głównego obiektu aplikacji Qt
    QApplication app(argc, argv);

    // Ustawianie nazwy organizacji
    app.setOrganizationName("JPOGIOS");
    // Ustawianie domeny organizacji
    app.setOrganizationDomain("jpo.example.com");
    // Ustawianie nazwy aplikacji
    app.setApplicationName("MonitorJakosciPowietrza");

    // Tworzenie silnika do obsługi QML
    QQmlApplicationEngine engine;
    // Tworzenie obiektu głównego okna
    MainWindow mainWindow;

    // Przekazywanie obiektu mainWindow do QML, aby umożliwić jego używanie
    engine.rootContext()->setContextProperty("mainWindow", &mainWindow);

    // Określanie adresu pliku QML do załadowania
    const QUrl url(QStringLiteral("qrc:/main.qml"));

    // Sprawdzanie, czy QML został poprawnie załadowany; w razie błędu kończenie programu
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    // Ładowanie pliku QML
    engine.load(url);

    // Sprawdzanie, czy QML utworzył obiekty; w razie braku kończenie programu
    if (engine.rootObjects().isEmpty())
        return -1;

    // Uruchamianie aplikacji i oczekiwanie na interakcje użytkownika
    return app.exec();
}
