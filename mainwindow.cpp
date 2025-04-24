#include "mainwindow.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QUrl>
#include <QFileInfo>

// Tworzenie obiektu MainWindow
MainWindow::MainWindow(QObject *parent)
    : QObject(parent)
{
    // Inicjalizowanie menedżera sieci
    networkManager = new QNetworkAccessManager(this);
    // Pobieranie listy stacji
    fetchStations();

    // Tworzenie katalogu na dane, jeśli nie istnieje
    QDir dir;
    dir.mkpath(getDataDirectory());
}

// Zwalnianie zasobów
MainWindow::~MainWindow()
{
}

// Zwracanie ścieżki do katalogu z danymi
QString MainWindow::getDataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data";
}

// Generowanie nazwy pliku na podstawie ID sensora, formatu i daty
QString MainWindow::generateFilename(int sensorId, const QString& format, const QDateTime& dateTime)
{
    QString dateStr = dateTime.toString("yyyyMMdd_HHmmss");
    return getDataDirectory() + QString("/sensor_%1_%2.%3").arg(sensorId).arg(dateStr).arg(format.toLower());
}

// Zapisywanie danych do pliku w wybranym formacie
void MainWindow::saveDataToFile(int sensorId, const QString& format)
{
    // Sprawdzanie, czy dane i ID sensora są prawidłowe
    if (!sensorsMap.contains(sensorId) || currentMeasurements.isEmpty()) {
        qDebug() << "No data to save or invalid sensor ID";
        return;
    }

    // Przygotowywanie danych do zapisu
    QJsonObject dataToSave = currentMeasurements;
    dataToSave["sensorInfo"] = sensorsMap[sensorId];
    dataToSave["saveDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QString dateKey = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    // Wczytywanie historii zapisanych danych
    QString historyPath = getDataDirectory() + "/air_quality_history.json";
    QJsonObject history;
    QFile historyFile(historyPath);

    if (historyFile.exists() && historyFile.open(QIODevice::ReadOnly)) {
        QByteArray data = historyFile.readAll();
        history = QJsonDocument::fromJson(data).object();
        historyFile.close();
    }

    // Dodawanie nowych danych do historii
    if (!history.contains(QString::number(sensorId))) {
        history[QString::number(sensorId)] = QJsonObject();
    }

    QJsonObject sensorHistory = history[QString::number(sensorId)].toObject();
    sensorHistory[dateKey] = dataToSave;
    history[QString::number(sensorId)] = sensorHistory;

    // Zapisywanie historii do pliku
    if (historyFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(history);
        historyFile.write(doc.toJson());
        historyFile.close();
        qDebug() << "Data saved successfully to history file";
    } else {
        qDebug() << "Failed to save data to history file:" << historyFile.errorString();
    }

    // Zapisywanie danych do osobnego pliku
    QString filename = generateFilename(sensorId, format);
    bool success = false;

    if (format.toLower() == "json") {
        success = saveToJson(filename, dataToSave);
    } else if (format.toLower() == "xml") {
        success = saveToXml(filename, dataToSave);
    }

    if (success) {
        qDebug() << "Data also saved to individual file" << filename;
    }
}

// Zapisywanie danych w formacie JSON
bool MainWindow::saveToJson(const QString& filename, const QJsonObject& data)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open file for writing:" << file.errorString();
        return false;
    }

    QJsonDocument doc(data);
    file.write(doc.toJson());
    file.close();
    return true;
}

// Pobieranie listy stacji z API
void MainWindow::fetchStations()
{
    QNetworkRequest request;
    request.setUrl(QUrl(API_BASE_URL + API_STATIONS_ENDPOINT));
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onStationsReceived);
}

// Obsługa odpowiedzi z listą stacji
void MainWindow::onStationsReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        // Wczytywanie i zapisywanie listy stacji
        QByteArray response = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QJsonArray stations = jsonDoc.array();
        allStations = stations;

        stationsMap.clear();
        for (const QJsonValue& value : stations) {
            QJsonObject station = value.toObject();
            stationsMap[station["id"].toInt()] = station;
        }

        // Wyświetlanie stacji
        displayStations(stations);
    } else {
        qDebug() << "Error fetching stations:" << reply->errorString();
    }
    reply->deleteLater();
}

// Obsługa odpowiedzi z listą sensorów
void MainWindow::onSensorsReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        // Wczytywanie i zapisywanie listy sensorów
        QByteArray response = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QJsonArray sensors = jsonDoc.array();

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

        // Aktualizowanie listy sensorów w interfejsie
        emit sensorsUpdateRequested(sensorsList);
    } else {
        qDebug() << "Error fetching sensors:" << reply->errorString();
    }
    reply->deleteLater();
}

// Obsługa odpowiedzi z indeksem jakości powietrza
void MainWindow::onAirQualityIndexReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        // Wczytywanie danych o jakości powietrza
        QByteArray response = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QJsonObject airQuality = jsonDoc.object();

        QString indexLevel = "Brak danych";
        QString color = "#808080";

        // Określanie poziomu jakości powietrza i koloru
        if (!airQuality.isEmpty() && airQuality.contains("stIndexLevel") &&
            !airQuality["stIndexLevel"].isNull()) {
            QJsonObject stIndexLevel = airQuality["stIndexLevel"].toObject();
            indexLevel = stIndexLevel["indexLevelName"].toString();

            if (indexLevel == "Bardzo dobry") {
                color = "#00FF00";
            } else if (indexLevel == "Dobry") {
                color = "#97FF00";
            } else if (indexLevel == "Umiarkowany") {
                color = "#FFFF00";
            } else if (indexLevel == "Dostateczny") {
                color = "#FFBB00";
            } else if (indexLevel == "Zły") {
                color = "#FF0000";
            } else if (indexLevel == "Bardzo zły") {
                color = "#990000";
            }
        }

        // Aktualizowanie informacji o jakości powietrza
        emit airQualityUpdateRequested(indexLevel, color);
    } else {
        qDebug() << "Error fetching air quality index:" << reply->errorString();
    }
    reply->deleteLater();
}

// Wyszukiwanie stacji na podstawie tekstu
void MainWindow::searchStations(const QString& searchText)
{
    if (searchText.isEmpty()) {
        // Wyświetlanie wszystkich stacji, jeśli brak tekstu
        displayStations(allStations);
        return;
    }

    // Filtrowanie stacji według nazwy lub miasta
    QJsonArray filteredStations;
    for (const QJsonValue& value : allStations) {
        QJsonObject station = value.toObject();
        QString stationName = station["stationName"].toString().toLower();
        QString city = station["city"].toObject()["name"].toString().toLower();

        if (stationName.contains(searchText.toLower()) || city.contains(searchText.toLower())) {
            filteredStations.append(station);
        }
    }

    // Wyświetlanie przefiltrowanych stacji
    displayStations(filteredStations);
}

// Wyświetlanie wszystkich stacji
void MainWindow::showAllStations()
{
    displayStations(allStations);
}

// Obsługa wyboru stacji
void MainWindow::stationSelected(int stationId)
{
    if (!stationsMap.contains(stationId)) {
        qDebug() << "Invalid station ID:" << stationId;
        return;
    }

    // Przygotowywanie danych o stacji
    QJsonObject station = stationsMap[stationId];
    QString lat = QString::number(station["gegrLat"].toDouble());
    QString lon = QString::number(station["gegrLon"].toDouble());
    QString addressStreet = station.contains("addressStreet") ? station["addressStreet"].toString() : "Brak adresu";
    QString city = station["city"].toObject()["name"].toString();

    // Aktualizowanie informacji o stacji w interfejsie
    emit stationInfoUpdateRequested(stationId, station["stationName"].toString(), addressStreet, city, lat, lon);

    // Pobieranie sensorów i indeksu jakości powietrza
    fetchSensors(stationId);
    fetchAirQualityIndex(stationId);
}

// Obsługa wyboru sensora
void MainWindow::sensorSelected(int sensorId)
{
    if (!sensorsMap.contains(sensorId)) {
        qDebug() << "Invalid sensor ID:" << sensorId;
        return;
    }

    // Pobieranie pomiarów dla sensora
    fetchMeasurements(sensorId);
}

// Pobieranie listy sensorów dla stacji
void MainWindow::fetchSensors(int stationId)
{
    QNetworkRequest request;
    request.setUrl(QUrl(API_BASE_URL + API_SENSORS_ENDPOINT + QString::number(stationId)));
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onSensorsReceived);
}

// Pobieranie pomiarów dla sensora
void MainWindow::fetchMeasurements(int sensorId)
{
    // Sprawdzanie, czy dane są w pamięci podręcznej
    if (isCacheValid(sensorId)) {
        QJsonObject cachedData = getFromCache(sensorId);
        if (!cachedData.isEmpty()) {
            // Wyświetlanie danych z pamięci podręcznej
            processAndDisplayMeasurements(cachedData);
            return;
        }
    }

    // Pobieranie danych z API
    QNetworkRequest request;
    request.setUrl(QUrl(API_BASE_URL + API_MEASUREMENTS_ENDPOINT + QString::number(sensorId)));
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onMeasurementsReceived);
}

// Pobieranie indeksu jakości powietrza dla stacji
void MainWindow::fetchAirQualityIndex(int stationId)
{
    QNetworkRequest request;
    request.setUrl(QUrl(API_BASE_URL + API_AIR_QUALITY_ENDPOINT + QString::number(stationId)));
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &MainWindow::onAirQualityIndexReceived);
}

// Wyświetlanie listy stacji w interfejsie
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
    // Aktualizowanie listy stacji w interfejsie
    emit stationsUpdateRequested(stationsList);
}

// Generowanie opisu stacji
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

// Zapisywanie danych w formacie XML
bool MainWindow::saveToXml(const QString& filename, const QJsonObject& data)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open file for writing:" << file.errorString();
        return false;
    }

    // Tworzenie struktury XML
    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("SensorData");

    if (data.contains("sensorInfo")) {
        QJsonObject sensorInfo = data["sensorInfo"].toObject();
        writer.writeStartElement("SensorInfo");

        for (auto it = sensorInfo.begin(); it != sensorInfo.end(); ++it) {
            writer.writeTextElement(it.key(), it.value().toVariant().toString());
        }

        writer.writeEndElement();
    }

    if (data.contains("saveDate")) {
        writer.writeTextElement("SaveDate", data["saveDate"].toString());
    }

    if (data.contains("key")) {
        writer.writeTextElement("Key", data["key"].toString());
    }

    if (data.contains("values")) {
        QJsonArray values = data["values"].toArray();
        writer.writeStartElement("Values");

        for (const QJsonValue& value : values) {
            QJsonObject measurement = value.toObject();
            writer.writeStartElement("Measurement");
            writer.writeTextElement("Date", measurement["date"].toString());

            if (!measurement["value"].isNull()) {
                writer.writeTextElement("Value", QString::number(measurement["value"].toDouble()));
            } else {
                writer.writeTextElement("Value", "null");
            }

            writer.writeEndElement();
        }

        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndDocument();

    file.close();
    return true;
}

// Zwracanie ścieżki do pliku pamięci podręcznej
QString MainWindow::getCachePath()
{
    return getDataDirectory() + "/" + CACHE_FILENAME;
}

// Zapisywanie danych do pliku pamięci podręcznej
bool MainWindow::saveToCacheFile(const QJsonObject& cacheData)
{
    QFile file(getCachePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open cache file for writing:" << file.errorString();
        return false;
    }

    QJsonDocument doc(cacheData);
    file.write(doc.toJson());
    file.close();
    return true;
}

// Wczytywanie danych z pliku pamięci podręcznej
QJsonObject MainWindow::loadFromCacheFile()
{
    QFile file(getCachePath());
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open cache file for reading:" << file.errorString();
        return QJsonObject();
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    return doc.object();
}

// Sprawdzanie ważności danych w pamięci podręcznej
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
    QDateTime now = QDateTime::currentDateTime();

    return cacheTime.secsTo(now) < CACHE_VALIDITY_HOURS * 3600;
}

// Aktualizowanie pamięci podręcznej
void MainWindow::updateCache(int sensorId, const QJsonObject& data)
{
    QJsonObject cache = loadFromCacheFile();

    QJsonObject sensorCache;
    sensorCache["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    sensorCache["data"] = data;

    cache[QString::number(sensorId)] = sensorCache;

    // Zapisywanie i czyszczenie pamięci podręcznej
    saveToCacheFile(cache);

    cleanupOldCache();
}

// Pobieranie danych z pamięci podręcznej
QJsonObject MainWindow::getFromCache(int sensorId)
{
    QJsonObject cache = loadFromCacheFile();
    if (cache.isEmpty() || !cache.contains(QString::number(sensorId))) {
        return QJsonObject();
    }

    return cache[QString::number(sensorId)].toObject()["data"].toObject();
}

// Czyszczenie starej pamięci podręcznej
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

// Wczytywanie historycznych danych
void MainWindow::loadHistoricalData(int sensorId, const QString& dateKey)
{
    if (!sensorsMap.contains(sensorId)) {
        qDebug() << "Invalid sensor ID";
        return;
    }

    // Wczytywanie pliku z historią
    QString historyPath = getDataDirectory() + "/air_quality_history.json";
    QFile historyFile(historyPath);

    if (!historyFile.exists() || !historyFile.open(QIODevice::ReadOnly)) {
        qDebug() << "No history file found or could not open:" << historyFile.errorString();
        return;
    }

    QByteArray fileData = historyFile.readAll();
    historyFile.close();

    QJsonObject history = QJsonDocument::fromJson(fileData).object();

    if (history.contains(QString::number(sensorId))) {
        QJsonObject sensorHistory = history[QString::number(sensorId)].toObject();

        if (sensorHistory.contains(dateKey)) {
            QJsonObject data = sensorHistory[dateKey].toObject();

            if (!data.isEmpty() && data.contains("key") && data.contains("values")) {
                // Przygotowywanie danych historycznych do wyświetlenia
                QString key = data["key"].toString();
                QJsonArray values = data["values"].toArray();

                QVariantList valuesList;
                for (const QJsonValue& value : values) {
                    QJsonObject measurement = value.toObject();
                    QString dateStr = measurement["date"].toString();
                    QVariant valueVariant = measurement["value"].toVariant();

                    QVariantMap point;
                    point["date"] = dateStr;
                    point["value"] = valueVariant.isNull() ? QVariant() : valueVariant.toDouble();
                    valuesList.append(point);
                }

                // Aktualizowanie interfejsu z danymi historycznymi
                emit measurementsUpdateRequested(key + " [HISTORYCZNY]", valuesList);
            } else {
                qDebug() << "Failed to load historical data or data is incomplete";
            }
        }
    }
}

// Zwracanie listy dostępnych danych historycznych
QStringList MainWindow::getAvailableHistoricalData(int sensorId)
{
    QStringList results;

    QString historyPath = getDataDirectory() + "/air_quality_history.json";
    QFile historyFile(historyPath);

    if (!historyFile.exists() || !historyFile.open(QIODevice::ReadOnly)) {
        qDebug() << "No history file found or could not open:" << historyFile.errorString();
        emit historicalDataListUpdated(results);
        return results;
    }

    // Wczytywanie i sortowanie danych historycznych
    QByteArray data = historyFile.readAll();
    historyFile.close();

    QJsonObject history = QJsonDocument::fromJson(data).object();

    if (history.contains(QString::number(sensorId))) {
        QJsonObject sensorHistory = history[QString::number(sensorId)].toObject();

        for (auto it = sensorHistory.begin(); it != sensorHistory.end(); ++it) {
            QString dateKey = it.key();
            QDateTime dt = QDateTime::fromString(dateKey, "yyyyMMdd_HHmmss");
            if (dt.isValid()) {
                QString displayName = dt.toString("yyyy-MM-dd HH:mm:ss");
                results.append(displayName + "|" + dateKey);
            }
        }
    }

    std::sort(results.begin(), results.end(), std::greater<QString>());

    // Aktualizowanie listy danych historycznych w interfejsie
    emit historicalDataListUpdated(results);
    return results;
}

// Wczytywanie danych z pliku JSON
QJsonObject MainWindow::loadFromJson(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file for reading:" << file.errorString();
        return QJsonObject();
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    return doc.object();
}

// Wczytywanie danych z pliku XML
QJsonObject MainWindow::loadFromXml(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file for reading:" << file.errorString();
        return QJsonObject();
    }

    // Parsowanie pliku XML
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
                                if (valueStr == "null") {
                                    measurement["value"] = QJsonValue::Null;
                                } else {
                                    measurement["value"] = valueStr.toDouble();
                                }
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
        qDebug() << "XML parsing error:" << reader.errorString();
    }

    result["values"] = values;
    file.close();
    return result;
}

// Obsługa odpowiedzi z pomiarami
void MainWindow::onMeasurementsReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        // Wczytywanie i zapisywanie pomiarów
        QByteArray response = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QJsonObject measurements = jsonDoc.object();

        int sensorId = reply->request().url().toString().split('/').last().toInt();
        updateCache(sensorId, measurements);

        // Wyświetlanie pomiarów
        processAndDisplayMeasurements(measurements);
    } else {
        qDebug() << "Error fetching measurements:" << reply->errorString();
    }
    reply->deleteLater();
}

// Przetwarzanie i wyświetlanie pomiarów
void MainWindow::processAndDisplayMeasurements(const QJsonObject& measurements)
{
    currentMeasurements = measurements;

    QString key = measurements["key"].toString();
    QJsonArray values = measurements["values"].toArray();

    QVariantList valuesList;

    for (const QJsonValue& value : values) {
        QJsonObject measurement = value.toObject();
        QString dateStr = measurement["date"].toString();
        QVariant valueVariant = measurement["value"].toVariant();

        QVariantMap point;
        point["date"] = dateStr;
        point["value"] = valueVariant.isNull() ? QVariant() : valueVariant.toDouble();
        valuesList.append(point);
    }

    // Aktualizowanie interfejsu z pomiarami
    emit measurementsUpdateRequested(key, valuesList);
}
