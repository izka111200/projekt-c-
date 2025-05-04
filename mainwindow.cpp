#include "mainwindow.h"
#include <QDebug>              ///< Biblioteka do logowania komunikatów debugowania.
#include <QDateTime>          ///< Biblioteka do obsługi dat i czasu.
#include <QJsonDocument>      ///< Biblioteka do pracy z danymi JSON.
#include <QXmlStreamReader>   ///< Biblioteka do parsowania XML.
#include <QUrl>               ///< Biblioteka do obsługi adresów URL.
#include <QFileInfo>          ///< Biblioteka do informacji o plikach.
#include <cmath>              ///< Biblioteka do operacji matematycznych.

/**
 * @file mainwindow.cpp
 * @brief Implementacja klasy MainWindow, zarządzającej danymi, API i pamięcią podręczną aplikacji Qt.
 */

/**
 * @brief Konstruktor klasy MainWindow.
 * Inicjalizuje managera sieci, timer autozapisu, tworzy katalog danych i pobiera listę stacji.
 * @param parent Opcjonalny rodzic obiektu.
 */
MainWindow::MainWindow(QObject *parent)
    : QObject(parent), currentSensorId(0)
{
    /// Tworzy managera do żądań sieciowych.
    networkManager = new QNetworkAccessManager(this);
    /// Ustawia timer autozapisu na 60 sekund.
    autoSaveTimer = new QTimer(this);
    autoSaveTimer->setInterval(60000);
    connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSaveMeasurements);
    autoSaveTimer->start();
    /// Pobiera listę stacji na starcie.
    fetchStations();

    /// Tworzy katalog danych i raportuje jego status.
    QDir dir;
    QString dataDir = getDataDirectory();
    if (!dir.exists(dataDir)) {
        if (dir.mkpath(dataDir)) {
            qDebug() << "Utworzono katalog danych:" << dataDir;
            emit dataPathInfo("Katalog danych utworzony: " + dataDir);
        } else {
            qDebug() << "Błąd tworzenia katalogu danych:" << dataDir;
            emit dataPathInfo("Błąd tworzenia katalogu: " + dataDir);
        }
    } else {
        qDebug() << "Katalog danych już istnieje:" << dataDir;
        emit dataPathInfo("Katalog danych: " + dataDir);
    }
}

/**
 * @brief Destruktor klasy MainWindow.
 * Zwalnianie zasobów jest automatyczne dzięki hierarchii Qt.
 */
MainWindow::~MainWindow()
{
}

/**
 * @brief Zwraca ścieżkę do katalogu danych aplikacji.
 * @return Ścieżka do katalogu w standardowej lokalizacji AppData.
 */
QString MainWindow::getDataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data";
}

/**
 * @brief Zapisuje dane pomiarowe do pliku historii w formacie JSON.
 * @param sensorId ID czujnika.
 * @param data Dane pomiarowe do zapisania.
 * @param dateKey Klucz daty dla danych.
 * @return True, jeśli zapis się powiódł; false w przeciwnym razie.
 */
bool MainWindow::saveToHistoryFile(int sensorId, const QJsonObject& data, const QString& dateKey)
{
    QString historyPath = getDataDirectory() + "/air_quality_history.json";
    QFile historyFile(historyPath);
    QJsonObject history;

    /// Wczytuje istniejącą historię, jeśli plik istnieje.
    if (historyFile.exists() && historyFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(historyFile.readAll());
        if (!doc.isNull()) {
            history = doc.object();
        } else {
            qDebug() << "Uszkodzony plik historii, tworzenie nowego:" << historyPath;
            emit autoSaveStatus("Uszkodzony plik historii: " + historyPath, false);
        }
        historyFile.close();
    }

    /// Sprawdza możliwość zapisu pliku.
    QFileInfo fileInfo(historyPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
        qDebug() << "Nie można utworzyć katalogu:" << dir.path();
        emit autoSaveStatus("Błąd: Nie można utworzyć katalogu " + dir.path(), false);
        emit dataPathInfo("Błąd: Nie można utworzyć katalogu " + dir.path());
        return false;
    }
    if (!fileInfo.isWritable() && historyFile.exists()) {
        qDebug() << "Brak uprawnień do zapisu pliku:" << historyPath;
        emit autoSaveStatus("Błąd: Brak uprawnień do zapisu pliku " + historyPath, false);
        emit dataPathInfo("Błąd: Brak uprawnień do zapisu pliku " + historyPath);
        return false;
    }

    /// Dodaje dane dla czujnika do historii.
    if (!history.contains(QString::number(sensorId))) {
        history[QString::number(sensorId)] = QJsonObject();
    }
    QJsonObject sensorHistory = history[QString::number(sensorId)].toObject();
    sensorHistory[dateKey] = data;
    history[QString::number(sensorId)] = sensorHistory;

    /// Zapisuje do pliku tymczasowego dla bezpieczeństwa.
    QString tempPath = historyPath + ".tmp";
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        qDebug() << "Błąd otwierania pliku tymczasowego:" << tempFile.errorString();
        emit autoSaveStatus("Błąd zapisu: Brak dostępu do pliku " + tempPath, false);
        emit dataPathInfo("Błąd zapisu: Brak dostępu do pliku " + tempPath);
        return false;
    }
    QJsonDocument doc(history);
    tempFile.write(doc.toJson());
    tempFile.close();

    /// Zastępuje oryginalny plik.
    if (historyFile.exists() && !historyFile.remove()) {
        qDebug() << "Błąd usuwania starego pliku historii:" << historyFile.errorString();
        emit autoSaveStatus("Błąd zapisu: Nie można usunąć starego pliku " + historyPath, false);
        emit dataPathInfo("Błąd zapisu: Nie można usunąć starego pliku " + historyPath);
        return false;
    }
    if (!tempFile.rename(historyPath)) {
        qDebug() << "Błąd zmiany nazwy pliku tymczasowego:" << tempFile.errorString();
        emit autoSaveStatus("Błąd zapisu: Problem z zapisem pliku " + historyPath, false);
        emit dataPathInfo("Błąd zapisu: Problem z zapisem pliku " + historyPath);
        return false;
    }

    qDebug() << "Dane zapisano do pliku historii dla czujnika ID:" << sensorId << "z kluczem daty:" << dateKey;
    emit autoSaveStatus("Dane zapisane automatycznie: " + historyPath, true);
    emit dataPathInfo("Zapisano plik: " + historyPath);
    return true;
}

/**
 * @brief Automatycznie zapisuje pomiary co 60 sekund.
 * Sprawdza, czy dane są dostępne, i zapisuje je do pliku historii.
 */
void MainWindow::autoSaveMeasurements()
{
    /// Sprawdza, czy wybrano czujnik.
    if (currentSensorId == 0) {
        qDebug() << "Autozapis pominięty: Brak wybranego czujnika";
        emit autoSaveStatus("Brak wybranego czujnika", false);
        return;
    }
    /// Sprawdza, czy są dane pomiarowe.
    if (currentMeasurements.isEmpty()) {
        qDebug() << "Autozapis pominięty: Brak danych pomiarowych";
        emit autoSaveStatus("Brak danych do zapisu", false);
        return;
    }
    /// Sprawdza poprawność ID czujnika.
    if (!sensorsMap.contains(currentSensorId)) {
        qDebug() << "Autozapis pominięty: Nieprawidłowy ID czujnika:" << currentSensorId;
        emit autoSaveStatus("Nieprawidłowy czujnik", false);
        return;
    }

    /// Przygotowuje dane do zapisu.
    QJsonObject dataToSave = currentMeasurements;
    dataToSave["sensorInfo"] = sensorsMap[currentSensorId];
    dataToSave["saveDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QString dateKey = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    /// Zapisuje dane do pliku historii.
    bool success = saveToHistoryFile(currentSensorId, dataToSave, dateKey);
    qDebug() << (success ? "Autozapis zakończony powodzeniem" : "Autozapis nieudany") << "dla czujnika ID:" << currentSensorId;
}

/**
 * @brief Pobiera listę stacji z API GIOS.
 */
void MainWindow::fetchStations()
{
    QNetworkRequest request(QUrl(API_BASE_URL + API_STATIONS_ENDPOINT));
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onStationsReceived);
}

/**
 * @brief Obsługuje odpowiedź API z listą stacji.
 * Zapisuje stacje do mapy i wyświetla je w QML.
 */
void MainWindow::onStationsReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray stations = jsonDoc.array();
        allStations = stations;

        /// Zapisuje stacje do mapy.
        stationsMap.clear();
        for (const QJsonValue& value : stations) {
            QJsonObject station = value.toObject();
            stationsMap[station["id"].toInt()] = station;
        }
        displayStations(stations);
    } else {
        qDebug() << "Błąd pobierania stacji:" << reply->errorString();
    }
    reply->deleteLater();
}

/**
 * @brief Obsługuje odpowiedź API z listą czujników.
 * Zapisuje czujniki do mapy i przekazuje listę do QML.
 */
void MainWindow::onSensorsReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray sensors = jsonDoc.array();

        /// Zapisuje czujniki i przygotowuje dane dla QML.
        sensorsMap.clear();
        QVariantList sensorsList;
        for (const QJsonValue& value : sensors) {
            QJsonObject sensor = value.toObject();
            int sensorId = sensor["id"].toInt();
            sensorsMap[sensorId] = sensor;
            QVariantMap sensorData;
            sensorData["id"] = sensorId;
            sensorData["param"] = sensor["param"].toObject()["paramName"].toString();
            sensorData["code"] = sensor["param"].toObject()["paramCode"].toString();
            sensorsList.append(sensorData);
        }
        emit sensorsUpdateRequested(sensorsList);
    } else {
        qDebug() << "Błąd pobierania czujników:" << reply->errorString();
    }
    reply->deleteLater();
}

/**
 * @brief Obsługuje odpowiedź API z indeksem jakości powietrza.
 * Przetwarza dane i aktualizuje interfejs QML.
 */
void MainWindow::onAirQualityIndexReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject airQuality = jsonDoc.object();

        /// Określa poziom jakości powietrza i kolor.
        QString indexLevel = "Brak danych";
        QString color = "#808080";
        if (!airQuality.isEmpty() && airQuality.contains("stIndexLevel") && !airQuality["stIndexLevel"].isNull()) {
            indexLevel = airQuality["stIndexLevel"].toObject()["indexLevelName"].toString();
            if (indexLevel == "Bardzo dobry") color = "#00FF00";
            else if (indexLevel == "Dobry") color = "#97FF00";
            else if (indexLevel == "Umiarkowany") color = "#FFFF00";
            else if (indexLevel == "Dostateczny") color = "#FFBB00";
            else if (indexLevel == "Zły") color = "#FF0000";
            else if (indexLevel == "Bardzo zły") color = "#990000";
        }
        emit airQualityUpdateRequested(indexLevel, color);
    } else {
        qDebug() << "Błąd pobierania indeksu jakości powietrza:" << reply->errorString();
    }
    reply->deleteLater();
}

/**
 * @brief Wyszukuje stacje na podstawie tekstu (nazwa lub miasto).
 * @param searchText Tekst wyszukiwania.
 */
void MainWindow::searchStations(const QString& searchText)
{
    if (searchText.isEmpty()) {
        displayStations(allStations);
        return;
    }
    QJsonArray filteredStations;
    for (const QJsonValue& value : allStations) {
        QJsonObject station = value.toObject();
        QString stationName = station["stationName"].toString().toLower();
        QString city = station["city"].toObject()["name"].toString().toLower();
        if (stationName.contains(searchText.toLower()) || city.contains(searchText.toLower())) {
            filteredStations.append(station);
        }
    }
    displayStations(filteredStations);
}

/**
 * @brief Wyświetla wszystkie dostępne stacje.
 */
void MainWindow::showAllStations()
{
    displayStations(allStations);
}

/**
 * @brief Obsługuje wybór stacji przez użytkownika.
 * @param stationId ID wybranej stacji.
 */
void MainWindow::stationSelected(int stationId)
{
    if (!stationsMap.contains(stationId)) {
        qDebug() << "Nieprawidłowy ID stacji:" << stationId;
        return;
    }
    QJsonObject station = stationsMap[stationId];
    QString lat = QString::number(station["gegrLat"].toDouble());
    QString lon = QString::number(station["gegrLon"].toDouble());
    QString addressStreet = station.contains("addressStreet") ? station["addressStreet"].toString() : "Brak adresu";
    QString city = station["city"].toObject()["name"].toString();
    emit stationInfoUpdateRequested(stationId, station["stationName"].toString(), addressStreet, city, lat, lon);
    fetchSensors(stationId);
    fetchAirQualityIndex(stationId);
}

/**
 * @brief Obsługuje wybór czujnika przez użytkownika.
 * @param sensorId ID wybranego czujnika.
 */
void MainWindow::sensorSelected(int sensorId)
{
    if (!sensorsMap.contains(sensorId)) {
        qDebug() << "Nieprawidłowy ID czujnika:" << sensorId;
        return;
    }
    currentSensorId = sensorId;
    qDebug() << "Wybrano czujnik, pobieranie pomiarów dla ID:" << sensorId;
    fetchMeasurements(sensorId);
}

/**
 * @brief Pobiera czujniki dla wybranej stacji z API.
 * @param stationId ID stacji.
 */
void MainWindow::fetchSensors(int stationId)
{
    QNetworkRequest request(QUrl(API_BASE_URL + API_SENSORS_ENDPOINT + QString::number(stationId)));
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onSensorsReceived);
}

/**
 * @brief Pobiera pomiary dla czujnika, najpierw sprawdza cache.
 * @param sensorId ID czujnika.
 */
void MainWindow::fetchMeasurements(int sensorId)
{
    if (isCacheValid(sensorId)) {
        QJsonObject cachedData = getFromCache(sensorId);
        if (!cachedData.isEmpty()) {
            qDebug() << "Używanie danych z pamięci podręcznej dla czujnika ID:" << sensorId;
            processAndDisplayMeasurements(cachedData);
            emit statisticsUpdated(computeStatistics(sensorId));
            autoSaveMeasurements();
            return;
        }
    }
    QNetworkRequest request(QUrl(API_BASE_URL + API_MEASUREMENTS_ENDPOINT + QString::number(sensorId)));
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onMeasurementsReceived);
}

/**
 * @brief Pobiera indeks jakości powietrza dla stacji.
 * @param stationId ID stacji.
 */
void MainWindow::fetchAirQualityIndex(int stationId)
{
    QNetworkRequest request(QUrl(API_BASE_URL + API_AIR_QUALITY_ENDPOINT + QString::number(stationId)));
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onAirQualityIndexReceived);
}

/**
 * @brief Przygotowuje listę stacji do wyświetlenia w QML.
 * @param stations Tablica stacji w formacie JSON.
 */
void MainWindow::displayStations(const QJsonArray& stations)
{
    QVariantList stationsList;
    for (const QJsonValue& value : stations) {
        QJsonObject station = value.toObject();
        QVariantMap stationData;
        stationData["id"] = station["id"].toInt();
        stationData["name"] = station["stationName"].toString();
        stationData["city"] = station["city"].toObject()["name"].toString();
        stationsList.append(stationData);
    }
    emit stationsUpdateRequested(stationsList);
}

/**
 * @brief Generuje szczegółowe informacje o stacji.
 * @param station Obiekt JSON z danymi stacji.
 * @return Tekst z informacjami o stacji.
 */
QString MainWindow::generateStationInfo(const QJsonObject& station)
{
    QString info = station["stationName"].toString() + "\n";
    QJsonObject city = station["city"].toObject();
    QJsonObject commune = city["commune"].toObject();
    info += "Miasto: " + city["name"].toString() + "\n";
    info += "Gmina: " + commune["communeName"].toString() + "\n";
    info += "Województwo: " + commune["provinceName"].toString() + "\n";
    if (station.contains("addressStreet")) {
        info += "Adres: " + station["addressStreet"].toString() + "\n";
    }
    double lat = station["gegrLat"].toDouble();
    double lon = station["gegrLon"].toDouble();
    info += QString("Współrzędne: %1, %2").arg(lat).arg(lon);
    return info;
}

/**
 * @brief Zwraca ścieżkę do pliku pamięci podręcznej.
 * @return Ścieżka do pliku cache.
 */
QString MainWindow::getCachePath()
{
    return getDataDirectory() + "/" + CACHE_FILENAME;
}

/**
 * @brief Zapisuje dane do pliku pamięci podręcznej.
 * @param cacheData Dane do zapisania.
 * @return True, jeśli zapis się powiódł; false w przeciwnym razie.
 */
bool MainWindow::saveToCacheFile(const QJsonObject& cacheData)
{
    QFile file(getCachePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Błąd otwierania pliku pamięci podręcznej do zapisu:" << file.errorString();
        emit autoSaveStatus("Błąd zapisu pamięci podręcznej: " + getCachePath(), false);
        emit dataPathInfo("Błąd zapisu pamięci podręcznej: " + getCachePath());
        return false;
    }
    QJsonDocument doc(cacheData);
    file.write(doc.toJson());
    file.close();
    qDebug() << "Zapisano pamięć podręczną do:" << getCachePath();
    emit dataPathInfo("Zapisano pamięć podręczną: " + getCachePath());
    return true;
}

/**
 * @brief Wczytuje dane z pliku pamięci podręcznej.
 * @return Obiekt JSON z danymi cache lub pusty obiekt w razie błędu.
 */
QJsonObject MainWindow::loadFromCacheFile()
{
    QFile file(getCachePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Błąd otwierania pliku pamięci podręcznej do odczytu:" << file.errorString();
        return QJsonObject();
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return doc.object();
}

/**
 * @brief Sprawdza, czy dane w pamięci podręcznej są aktualne (ważne 24h).
 * @param sensorId ID czujnika.
 * @return True, jeśli cache jest ważny; false w przeciwnym razie.
 */
bool MainWindow::isCacheValid(int sensorId)
{
    QJsonObject cache = loadFromCacheFile();
    if (cache.isEmpty() || !cache.contains(QString::number(sensorId))) {
        return false;
    }
    QJsonObject sensorCache = cache[QString::number(sensorId)].toObject();
    if (!sensorCache.contains("timestamp")) {
        return false;
    }
    QDateTime cacheTime = QDateTime::fromString(sensorCache["timestamp"].toString(), Qt::ISODate);
    return cacheTime.secsTo(QDateTime::currentDateTime()) < CACHE_VALIDITY_HOURS * 3600;
}

/**
 * @brief Aktualizuje pamięć podręczną dla czujnika.
 * @param sensorId ID czujnika.
 * @param data Dane do zapisania.
 */
void MainWindow::updateCache(int sensorId, const QJsonObject& data)
{
    QJsonObject cache = loadFromCacheFile();
    QJsonObject sensorCache;
    sensorCache["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    sensorCache["data"] = data;
    cache[QString::number(sensorId)] = sensorCache;
    saveToCacheFile(cache);
    cleanupOldCache();
}

/**
 * @brief Pobiera dane z pamięci podręcznej dla czujnika.
 * @param sensorId ID czujnika.
 * @return Obiekt JSON z danymi lub pusty obiekt, jeśli brak danych.
 */
QJsonObject MainWindow::getFromCache(int sensorId)
{
    QJsonObject cache = loadFromCacheFile();
    if (cache.isEmpty() || !cache.contains(QString::number(sensorId))) {
        return QJsonObject();
    }
    return cache[QString::number(sensorId)].toObject()["data"].toObject();
}

/**
 * @brief Usuwa stare dane z pamięci podręcznej.
 */
void MainWindow::cleanupOldCache()
{
    QJsonObject cache = loadFromCacheFile();
    QJsonObject newCache;
    QDateTime now = QDateTime::currentDateTime();
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        QJsonObject sensorCache = it.value().toObject();
        if (sensorCache.contains("timestamp")) {
            QDateTime cacheTime = QDateTime::fromString(sensorCache["timestamp"].toString(), Qt::ISODate);
            if (cacheTime.secsTo(now) < CACHE_VALIDITY_HOURS * 3600) {
                newCache[it.key()] = it.value();
            }
        }
    }
    saveToCacheFile(newCache);
}

/**
 * @brief Wczytuje historyczne dane dla czujnika i klucza daty.
 * @param sensorId ID czujnika.
 * @param dateKey Klucz daty dla danych.
 */
void MainWindow::loadHistoricalData(int sensorId, const QString& dateKey)
{
    if (!sensorsMap.contains(sensorId)) {
        qDebug() << "Nieprawidłowy ID czujnika:" << sensorId;
        return;
    }
    QString historyPath = getDataDirectory() + "/air_quality_history.json";
    QFile historyFile(historyPath);
    if (!historyFile.exists() || !historyFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Brak pliku historii lub błąd otwarcia:" << historyFile.errorString();
        emit measurementsUpdateRequested("Brak danych historycznych", QVariantList());
        emit dataPathInfo("Brak pliku historii: " + historyPath);
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(historyFile.readAll());
    historyFile.close();
    if (doc.isNull()) {
        qDebug() << "Nieprawidłowy JSON w pliku historii:" << historyPath;
        emit measurementsUpdateRequested("Błąd danych historycznych", QVariantList());
        emit dataPathInfo("Błąd: Nieprawidłowy JSON w pliku " + historyPath);
        return;
    }
    QJsonObject history = doc.object();
    QString sensorKey = QString::number(sensorId);
    if (!history.contains(sensorKey) || !history[sensorKey].toObject().contains(dateKey)) {
        qDebug() << "Brak danych historycznych dla czujnika ID:" << sensorId << "lub klucza:" << dateKey;
        emit measurementsUpdateRequested("Brak danych dla tej daty", QVariantList());
        return;
    }
    QJsonObject data = history[sensorKey].toObject()[dateKey].toObject();
    if (!data.contains("key") || !data.contains("values")) {
        qDebug() << "Niekompletne dane historyczne dla klucza daty:" << dateKey;
        emit measurementsUpdateRequested("Niekompletne dane", QVariantList());
        return;
    }
    QString key = data["key"].toString();
    QJsonArray values = data["values"].toArray();
    QVariantList valuesList;
    for (const QJsonValue& value : values) {
        QJsonObject measurement = value.toObject();
        QVariantMap point;
        point["date"] = measurement["date"].toString();
        point["value"] = measurement["value"].isNull() ? QVariant() : measurement["value"].toDouble();
        valuesList.append(point);
    }
    qDebug() << "Wczytano historyczne pomiary dla czujnika ID:" << sensorId << "klucz daty:" << dateKey;
    emit measurementsUpdateRequested(key + " [HISTORYCZNY]", valuesList);
    emit statisticsUpdated(computeStatistics(sensorId));
}

/**
 * @brief Zwraca listę dostępnych danych historycznych dla czujnika.
 * @param sensorId ID czujnika.
 * @return Lista dat i kluczy historycznych.
 */
QStringList MainWindow::getAvailableHistoricalData(int sensorId)
{
    QStringList results;
    QString historyPath = getDataDirectory() + "/air_quality_history.json";
    QFile historyFile(historyPath);
    if (!historyFile.exists() || !historyFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Brak pliku historii lub błąd otwarcia:" << historyFile.errorString();
        emit historicalDataListUpdated(results);
        emit dataPathInfo("Brak pliku historii: " + historyPath);
        return results;
    }
    QJsonDocument doc = QJsonDocument::fromJson(historyFile.readAll());
    historyFile.close();
    if (doc.isNull()) {
        qDebug() << "Nieprawidłowy JSON w pliku historii:" << historyPath;
        emit historicalDataListUpdated(results);
        emit dataPathInfo("Błąd: Nieprawidłowy JSON w pliku " + historyPath);
        return results;
    }
    QJsonObject history = doc.object();
    QString sensorKey = QString::number(sensorId);
    if (history.contains(sensorKey)) {
        QJsonObject sensorHistory = history[sensorKey].toObject();
        for (auto it = sensorHistory.begin(); it != sensorHistory.end(); ++it) {
            QString dateKey = it.key();
            QDateTime dt = QDateTime::fromString(dateKey, "yyyyMMdd_HHmmss");
            if (dt.isValid()) {
                results.append(dt.toString("yyyy-MM-dd HH:mm:ss") + "|" + dateKey);
            }
        }
    }
    results.sort(Qt::CaseInsensitive);
    qDebug() << "Znaleziono" << results.size() << "wpisów historycznych dla czujnika ID:" << sensorId;
    emit historicalDataListUpdated(results);
    return results;
}

/**
 * @brief Wczytuje dane z pliku JSON.
 * @param filename Ścieżka do pliku JSON.
 * @return Obiekt JSON z danymi lub pusty obiekt w razie błędu.
 */
QJsonObject MainWindow::loadFromJson(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Błąd otwierania pliku do odczytu:" << file.errorString();
        emit dataPathInfo("Błąd odczytu pliku: " + filename);
        return QJsonObject();
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isNull()) {
        qDebug() << "Nieprawidłowy JSON w pliku:" << filename;
        emit dataPathInfo("Błąd: Nieprawidłowy JSON w pliku " + filename);
        return QJsonObject();
    }
    return doc.object();
}

/**
 * @brief Wczytuje dane z pliku XML.
 * @param filename Ścieżka do pliku XML.
 * @return Obiekt JSON z danymi lub pusty obiekt w razie błędu.
 */
QJsonObject MainWindow::loadFromXml(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Błąd otwierania pliku do odczytu:" << file.errorString();
        emit dataPathInfo("Błąd odczytu pliku: " + filename);
        return QJsonObject();
    }
    QXmlStreamReader reader(&file);
    QJsonObject result;
    QJsonArray values;
    while (!reader.atEnd()) {
        if (reader.readNextStartElement()) {
            if (reader.name() == "Key") {
                result["key"] = reader.readElementText();
            } else if (reader.name() == "SaveDate") {
                result["saveDate"] = reader.readElementText();
            } else if (reader.name() == "SensorInfo") {
                QJsonObject sensorInfo;
                while (reader.readNextStartElement()) {
                    sensorInfo[reader.name().toString()] = reader.readElementText();
                }
                result["sensorInfo"] = sensorInfo;
            } else if (reader.name() == "Values") {
                while (reader.readNextStartElement()) {
                    if (reader.name() == "Measurement") {
                        QJsonObject measurement;
                        while (reader.readNextStartElement()) {
                            if (reader.name() == "Date") {
                                measurement["date"] = reader.readElementText();
                            } else if (reader.name() == "Value") {
                                QString valueStr = reader.readElementText();
                                measurement["value"] = (valueStr == "null") ? QJsonValue::Null : valueStr.toDouble();
                            } else {
                                reader.skipCurrentElement();
                            }
                        }
                        values.append(measurement);
                    } else {
                        reader.skipCurrentElement();
                    }
                }
            } else {
                reader.skipCurrentElement();
            }
        }
    }
    if (reader.hasError()) {
        qDebug() << "Błąd parsowania XML:" << reader.errorString();
        emit dataPathInfo("Błąd parsowania XML: " + filename);
    }
    result["values"] = values;
    file.close();
    return result;
}

/**
 * @brief Obsługuje odpowiedź z pomiarami z API.
 * Aktualizuje cache, przetwarza dane i wywołuje autozapis.
 */
void MainWindow::onMeasurementsReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());
        if (jsonDoc.isNull()) {
            qDebug() << "Nieprawidłowa odpowiedź JSON dla pomiarów";
            emit measurementsUpdateRequested("Błąd danych", QVariantList());
            return;
        }
        QJsonObject measurements = jsonDoc.object();
        int sensorId = reply->request().url().toString().split('/').last().toInt();
        qDebug() << "Odebrano pomiary dla czujnika ID:" << sensorId;
        updateCache(sensorId, measurements);
        processAndDisplayMeasurements(measurements);
        emit statisticsUpdated(computeStatistics(sensorId));
        autoSaveMeasurements();
    } else {
        qDebug() << "Błąd pobierania pomiarów:" << reply->errorString();
        emit measurementsUpdateRequested("Błąd pobierania danych", QVariantList());
    }
    reply->deleteLater();
}

/**
 * @brief Przetwarza i wyświetla pomiary w QML.
 * @param measurements Obiekt JSON z danymi pomiarowymi.
 */
void MainWindow::processAndDisplayMeasurements(const QJsonObject& measurements)
{
    currentMeasurements = measurements;
    if (!measurements.contains("key") || !measurements.contains("values")) {
        qDebug() << "Nieprawidłowe dane pomiarów: brak klucza lub wartości";
        emit measurementsUpdateRequested("Brak danych", QVariantList());
        return;
    }
    QString key = measurements["key"].toString();
    QJsonArray values = measurements["values"].toArray();
    QVariantList valuesList;
    int validCount = 0;
    for (const QJsonValue& value : values) {
        QJsonObject measurement = value.toObject();
        QString dateStr = measurement["date"].toString();
        QVariant valueVariant = measurement["value"].toVariant();
        if (!valueVariant.isNull() && !dateStr.isEmpty()) {
            validCount++;
        }
        QVariantMap point;
        point["date"] = dateStr;
        point["value"] = valueVariant.isNull() ? QVariant() : valueVariant.toDouble();
        valuesList.append(point);
    }
    qDebug() << "Przetworzono" << valuesList.size() << "pomiarów," << validCount << "ważnych, dla klucza:" << key;
    emit measurementsUpdateRequested(key, valuesList);
}

/**
 * @brief Oblicza statystyki (min, max, średnia, odchylenie) dla pomiarów.
 * @param sensorId ID czujnika.
 * @return Mapa QVariant z obliczonymi statystykami.
 */
QVariantMap MainWindow::computeStatistics(int sensorId)
{
    QVariantMap stats;
    stats["min"] = QVariant();
    stats["max"] = QVariant();
    stats["mean"] = QVariant();
    stats["stdDev"] = QVariant();
    stats["count"] = 0;
    if (!currentMeasurements.contains("values")) {
        qDebug() << "Brak wartości do obliczania statystyk";
        return stats;
    }
    QJsonArray values = currentMeasurements["values"].toArray();
    if (values.isEmpty()) {
        qDebug() << "Pusta tablica wartości dla statystyk";
        return stats;
    }
    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();
    double sum = 0.0;
    double sumSquares = 0.0;
    int count = 0;
    for (const QJsonValue& value : values) {
        QJsonObject measurement = value.toObject();
        if (measurement["value"].isNull()) {
            continue;
        }
        double val = measurement["value"].toDouble();
        if (std::isnan(val) || std::isinf(val)) {
            continue;
        }
        minVal = std::min(minVal, val);
        maxVal = std::max(maxVal, val);
        sum += val;
        sumSquares += val * val;
        count++;
    }
    if (count == 0) {
        qDebug() << "Brak ważnych danych do statystyk";
        return stats;
    }
    double mean = sum / count;
    double stdDev = count > 1 ? std::sqrt((sumSquares / count) - (mean * mean)) : 0.0;
    stats["min"] = minVal;
    stats["max"] = maxVal;
    stats["mean"] = mean;
    stats["stdDev"] = stdDev;
    stats["count"] = count;
    qDebug() << "Obliczono statystyki dla czujnika ID:" << sensorId << ": min=" << minVal << ", max=" << maxVal << ", średnia=" << mean;
    return stats;
}

/**
 * @brief Importuje dane z pliku JSON lub XML.
 * @param path Ścieżka do pliku.
 * @param format Format pliku (json lub xml).
 * @return True, jeśli import się powiódł; false w przeciwnym razie.
 */
bool MainWindow::importDataFromFile(const QString& path, const QString& format)
{
    QJsonObject data;
    if (format.toLower() == "json") {
        data = loadFromJson(path);
    } else if (format.toLower() == "xml") {
        data = loadFromXml(path);
    } else {
        qDebug() << "Nieobsługiwany format importu:" << format;
        emit dataPathInfo("Nieobsługiwany format importu: " + path);
        return false;
    }
    if (data.isEmpty()) {
        qDebug() << "Brak ważnych danych zaimportowanych z:" << path;
        emit dataPathInfo("Brak ważnych danych z pliku: " + path);
        return false;
    }
    if (!data.contains("sensorInfo") || !data.contains("key") || !data.contains("values")) {
        qDebug() << "Zaimportowane dane nie zawierają wymaganych pól";
        emit dataPathInfo("Niekompletne dane w pliku: " + path);
        return false;
    }
    QJsonObject sensorInfo = data["sensorInfo"].toObject();
    int sensorId = sensorInfo["id"].toInt();
    QString dateKey = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    if (!saveToHistoryFile(sensorId, data, dateKey)) {
        qDebug() << "Błąd zapisu zaimportowanych danych do historii";
        return false;
    }
    qDebug() << "Pomyślnie zaimportowano dane dla czujnika ID:" << sensorId;
    return true;
}

/**
 * @brief Usuwa dane historyczne dla podanego klucza daty.
 * @param dateKey Klucz daty do usunięcia.
 * @return True, jeśli usunięcie się powiodło; false w przeciwnym razie.
 */
bool MainWindow::deleteHistoricalData(const QString& dateKey)
{
    QString historyPath = getDataDirectory() + "/air_quality_history.json";
    QFile historyFile(historyPath);
    QJsonObject history;
    if (!historyFile.exists() || !historyFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Brak pliku historii lub błąd otwarcia:" << historyFile.errorString();
        emit dataPathInfo("Brak pliku historii: " + historyPath);
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(historyFile.readAll());
    historyFile.close();
    if (doc.isNull()) {
        qDebug() << "Nieprawidłowy JSON w pliku historii:" << historyPath;
        emit dataPathInfo("Błąd: Nieprawidłowy JSON w pliku " + historyPath);
        return false;
    }
    history = doc.object();
    bool modified = false;
    for (auto it = history.begin(); it != history.end(); ++it) {
        QJsonObject sensorHistory = it.value().toObject();
        if (sensorHistory.contains(dateKey)) {
            sensorHistory.remove(dateKey);
            history[it.key()] = sensorHistory;
            modified = true;
        }
    }
    if (!modified) {
        qDebug() << "Brak danych dla klucza daty:" << dateKey;
        return false;
    }
    QString tempPath = historyPath + ".tmp";
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        qDebug() << "Błąd otwierania pliku tymczasowego do zapisu:" << tempFile.errorString();
        emit dataPathInfo("Błąd zapisu: Brak dostępu do pliku " + tempPath);
        return false;
    }
    QJsonDocument newDoc(history);
    tempFile.write(newDoc.toJson());
    tempFile.close();
    if (historyFile.exists() && !historyFile.remove()) {
        qDebug() << "Błąd usuwania starego pliku historii:" << historyFile.errorString();
        emit dataPathInfo("Błąd zapisu: Nie można usunąć starego pliku " + historyPath);
        return false;
    }
    if (!tempFile.rename(historyPath)) {
        qDebug() << "Błąd zmiany nazwy pliku tymczasowego:" << tempFile.errorString();
        emit dataPathInfo("Błąd zapisu: Problem z zapisem pliku " + historyPath);
        return false;
    }
    qDebug() << "Pomyślnie usunięto dane historyczne dla klucza daty:" << dateKey;
    emit dataPathInfo("Usunięto dane historyczne: " + historyPath);
    return true;
}

/**
 * @brief Ponownie próbuje pobrać dane z API w razie problemów.
 */
void MainWindow::retryConnection()
{
    qDebug() << "Ponowne próbowanie połączenia z API GIOŚ";
    fetchStations();
}
