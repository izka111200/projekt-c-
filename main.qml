import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtCharts 2.15
import Qt.labs.platform 1.1 as Platform

ApplicationWindow {
    id: root
    visible: true
    width: 1200
    height: 800
    title: "Monitor Jakości Powietrza - GIOŚ"
    color: "#f0f4f8"

    property var currentStation: null
    property var currentSensor: null
    property int currentSensorId: 0
    property real minValue: 0
    property real maxValue: 100
    property real avgValue: 0

    // Zaktualizowana paleta kolorów
    property color primaryColor: "#ffb6c1" // Pudrowy róż
    property color accentColor: "#dda0dd"  // Delikatny fiolet
    property color textColor: "#263238"
    property color lightTextColor: "#ffffff"
    property color cardBackground: "#ffffff"
    property color borderColor: "#e0e6ed"
    property color chartLineColor: "#ff9999" // Jasny róż dla linii wykresu
    property color chartFillColor: "#ff999922" // Jasny róż z przezroczystością
    property color errorColor: "#d32f2f"
    property color successColor: "#388e3c"

    Component.onCompleted: {
        mainWindow.stationsUpdateRequested.connect(onStationsUpdate);
        mainWindow.stationInfoUpdateRequested.connect(updateStationInfo);
        mainWindow.sensorsUpdateRequested.connect(onSensorsUpdate);
        mainWindow.measurementsUpdateRequested.connect(setMeasurementData);
        mainWindow.historicalDataListUpdated.connect(updateHistoricalDataList);
    }

    Platform.FileDialog {
        id: saveFileDialog
        title: "Zapisz dane"
        folder: Platform.StandardPaths.writableLocation(Platform.StandardPaths.DocumentsLocation)
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: ["JSON (*.json)", "XML (*.xml)", "Baza lokalna (*.db)"]
        onAccepted: {
            var path = file.toString().replace(/^(file:\/{2})/, "");
            if (Qt.platform.os === "windows") path = path.replace(/^\//, "");
            var format = selectedNameFilter.match(/.+\(\*\.(\w+)\)/)[1];
            mainWindow.saveDataToFile(currentSensorId, format);
            showNotification("Dane zapisane", false);
        }
    }

    Popup {
        id: notification
        width: notificationText.width + 60
        height: 50
        x: (parent.width - width) / 2
        y: parent.height - height - 20
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property bool isError: false

        background: Rectangle {
            color: notification.isError ? errorColor : successColor
            radius: 8
        }

        contentItem: RowLayout {
            spacing: 10

            Text {
                text: notification.isError ? "⚠️" : "✓"
                font.pixelSize: 20
                color: lightTextColor
            }

            Text {
                id: notificationText
                text: "Powiadomienie"
                font.pixelSize: 14
                color: lightTextColor
            }
        }

        Timer {
            id: notificationTimer
            interval: 3000
            onTriggered: notification.close()
        }
    }

    function showNotification(message, isError) {
        notificationText.text = message;
        notification.isError = isError;
        notification.open();
        notificationTimer.restart();
    }

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
                text: "Importuj z pliku"
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
                    color: mouseArea.containsMouse ? "#f3e5f5" : "transparent" // Zmiana koloru tła przy najechaniu
                    radius: 4

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            mainWindow.loadHistoricalData(currentSensorId, model.filename);
                            historicalDataDialog.close();
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Text {
                            text: model.display
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            font.pixelSize: 14
                            color: textColor
                        }

                        Button {
                            text: "🗑"
                            width: 30
                            onClicked: {
                                confirmDeleteDialog.filenameToDelete = model.filename;
                                confirmDeleteDialog.open();
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    active: true
                }
            }

            Label {
                text: "Brak danych historycznych"
                font.pixelSize: 14
                color: textColor
                Layout.alignment: Qt.AlignCenter
                visible: historicalDataModel.count === 0
            }
        }

        onOpened: {
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

    Platform.FileDialog {
        id: importFileDialog
        title: "Importuj dane"
        folder: Platform.StandardPaths.writableLocation(Platform.StandardPaths.DocumentsLocation)
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: ["JSON (*.json)", "XML (*.xml)", "Baza lokalna (*.db)"]
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

    Dialog {
        id: confirmDeleteDialog
        title: "Potwierdzenie"
        modal: true
        width: 350
        height: 120
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.No

        property string filenameToDelete: ""

        onAccepted: {
            mainWindow.deleteHistoricalData(filenameToDelete);
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

        contentItem: Label {
            text: "Czy usunąć dane historyczne?"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 14
            color: textColor
        }
    }

    Dialog {
        id: connectionErrorDialog
        title: "Brak połączenia"
        modal: true
        width: 350
        height: 150
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: historicalDataDialog.open()
        onRejected: mainWindow.retryConnection()

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

    Rectangle {
        anchors.fill: parent
        color: root.color

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

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
                                text: "🔍"
                                width: 30
                                onClicked: mainWindow.searchStations(searchField.text)
                            }
                        }
                    }

                    Button {
                        text: "Wszystkie stacje"
                        height: 36
                        palette.button: accentColor
                        palette.buttonText: lightTextColor
                        onClicked: mainWindow.showAllStations()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

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
                                color: mouseArea.containsMouse ? "#f3e5f5" : "transparent" // Zmiana koloru tła przy najechaniu

                                MouseArea {
                                    id: mouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        currentStation = model.station;
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

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

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

                                Row {
                                    spacing: 8

                                    Button {
                                        text: "Zapisz dane"
                                        height: 36
                                        enabled: currentSensor !== null
                                        palette.button: enabled ? accentColor : "#cccccc"
                                        palette.buttonText: textColor // Zmiana koloru tekstu na kontrastujący
                                        onClicked: saveFileDialog.open()
                                    }

                                    Button {
                                        text: "Dane historyczne"
                                        height: 36
                                        enabled: currentSensor !== null
                                        palette.button: enabled ? primaryColor : "#cccccc"
                                        palette.buttonText: textColor // Zmiana koloru tekstu na kontrastujący
                                        onClicked: historicalDataDialog.open()
                                    }
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

                            ChartView {
                                id: chartView
                                Layout.fillWidth: true
                                Layout.preferredHeight: 250
                                antialiasing: true
                                legend.visible: false
                                backgroundColor: cardBackground

                                DateTimeAxis {
                                    id: axisX
                                    format: "dd.MM HH:mm"
                                    tickCount: 5
                                    labelsColor: textColor
                                    gridLineColor: borderColor
                                    labelsVisible: true // Upewniamy się, że etykiety osi X są widoczne
                                }

                                ValueAxis {
                                    id: axisY
                                    min: Math.min(minValue - (maxValue - minValue) * 0.1, 0) // Zapewniamy, że oś Y zaczyna się od 0 lub mniej
                                    max: maxValue + (maxValue - minValue) * 0.1
                                    labelFormat: "%.1f"
                                    labelsColor: textColor
                                    gridLineColor: borderColor
                                    labelsVisible: true // Upewniamy się, że etykiety osi Y są widoczne
                                }

                                AreaSeries {
                                    id: areaSeries
                                    axisX: axisX
                                    axisY: axisY
                                    color: chartLineColor
                                    borderWidth: 2
                                    borderColor: chartLineColor
                                    upperSeries: LineSeries { id: upperSeries }
                                    lowerSeries: LineSeries { id: lowerSeries }
                                    opacity: 0.3
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 60
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
                                            text: `Min: ${minValue.toFixed(1)} | Max: ${maxValue.toFixed(1)} | Śr: ${avgValue.toFixed(1)}`
                                            font.pixelSize: 12
                                            color: textColor
                                        }
                                    }
                                }
                            }

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

    function clearStationsList() {
        stationsModel.clear();
    }

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

    function updateStationInfo(stationId, stationName, addressStreet, city, lat, lon) {
        currentStation = {
            "stationId": stationId,
            "stationName": stationName,
            "addressStreet": addressStreet,
            "city": city,
            "gegrLat": lat,
            "gegrLon": lon
        };
    }

    function clearSensorsList() {
        sensorsModel.clear();
    }

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

    function clearMeasurementData() {
        upperSeries.clear();
        lowerSeries.clear();
        minValue = 0;
        maxValue = 100;
        avgValue = 0;
    }

    function setMeasurementData(key, values) {
        clearMeasurementData();
        busyIndicator.running = false;

        if (!values || values.length === 0) {
            statusLabel.text = "Brak danych";
            statusIcon.text = "⚠️";
            statusIcon.visible = true;
            return;
        }

        var unit = currentSensor ? currentSensor.paramFormula : "";
        var minVal = Number.MAX_VALUE;
        var maxVal = Number.MIN_VALUE;
        var sum = 0;
        var count = 0;

        for (var i = 0; i < values.length; i++) {
            var dateStr = values[i].date;
            var value = parseFloat(values[i].value);

            if (isNaN(value)) continue;

            minVal = Math.min(minVal, value);
            maxVal = Math.max(maxVal, value);
            sum += value;
            count++;

            var date = new Date(dateStr);
            upperSeries.append(date.getTime(), value);
            lowerSeries.append(date.getTime(), 0);
        }

        if (count === 0) {
            statusLabel.text = "Brak ważnych danych";
            statusIcon.text = "⚠️";
            statusIcon.visible = true;
            return;
        }

        minValue = minVal;
        maxValue = maxVal;
        avgValue = sum / count;

        // Ustawiamy minimalną i maksymalną wartość osi Y, aby wykres był zawsze widoczny
        axisY.min = Math.min(minValue - (maxValue - minValue) * 0.1, 0);
        axisY.max = maxValue + (maxValue - minValue) * 0.1;

        // Aktualizujemy etykiety danych
        dataRangeLabel.text = `Zakres: ${new Date(upperSeries.at(0).x).toLocaleString(Qt.locale(), "dd.MM.yyyy HH:mm")} - ${new Date(upperSeries.at(upperSeries.count - 1).x).toLocaleString(Qt.locale(), "dd.MM.yyyy HH:mm")}`;
        dataAnalysisLabel.text = `Min: ${minValue.toFixed(1)} ${unit} | Max: ${maxValue.toFixed(1)} ${unit} | Śr: ${avgValue.toFixed(1)} ${unit}`;

        statusLabel.text = "Dane wczytane";
        statusIcon.text = "✓";
        statusIcon.visible = true;
    }

    function onStationsUpdate(stations) {
        clearStationsList();
        for (var i = 0; i < stations.length; i++) {
            addStation(stations[i].id, stations[i].name, stations[i].city);
        }
    }

    function onSensorsUpdate(sensors) {
        clearSensorsList();
        for (var i = 0; i < sensors.length; i++) {
            addSensor(sensors[i].id, sensors[i].param, sensors[i].code, "");
        }
    }

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
    }

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
    }
}
