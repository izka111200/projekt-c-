/**
 * @file main.qml
 * @brief Główny plik QML aplikacji Monitor Jakości Powietrza, definiujący interfejs użytkownika.
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtCharts 2.15
import Qt.labs.platform 1.1 as Platform

/**
 * @brief Główny komponent okna aplikacji.
 * Zarządza interfejsem użytkownika, wykresami, danymi stacji i czujników.
 */
ApplicationWindow {
    id: root
    visible: true
    width: 1200
    height: 800
    title: "Monitor Jakości Powietrza - GIOŚ"
    color: "#f0f4f7" //!< Kolor tła okna

    // Właściwości przechowujące stan aplikacji
    property var currentStation: null //!< Aktualnie wybrana stacja
    property var currentSensor: null  //!< Aktualnie wybrany czujnik
    property int currentSensorId: 0  //!< ID wybranego czujnika
    property real minValue: 0        //!< Minimalna wartość pomiaru
    property real maxValue: 100      //!< Maksymalna wartość pomiaru
    property real avgValue: 0        //!< Średnia wartość pomiaru
    property real stdDevValue: 0     //!< Odchylenie standardowe
    property var measurementData: []  //!< Dane pomiarowe do wykresu

    // Kolory używane w interfejsie
    property color primaryColor: "#98FB98"   //!< Główny kolor (zielony)
    property color accentColor: "#dda0dd"    //!< Akcent (fioletowy)
    property color textColor: "#263238"      //!< Kolor tekstu
    property color lightTextColor: "#ffffff" //!< Jasny tekst
    property color cardBackground: "#ffffff" //!< Tło kart
    property color borderColor: "#e0e6ed"    //!< Kolor obramowania
    property color chartLineColor: "#ff5555" //!< Kolor linii wykresu
    property color chartFillColor: "#ff999922" //!< Wypełnienie wykresu
    property color errorColor: "#d32f2f"     //!< Kolor błędu
    property color successColor: "#388e3c"   //!< Kolor sukcesu

    /**
     * @brief Inicjalizuje połączenia sygnałów z C++ do funkcji QML po załadowaniu komponentu.
     */
    Component.onCompleted: {
        mainWindow.stationsUpdateRequested.connect(onStationsUpdate);
        mainWindow.stationInfoUpdateRequested.connect(updateStationInfo);
        mainWindow.sensorsUpdateRequested.connect(onSensorsUpdate);
        mainWindow.measurementsUpdateRequested.connect(setMeasurementData);
        mainWindow.historicalDataListUpdated.connect(updateHistoricalDataList);
        mainWindow.statisticsUpdated.connect(updateStatistics);
        console.log("QML initialized, signal connections set up");
    }

    /**
     * @brief Powiadomienie wyskakujące na dole ekranu.
     */
    Popup {
        id: notification
        width: Math.min(notificationText.width + 60, 400)
        height: 50
        x: (parent.width - width) / 2
        y: parent.height - height - 20
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property bool isError: false //!< Czy powiadomienie to błąd

        background: Rectangle {
            color: notification.isError ? errorColor : successColor
            radius: 8
        }

        contentItem: RowLayout {
            spacing: 10
            anchors.centerIn: parent

            Text {
                text: notification.isError ? "⚠️" : "✓" //!< Ikona błędu lub sukcesu
                font.pixelSize: 20
                color: lightTextColor
            }

            Text {
                id: notificationText
                text: "Powiadomienie"
                font.pixelSize: 14
                color: lightTextColor
                wrapMode: Text.Wrap
                maximumLineCount: 2
            }
        }

        Timer {
            id: notificationTimer
            interval: 3000 //!< Zamyka powiadomienie po 3 sekundach
            onTriggered: notification.close()
        }
    }

    /**
     * @brief Wyświetla powiadomienie z komunikatem.
     * @param message Tekst powiadomienia.
     * @param isError Czy powiadomienie oznacza błąd.
     */
    function showNotification(message, isError) {
        notificationText.text = message;
        notification.isError = isError;
        notification.open();
        notificationTimer.restart();
    }

    /**
     * @brief Okno dialogowe do przeglądania danych historycznych.
     */
    Dialog {
        id: historicalDataDialog
        title: "Dane historyczne"
        modal: true
        width: 400
        height: 350
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Close

        background: Rectangle {
            color: cardBackground
            radius: 8
            border.color: borderColor
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Label {
                text: "Dane historyczne:"
                font.pixelSize: 14
                font.bold: true
                color: textColor
            }

            Button {
                text: "Importuj z pliku" //!< Przycisk do importu danych
                Layout.fillWidth: true
                height: 40
                palette.button: accentColor
                palette.buttonText: lightTextColor
                onClicked: importFileDialog.open()
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: borderColor
            }

            ListView {
                id: historicalDataList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: ListModel { id: historicalDataModel }

                delegate: Rectangle {
                    width: parent.width
                    height: 40
                    color: mouseArea.containsMouse ? "#f3e5f5" : "transparent"
                    radius: 4

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            console.log("Loading historical data for filename:", model.filename);
                            mainWindow.loadHistoricalData(currentSensorId, model.filename);
                            historicalDataDialog.close();
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Text {
                            text: model.display //!< Wyświetla datę
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            font.pixelSize: 14
                            color: textColor
                        }

                        Button {
                            text: "🗑" //!< Przycisk do usuwania danych
                            width: 30
                            onClicked: {
                                confirmDeleteDialog.filenameToDelete = model.filename;
                                confirmDeleteDialog.open();
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar { active: true }
            }

            Label {
                text: "Brak danych historycznych"
                font.pixelSize: 14
                color: textColor
                Layout.alignment: Qt.AlignCenter
                visible: historicalDataModel.count === 0
            }
        }

        /**
         * @brief Ładuje listę danych historycznych po otwarciu dialogu.
         */
        onOpened: {
            console.log("Historical data dialog opened for sensor ID:", currentSensorId);
            historicalDataModel.clear();
            var dataList = mainWindow.getAvailableHistoricalData(currentSensorId);
            for (var i = 0; i < dataList.length; i++) {
                var parts = dataList[i].split("|");
                if (parts.length === 2) {
                    historicalDataModel.append({
                        "display": parts[0],
                        "filename": parts[1]
                    });
                }
            }
            console.log("Loaded", dataList.length, "historical data entries");
        }
    }

    /**
     * @brief Okno dialogowe do wyboru pliku do importu.
     */
    Platform.FileDialog {
        id: importFileDialog
        title: "Importuj dane"
        folder: Platform.StandardPaths.writableLocation(Platform.StandardPaths.DocumentsLocation)
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: ["JSON (*.json)", "XML (*.xml)"]
        onAccepted: {
            var path = file.toString().replace(/^(file:\/{2})/, "");
            if (Qt.platform.os === "windows") path = path.replace(/^\//, "");
            var format = selectedNameFilter.match(/.+\(\*\.(\w+)\)/)[1];
            var success = mainWindow.importDataFromFile(path, format);
            showNotification(success ? "Dane zaimportowane" : "Błąd importu", !success);
            if (success) {
                historicalDataDialog.close();
                historicalDataDialog.open();
            }
        }
    }

    /**
     * @brief Okno dialogowe do potwierdzenia usunięcia danych.
     */
    Dialog {
        id: confirmDeleteDialog
        title: "Potwierdzenie"
        modal: true
        width: 350
        height: 120
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.No

        property string filenameToDelete: "" //!< Nazwa pliku do usunięcia

        onAccepted: {
            console.log("Deleting historical data for filename:", filenameToDelete);
            var success = mainWindow.deleteHistoricalData(filenameToDelete);
            showNotification(success ? "Dane usunięte" : "Błąd usuwania danych", !success);
            if (success) {
                historicalDataModel.clear();
                var dataList = mainWindow.getAvailableHistoricalData(currentSensorId);
                for (var i = 0; i < dataList.length; i++) {
                    var parts = dataList[i].split("|");
                    if (parts.length === 2) {
                        historicalDataModel.append({
                            "display": parts[0],
                            "filename": parts[1]
                        });
                    }
                }
            }
        }

        contentItem: Label {
            text: "Czy usunąć dane historyczne?"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 14
            color: textColor
        }
    }

    /**
     * @brief Okno dialogowe w przypadku braku połączenia z API.
     */
    Dialog {
        id: connectionErrorDialog
        title: "Brak połączenia"
        modal: true
        width: 350
        height: 150
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: historicalDataDialog.open() //!< Otwiera dane historyczne
        onRejected: mainWindow.retryConnection() //!< Ponawia połączenie

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text: "Problem z połączeniem do GIOŚ."
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 14
                color: textColor
                Layout.fillWidth: true
            }
            Label {
                text: "Użyć danych historycznych?"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 14
                color: textColor
                Layout.fillWidth: true
            }
        }
    }

    /**
     * @brief Główny układ interfejsu.
     */
    Rectangle {
        anchors.fill: parent
        color: root.color

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            // Pasek nagłówka z wyszukiwaniem
            Rectangle {
                Layout.fillWidth: true
                height: 60
                color: primaryColor
                radius: 8

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 12

                    Label {
                        text: "Monitor Jakości Powietrza"
                        font.pixelSize: 20
                        font.bold: true
                        color: lightTextColor
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredWidth: 300
                        height: 36
                        color: cardBackground
                        radius: 18
                        border.color: borderColor

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 8

                            TextField {
                                id: searchField
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                placeholderText: "Szukaj miejscowości..."
                                color: textColor
                                font.pixelSize: 14
                                background: Rectangle { color: "transparent" }
                                onAccepted: mainWindow.searchStations(text)
                            }

                            Button {
                                text: "🔍" //!< Przycisk wyszukiwania
                                width: 30
                                onClicked: mainWindow.searchStations(searchField.text)
                            }
                        }
                    }

                    Button {
                        text: "Wszystkie stacje" //!< Przycisk pokazujący wszystkie stacje
                        height: 36
                        palette.button: accentColor
                        palette.buttonText: lightTextColor
                        onClicked: mainWindow.showAllStations()
                    }
                }
            }

            // Główny układ z listą stacji i szczegółami
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // Lista stacji pomiarowych
                Rectangle {
                    Layout.preferredWidth: 250
                    Layout.fillHeight: true
                    radius: 8
                    color: cardBackground
                    border.color: borderColor

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Label {
                            text: "Stacje pomiarowe"
                            font.pixelSize: 16
                            font.bold: true
                            color: primaryColor
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: borderColor
                        }

                        ListView {
                            id: stationsList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 4
                            model: ListModel { id: stationsModel }

                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 40
                                radius: 4
                                color: mouseArea.containsMouse ? "#f3e5f5" : "transparent"

                                MouseArea {
                                    id: mouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        currentStation = model.station;
                                        console.log("Station selected, ID:", model.stationId);
                                        mainWindow.stationSelected(model.stationId);
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    Rectangle {
                                        width: 4
                                        height: 20
                                        radius: 2
                                        color: primaryColor
                                    }

                                    Label {
                                        text: model.display
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        font.pixelSize: 14
                                        color: textColor
                                    }
                                }
                            }

                            ScrollBar.vertical: ScrollBar { active: true }
                        }

                        Label {
                            text: "Brak stacji"
                            font.pixelSize: 14
                            color: textColor
                            Layout.alignment: Qt.AlignCenter
                            visible: stationsModel.count === 0
                        }
                    }
                }

                // Sekcja z informacjami o stacji i wykresem
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    // Informacje o wybranej stacji i czujnikach
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        radius: 8
                        color: cardBackground
                        border.color: borderColor

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Column {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Label {
                                        text: currentStation ? "Stacja: " + currentStation.stationName : "Wybierz stację"
                                        font.pixelSize: 16
                                        font.bold: true
                                        color: primaryColor
                                    }

                                    Label {
                                        text: currentStation ? "Adres: " + currentStation.addressStreet : ""
                                        visible: currentStation !== null
                                        font.pixelSize: 14
                                        color: textColor
                                    }

                                    Label {
                                        text: currentStation ? "Współrzędne: " + currentStation.gegrLat + ", " + currentStation.gegrLon : ""
                                        visible: currentStation !== null
                                        font.pixelSize: 14
                                        color: textColor
                                    }
                                }

                                Button {
                                    text: "Dane historyczne" //!< Przycisk do danych historycznych
                                    height: 36
                                    enabled: currentSensor !== null
                                    palette.button: enabled ? primaryColor : "#cccccc"
                                    palette.buttonText: textColor
                                    onClicked: historicalDataDialog.open()
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: borderColor
                            }

                            Label {
                                text: "Czujniki:"
                                font.pixelSize: 14
                                color: textColor
                                visible: currentStation !== null
                            }

                            ListView {
                                id: sensorsList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                orientation: ListView.Horizontal
                                spacing: 10
                                clip: true
                                visible: currentStation !== null
                                model: ListModel { id: sensorsModel }

                                delegate: Rectangle {
                                    width: 180
                                    height: 80
                                    radius: 6
                                    color: currentSensorId === model.sensorId ? primaryColor : cardBackground
                                    border.color: borderColor

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            currentSensor = model.sensor;
                                            currentSensorId = model.sensorId;
                                            console.log("Sensor clicked, ID:", model.sensorId);
                                            mainWindow.sensorSelected(model.sensorId);
                                        }
                                    }

                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 4

                                        Text {
                                            text: model.paramName
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: currentSensorId === model.sensorId ? lightTextColor : primaryColor
                                        }

                                        Text {
                                            text: "ID: " + model.sensorId
                                            font.pixelSize: 12
                                            color: currentSensorId === model.sensorId ? lightTextColor : textColor
                                        }
                                    }
                                }

                                ScrollBar.horizontal: ScrollBar { active: true }
                            }

                            Label {
                                text: "Brak czujników"
                                font.pixelSize: 14
                                color: textColor
                                Layout.alignment: Qt.AlignCenter
                                visible: currentStation !== null && sensorsModel.count === 0
                            }
                        }
                    }

                    // Sekcja z wykresem i danymi surowymi
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        color: cardBackground
                        border.color: borderColor

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            Label {
                                text: currentSensor ? "Pomiary - " + currentSensor.paramName : "Wybierz czujnik"
                                font.pixelSize: 16
                                font.bold: true
                                color: primaryColor
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: borderColor
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 12

                                // Wykres pomiarów
                                ChartView {
                                    id: chartView
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    antialiasing: true
                                    legend.visible: false
                                    backgroundColor: cardBackground
                                    animationOptions: ChartView.SeriesAnimations

                                    DateTimeAxis {
                                        id: axisX
                                        format: "dd.MM HH:mm"
                                        tickCount: 5
                                        labelsColor: textColor
                                        gridLineColor: borderColor
                                        labelsFont.pixelSize: 12
                                        titleText: "Czas"
                                        titleFont.pixelSize: 14
                                        titleFont.bold: true
                                    }

                                    ValueAxis {
                                        id: axisY
                                        min: 0
                                        max: 100
                                        labelFormat: "%.1f"
                                        labelsColor: textColor
                                        gridLineColor: borderColor
                                        labelsFont.pixelSize: 12
                                        titleText: currentSensor ? currentSensor.paramName + " (" + currentSensor.paramFormula + ")" : ""
                                        titleFont.pixelSize: 14
                                        titleFont.bold: true
                                    }

                                    LineSeries {
                                        id: lineSeries
                                        axisX: axisX
                                        axisY: axisY
                                        color: chartLineColor
                                        width: 3
                                        pointsVisible: true
                                        pointLabelsVisible: false
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onPositionChanged: {
                                            var point = chartView.mapToValue(Qt.point(mouse.x, mouse.y));
                                            var series = lineSeries;
                                            var index = -1;
                                            var minDist = Number.MAX_VALUE;

                                            for (var i = 0; i < series.count; i++) {
                                                var p = series.at(i);
                                                var dist = Math.abs(p.x - point.x);
                                                if (dist < minDist) {
                                                    minDist = dist;
                                                    index = i;
                                                }
                                            }

                                            if (index >= 0) {
                                                var p = series.at(index);
                                                tooltip.text = `Data: ${new Date(p.x).toLocaleString(Qt.locale(), "dd.MM.yyyy HH:mm")}\nWartość: ${p.y.toFixed(1)} ${currentSensor ? currentSensor.paramFormula : ""}`;
                                                tooltip.x = mouse.x + 10;
                                                tooltip.y = mouse.y + 10;
                                                tooltip.visible = true;
                                            } else {
                                                tooltip.visible = false;
                                            }
                                        }
                                        onExited: tooltip.visible = false
                                    }

                                    Rectangle {
                                        id: tooltip
                                        color: "#ffffff"
                                        border.color: borderColor
                                        radius: 4
                                        width: tooltipText.width + 20
                                        height: tooltipText.height + 20
                                        visible: false

                                        Text {
                                            id: tooltipText
                                            anchors.centerIn: parent
                                            font.pixelSize: 12
                                            color: textColor
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }

                                // Tabela z danymi surowymi
                                Rectangle {
                                    Layout.preferredWidth: 300
                                    Layout.fillHeight: true
                                    color: cardBackground
                                    border.color: borderColor
                                    radius: 8

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 8

                                        Label {
                                            text: "Dane surowe"
                                            font.pixelSize: 14
                                            font.bold: true
                                            color: primaryColor
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 1
                                            color: borderColor
                                        }

                                        ListView {
                                            id: dataTable
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true
                                            model: ListModel { id: dataModel }

                                            delegate: Rectangle {
                                                width: parent.width
                                                height: 30
                                                color: index % 2 === 0 ? "#f8f9fb" : cardBackground

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 8
                                                    spacing: 8

                                                    Text {
                                                        text: model.date
                                                        Layout.preferredWidth: 150
                                                        font.pixelSize: 12
                                                        color: textColor
                                                        elide: Text.ElideRight
                                                    }

                                                    Text {
                                                        text: model.value !== null ? model.value.toFixed(1) : "Brak"
                                                        Layout.fillWidth: true
                                                        font.pixelSize: 12
                                                        color: textColor
                                                    }
                                                }
                                            }

                                            ScrollBar.vertical: ScrollBar { active: true }
                                        }
                                    }
                                }
                            }

                            // Pasek z informacjami o danych
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 80
                                color: "#f8f9fb"
                                radius: 4
                                border.color: borderColor

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 12

                                    Rectangle {
                                        width: 12
                                        height: 12
                                        color: chartLineColor
                                        radius: 6
                                    }

                                    Label {
                                        text: currentSensor ? currentSensor.paramName : ""
                                        font.pixelSize: 14
                                        color: textColor
                                    }

                                    Item { Layout.fillWidth: true }

                                    Column {
                                        spacing: 4
                                        visible: currentSensor !== null

                                        Label {
                                            id: dataRangeLabel
                                            text: "Zakres: --"
                                            font.pixelSize: 12
                                            color: textColor
                                        }

                                        Label {
                                            id: dataAnalysisLabel
                                            text: `Min: ${minValue.toFixed(1)} | Max: ${maxValue.toFixed(1)} | Śr: ${avgValue.toFixed(1)} | Std: ${stdDevValue.toFixed(1)}`
                                            font.pixelSize: 12
                                            color: textColor
                                        }
                                    }
                                }
                            }

                            // Pasek statusu
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                color: "#f5f5f5"
                                radius: 4
                                border.color: borderColor

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    Text {
                                        id: statusIcon
                                        text: "ℹ️"
                                        font.pixelSize: 16
                                        visible: false
                                    }

                                    Label {
                                        id: statusLabel
                                        text: "Gotowy"
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        font.pixelSize: 14
                                        color: textColor
                                    }

                                    Item {
                                        id: busyIndicator
                                        property bool running: false
                                        Layout.preferredWidth: 20
                                        Layout.preferredHeight: 20
                                        visible: running

                                        Text {
                                            anchors.centerIn: parent
                                            text: "⟳"
                                            font.pixelSize: 16
                                            color: primaryColor

                                            RotationAnimation {
                                                target: parent
                                                from: 0
                                                to: 360
                                                duration: 1000
                                                loops: Animation.Infinite
                                                running: busyIndicator.running
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /**
     * @brief Czyści listę stacji.
     */
    function clearStationsList() {
        stationsModel.clear();
        console.log("Stations list cleared");
    }

    /**
     * @brief Dodaje stację do listy.
     * @param stationId ID stacji.
     * @param stationName Nazwa stacji.
     * @param address Adres stacji.
     */
    function addStation(stationId, stationName, address) {
        stationsModel.append({
            "stationId": stationId,
            "display": stationName,
            "station": {
                "stationId": stationId,
                "stationName": stationName,
                "addressStreet": address,
                "gegrLat": "",
                "gegrLon": ""
            }
        });
    }

    /**
     * @brief Aktualizuje informacje o wybranej stacji.
     * @param stationId ID stacji.
     * @param stationName Nazwa stacji.
     * @param addressStreet Adres ulicy.
     * @param city Miasto.
     * @param lat Szerokość geograficzna.
     * @param lon Długość geograficzna.
     */
    function updateStationInfo(stationId, stationName, addressStreet, city, lat, lon) {
        currentStation = {
            "stationId": stationId,
            "stationName": stationName,
            "addressStreet": addressStreet,
            "city": city,
            "gegrLat": lat,
            "gegrLon": lon
        };
        console.log("Station info updated, ID:", stationId);
    }

    /**
     * @brief Czyści listę czujników.
     */
    function clearSensorsList() {
        sensorsModel.clear();
        console.log("Sensors list cleared");
    }

    /**
     * @brief Dodaje czujnik do listy.
     * @param sensorId ID czujnika.
     * @param paramName Nazwa parametru.
     * @param paramCode Kod parametru.
     * @param paramFormula Formuła parametru.
     */
    function addSensor(sensorId, paramName, paramCode, paramFormula) {
        sensorsModel.append({
            "sensorId": sensorId,
            "paramName": paramName,
            "paramCode": paramCode,
            "paramFormula": paramFormula,
            "sensor": {
                "sensorId": sensorId,
                "paramName": paramName,
                "paramCode": paramCode,
                "paramFormula": paramFormula
            }
        });
    }

    /**
     * @brief Czyści dane pomiarowe.
     */
    function clearMeasurementData() {
        lineSeries.clear();
        dataModel.clear();
        measurementData = [];
        minValue = 0;
        maxValue = 100;
        avgValue = 0;
        stdDevValue = 0;
        axisX.min = new Date();
        axisX.max = new Date();
        axisY.min = 0;
        axisY.max = 100;
        chartView.update();
        console.log("Measurement data cleared");
    }

    /**
     * @brief Ustawia dane pomiarowe na wykresie i w tabeli.
     * @param key Klucz danych.
     * @param values Wartości pomiarowe.
     */
    function setMeasurementData(key, values) {
        console.log("setMeasurementData called with key:", key, "values count:", values.length);
        clearMeasurementData();
        busyIndicator.running = false;

        if (!values || values.length === 0) {
            statusLabel.text = "Brak danych";
            statusIcon.text = "⚠️";
            statusIcon.visible = true;
            console.log("No data to display");
            return;
        }

        var unit = currentSensor ? currentSensor.paramFormula : "";
        var minVal = Number.MAX_VALUE;
        var maxVal = -Number.MAX_VALUE;
        var sum = 0;
        var count = 0;
        var minDate = null;
        var maxDate = null;

        measurementData = [];
        for (var i = 0; i < values.length; i++) {
            var dateStr = values[i].date;
            var value = values[i].value;

            if (value === null || isNaN(value) || value === undefined) {
                console.log("Skipping invalid value at index", i, ":", value);
                continue;
            }

            var date = new Date(dateStr);
            if (!date || isNaN(date.getTime())) {
                console.log("Invalid date at index", i, ":", dateStr);
                continue;
            }

            value = parseFloat(value);
            if (isNaN(value)) {
                console.log("Invalid parsed value at index", i, ":", value);
                continue;
            }

            minVal = Math.min(minVal, value);
            maxVal = Math.max(maxVal, value);
            sum += value;
            count++;

            if (!minDate || date < minDate) minDate = date;
            if (!maxDate || date > maxDate) maxDate = date;

            lineSeries.append(date.getTime(), value);
            dataModel.append({
                "date": date.toLocaleString(Qt.locale(), "dd.MM.yyyy HH:mm"),
                "value": value
            });
            measurementData.push({ date: date, value: value });
        }

        if (count === 0) {
            statusLabel.text = "Brak ważnych danych";
            statusIcon.text = "⚠️";
            statusIcon.visible = true;
            console.log("No valid data points to display");
            return;
        }

        minValue = minVal;
        maxValue = maxVal;
        avgValue = sum / count;

        // Dopasowuje osie wykresu
        if (count === 1) {
            axisY.min = minVal - 1;
            axisY.max = maxVal + 1;
            axisX.min = new Date(minDate.getTime() - 3600 * 1000); // 1 godzina przed
            axisX.max = new Date(maxDate.getTime() + 3600 * 1000); // 1 godzina po
        } else {
            axisY.min = Math.max(minVal - (maxVal - minVal) * 0.1, 0);
            axisY.max = maxVal + (maxVal - minVal) * 0.1;
            axisX.min = minDate;
            axisX.max = maxDate;
        }

        dataRangeLabel.text = `Zakres: ${minDate.toLocaleString(Qt.locale(), "dd.MM.yyyy HH:mm")} - ${maxDate.toLocaleString(Qt.locale(), "dd.MM.yyyy HH:mm")}`;
        dataAnalysisLabel.text = `Min: ${minValue.toFixed(1)} ${unit} | Max: ${maxValue.toFixed(1)} ${unit} | Śr: ${avgValue.toFixed(1)} ${unit} | Std: ${stdDevValue.toFixed(1)} ${unit}`;

        statusLabel.text = "Dane wczytane";
        statusIcon.text = "✓";
        statusIcon.visible = true;

        chartView.update();
        console.log("Chart updated with", count, "valid points, minVal:", minVal, "maxVal:", maxVal);
    }

    /**
     * @brief Aktualizuje listę stacji.
     * @param stations Lista stacji.
     */
    function onStationsUpdate(stations) {
        clearStationsList();
        for (var i = 0; i < stations.length; i++) {
            addStation(stations[i].id, stations[i].name, stations[i].city);
        }
        console.log("Stations updated:", stations.length);
    }

    /**
     * @brief Aktualizuje listę czujników.
     * @param sensors Lista czujników.
     */
    function onSensorsUpdate(sensors) {
        clearSensorsList();
        for (var i = 0; i < sensors.length; i++) {
            addSensor(sensors[i].id, sensors[i].param, sensors[i].code, sensors[i].code);
        }
        console.log("Sensors updated:", sensors.length);
    }

    /**
     * @brief Aktualizuje listę danych historycznych.
     * @param dataList Lista danych historycznych.
     */
    function updateHistoricalDataList(dataList) {
        historicalDataModel.clear();
        for (var i = 0; i < dataList.length; i++) {
            var parts = dataList[i].split("|");
            if (parts.length === 2) {
                historicalDataModel.append({
                    "display": parts[0],
                    "filename": parts[1]
                });
            }
        }
        console.log("Historical data list updated:", dataList.length);
    }

    /**
     * @brief Aktualizuje statystyki.
     * @param stats Obiekt ze statystykami (min, max, mean, stdDev, count).
     */
    function updateStatistics(stats) {
        if (stats.count > 0) {
            minValue = stats.min;
            maxValue = stats.max;
            avgValue = stats.mean;
            stdDevValue = stats.stdDev;
            var unit = currentSensor ? currentSensor.paramFormula : "";
            dataAnalysisLabel.text = `Min: ${minValue.toFixed(1)} ${unit} | Max: ${maxValue.toFixed(1)} ${unit} | Śr: ${avgValue.toFixed(1)} ${unit} | Std: ${stdDevValue.toFixed(1)} ${unit}`;
            console.log("Statistics updated: min=", minValue, "max=", maxValue, "mean=", avgValue, "stdDev=", stdDevValue);
        } else {
            console.log("No statistics available");
        }
    }

    /**
     * @brief Ustawia status połączenia.
     * @param online Czy połączenie jest aktywne.
     */
    function setConnectionStatus(online) {
        busyIndicator.running = !online;
        if (!online) {
            statusLabel.text = "Brak internetu";
            statusIcon.text = "⚠️";
            statusIcon.visible = true;
            connectionErrorDialog.open();
        } else {
            statusLabel.text = "Połączono z GIOŚ";
            statusIcon.text = "✓";
            statusIcon.visible = true;
        }
        console.log("Connection status:", online ? "Online" : "Offline");
    }
}
