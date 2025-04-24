#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>

class MainWindow : public QObject
{
    Q_OBJECT

public:
    explicit MainWindow(QObject *parent = nullptr);
    ~MainWindow();

    // Funkcje wywoływane z QML do interakcji z aplikacją
    Q_INVOKABLE void searchStations(const QString& searchText); // Wyszukiwanie stacji po tekście
    Q_INVOKABLE void showAllStations(); // Wyświetlenie wszystkich stacji
    Q_INVOKABLE void stationSelected(int stationId); // Wybór konkretnej stacji
    Q_INVOKABLE void sensorSelected(int sensorId); // Wybór konkretnego sensora
    Q_INVOKABLE void saveDataToFile(int sensorId, const QString& format); // Zapis danych do pliku
    Q_INVOKABLE void loadHistoricalData(int sensorId, const QString& dateKey); // Ładowanie historycznych danych
    Q_INVOKABLE QStringList getAvailableHistoricalData(int sensorId); // Pobieranie listy dostępnych danych historycznych

signals:
    // Sygnały wysyłane do QML w celu aktualizacji interfejsu
    void stationsUpdateRequested(const QVariantList& stations); // Aktualizacja listy stacji
    void stationInfoUpdateRequested(int stationId, const QString& stationName, const QString& addressStreet, const QString& city, const QString& lat, const QString& lon); // Aktualizacja informacji o stacji
    void sensorsUpdateRequested(const QVariantList& sensors); // Aktualizacja listy sensorów
    void measurementsUpdateRequested(const QString& key, const QVariantList& values); // Aktualizacja pomiarów
    void airQualityUpdateRequested(const QString& text, const QString& color); // Aktualizacja informacji o jakości powietrza
    void historicalDataListUpdated(const QStringList& dataList); // Aktualizacja listy danych historycznych

private slots:
    // Sloty obsługujące odpowiedzi z API
    void onStationsReceived(); // Odbiór danych o stacjach
    void onSensorsReceived(); // Odbiór danych o sensorach
    void onMeasurementsReceived(); // Odbiór danych pomiarowych
    void onAirQualityIndexReceived(); // Odbiór danych o indeksie jakości powietrza

private:
    QNetworkAccessManager* networkManager; // Manager do obsługi zapytań sieciowych

    // Adresy API
    const QString API_BASE_URL = "https://api.gios.gov.pl/pjp-api/rest/"; // Bazowy adres API
    const QString API_STATIONS_ENDPOINT = "station/findAll"; // Endpoint dla listy stacji
    const QString API_SENSORS_ENDPOINT = "station/sensors/"; // Endpoint dla sensorów
    const QString API_MEASUREMENTS_ENDPOINT = "data/getData/"; // Endpoint dla danych pomiarowych
    const QString API_AIR_QUALITY_ENDPOINT = "aqindex/getIndex/"; // Endpoint dla indeksu jakości powietrza

    // Struktury danych
    QJsonArray allStations; // Tablica wszystkich stacji
    QMap<int, QJsonObject> stationsMap; // Mapa stacji z ID jako kluczem
    QMap<int, QJsonObject> sensorsMap; // Mapa sensorów z ID jako kluczem
    QJsonObject currentMeasurements; // Obiekt przechowujący aktualne pomiary

    // Funkcje do obsługi zapisu i odczytu danych
    QString getDataDirectory(); // Pobieranie katalogu na dane
    QString generateFilename(int sensorId, const QString& format, const QDateTime& dateTime = QDateTime::currentDateTime()); // Generowanie nazwy pliku
    bool saveToJson(const QString& filename, const QJsonObject& data); // Zapis do pliku JSON
    bool saveToXml(const QString& filename, const QJsonObject& data); // Zapis do pliku XML
    QJsonObject loadFromJson(const QString& filename); // Odczyt z pliku JSON
    QJsonObject loadFromXml(const QString& filename); // Odczyt z pliku XML

    // Funkcje do pobierania danych z API
    void fetchStations(); // Pobieranie listy stacji
    void fetchSensors(int stationId); // Pobieranie sensorów dla stacji
    void fetchMeasurements(int sensorId); // Pobieranie pomiarów dla sensora
    void fetchAirQualityIndex(int stationId); // Pobieranie indeksu jakości powietrza
    void displayStations(const QJsonArray& stations); // Wyświetlanie stacji w interfejsie
    QString generateStationInfo(const QJsonObject& station); // Generowanie informacji o stacji

    // Ustawienia pamięci podręcznej
    const int CACHE_VALIDITY_HOURS = 24; // Ważność pamięci podręcznej w godzinach
    const QString CACHE_FILENAME = "air_quality_cache.json"; // Nazwa pliku pamięci podręcznej

    // Funkcje do obsługi pamięci podręcznej
    QString getCachePath(); // Pobieranie ścieżki do pamięci podręcznej
    bool saveToCacheFile(const QJsonObject& cacheData); // Zapis do pliku pamięci podręcznej
    QJsonObject loadFromCacheFile(); // Odczyt z pliku pamięci podręcznej
    bool isCacheValid(int sensorId); // Sprawdzanie ważności pamięci podręcznej
    void updateCache(int sensorId, const QJsonObject& data); // Aktualizacja pamięci podręcznej
    QJsonObject getFromCache(int sensorId); // Pobieranie danych z pamięci podręcznej
    void cleanupOldCache(); // Usuwanie starych danych z pamięci podręcznej
    void processAndDisplayMeasurements(const QJsonObject& measurements); // Przetwarzanie i wyświetlanie pomiarów
};

#endif // MAINWINDOW_H
