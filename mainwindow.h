#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/**
 * @file mainwindow.h
 * @brief Plik nagłówkowy dla klasy MainWindow, zarządzającej danymi i komunikacją z API w aplikacji Qt.
 */

/**
 * @class MainWindow
 * @brief Główna klasa aplikacji, odpowiedzialna za zarządzanie danymi, komunikację z API oraz interakcję z QML.
 */
#include <QObject>
#include <QNetworkAccessManager> ///< Do wysyłania żądań sieciowych.
#include <QNetworkReply>        ///< Do obsługi odpowiedzi sieciowych.
#include <QJsonDocument>       ///< Do pracy z danymi JSON.
#include <QJsonArray>          ///< Do przechowywania tablic JSON.
#include <QJsonObject>         ///< Do przechowywania obiektów JSON.
#include <QVariantList>        ///< Do przekazywania listy danych do QML.
#include <QVariantMap>         ///< Do przekazywania słownika danych do QML.
#include <QFile>               ///< Do operacji na plikach.
#include <QDir>                ///< Do pracy z katalogami.
#include <QStandardPaths>      ///< Do znajdowania standardowych ścieżek.
#include <QDateTime>           ///< Do obsługi dat i czasu.
#include <QTimer>              ///< Do zadań cyklicznych, np. autosave.

class MainWindow : public QObject
{
    Q_OBJECT ///< Umożliwia korzystanie z sygnałów i slotów Qt.

public:
    /// Konstruktor klasy, inicjalizuje obiekt z opcjonalnym rodzicem.
    explicit MainWindow(QObject *parent = nullptr);
    /// Destruktor, zwalnia zasoby obiektu.
    ~MainWindow();

    /// Wyszukuje stacje pomiarowe na podstawie tekstu (np. nazwa miasta).
    Q_INVOKABLE void searchStations(const QString& searchText);
    /// Wyświetla wszystkie dostępne stacje pomiarowe.
    Q_INVOKABLE void showAllStations();
    /// Obsługuje wybór stacji na podstawie jej ID.
    Q_INVOKABLE void stationSelected(int stationId);
    /// Obsługuje wybór czujnika na podstawie jego ID.
    Q_INVOKABLE void sensorSelected(int sensorId);
    /// Ładuje historyczne dane dla czujnika i klucza daty.
    Q_INVOKABLE void loadHistoricalData(int sensorId, const QString& dateKey);
    /// Zwraca listę dostępnych historycznych danych dla czujnika.
    Q_INVOKABLE QStringList getAvailableHistoricalData(int sensorId);
    /// Oblicza statystyki dla danych z czujnika.
    Q_INVOKABLE QVariantMap computeStatistics(int sensorId);
    /// Importuje dane z pliku w podanym formacie (np. JSON, XML).
    Q_INVOKABLE bool importDataFromFile(const QString& path, const QString& format);
    /// Usuwa historyczne dane dla podanego klucza daty.
    Q_INVOKABLE bool deleteHistoricalData(const QString& dateKey);
    /// Ponawia połączenie z API w razie problemów sieciowych.
    Q_INVOKABLE void retryConnection();

signals:
    /// Informuje QML o aktualizacji listy stacji.
    void stationsUpdateRequested(const QVariantList& stations);
    /// Przekazuje dane wybranej stacji (ID, nazwa, adres, miasto, współrzędne).
    void stationInfoUpdateRequested(int stationId, const QString& stationName, const QString& addressStreet, const QString& city, const QString& lat, const QString& lon);
    /// Informuje o aktualizacji listy czujników.
    void sensorsUpdateRequested(const QVariantList& sensors);
    /// Przekazuje nowe pomiary dla czujnika.
    void measurementsUpdateRequested(const QString& key, const QVariantList& values);
    /// Aktualizuje informacje o jakości powietrza (tekst i kolor).
    void airQualityUpdateRequested(const QString& text, const QString& color);
    /// Przekazuje listę dostępnych historycznych danych.
    void historicalDataListUpdated(const QStringList& dataList);
    /// Przekazuje obliczone statystyki.
    void statisticsUpdated(const QVariantMap& stats);
    /// Informuje o statusie automatycznego zapisu.
    void autoSaveStatus(const QString& message, bool success);
    /// Przekazuje ścieżkę zapisu danych.
    void dataPathInfo(const QString& path);

private slots:
    /// Obsługuje odpowiedź API z listą stacji.
    void onStationsReceived();
    /// Obsługuje odpowiedź API z listą czujników.
    void onSensorsReceived();
    /// Obsługuje odpowiedź API z pomiarami.
    void onMeasurementsReceived();
    /// Obsługuje odpowiedź API z indeksem jakości powietrza.
    void onAirQualityIndexReceived();
    /// Automatycznie zapisuje pomiary w tle.
    void autoSaveMeasurements();

private:
    /// Manager do żądań sieciowych.
    QNetworkAccessManager* networkManager;
    /// Timer do cyklicznego zapisu danych.
    QTimer* autoSaveTimer;

    /// Bazowy adres API GIOS.
    const QString API_BASE_URL = "https://api.gios.gov.pl/pjp-api/rest/";
    /// Endpoint API dla listy stacji.
    const QString API_STATIONS_ENDPOINT = "station/findAll";
    /// Endpoint API dla czujników stacji.
    const QString API_SENSORS_ENDPOINT = "station/sensors/";
    /// Endpoint API dla pomiarów czujnika.
    const QString API_MEASUREMENTS_ENDPOINT = "data/getData/";
    /// Endpoint API dla indeksu jakości powietrza.
    const QString API_AIR_QUALITY_ENDPOINT = "aqindex/getIndex/";

    /// Lista wszystkich stacji (JSON).
    QJsonArray allStations;
    /// Mapa ID stacji na ich dane.
    QMap<int, QJsonObject> stationsMap;
    /// Mapa ID czujników na ich dane.
    QMap<int, QJsonObject> sensorsMap;
    /// Aktualne pomiary dla wybranego czujnika.
    QJsonObject currentMeasurements;
    /// ID aktualnie wybranego czujnika.
    int currentSensorId;

    /// Zwraca katalog zapisu danych.
    QString getDataDirectory();
    /// Zapisuje dane pomiarowe do pliku historii.
    bool saveToHistoryFile(int sensorId, const QJsonObject& data, const QString& dateKey);
    /// Ładuje dane z pliku JSON.
    QJsonObject loadFromJson(const QString& filename);
    /// Ładuje dane z pliku XML.
    QJsonObject loadFromXml(const QString& filename);

    /// Pobiera listę stacji z API.
    void fetchStations();
    /// Pobiera czujniki dla stacji z API.
    void fetchSensors(int stationId);
    /// Pobiera pomiary dla czujnika z API.
    void fetchMeasurements(int sensorId);
    /// Pobiera indeks jakości powietrza z API.
    void fetchAirQualityIndex(int stationId);
    /// Wyświetla stacje w interfejsie QML.
    void displayStations(const QJsonArray& stations);
    /// Generuje informacje o stacji.
    QString generateStationInfo(const QJsonObject& station);

    /// Czas ważności cache (godziny).
    const int CACHE_VALIDITY_HOURS = 24;
    /// Nazwa pliku cache.
    const QString CACHE_FILENAME = "air_quality_cache.json";

    /// Zwraca ścieżkę do pliku cache.
    QString getCachePath();
    /// Zapisuje dane do cache.
    bool saveToCacheFile(const QJsonObject& cacheData);
    /// Ładuje dane z cache.
    QJsonObject loadFromCacheFile();
    /// Sprawdza ważność cache dla czujnika.
    bool isCacheValid(int sensorId);
    /// Aktualizuje cache dla czujnika.
    void updateCache(int sensorId, const QJsonObject& data);
    /// Pobiera dane z cache dla czujnika.
    QJsonObject getFromCache(int sensorId);
    /// Usuwa stare dane z cache.
    void cleanupOldCache();
    /// Przetwarza i wyświetla pomiary w interfejsie.
    void processAndDisplayMeasurements(const QJsonObject& measurements);
};

#endif // MAINWINDOW_H
