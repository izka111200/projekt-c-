# Projekt_jpo
# Monitor Jakości Powietrza - GIOŚ

## Opis
Aplikacja desktopowa w Qt/QML do monitorowania jakości powietrza z API GIOŚ. Umożliwia przeglądanie stacji, czujników, pomiarów, wykresów, statystyk oraz zapis i import danych (JSON/XML).

## Wymagania
- Qt 5.15+ lub 6.x
- Moduły: Qt Quick, Qt Charts, Qt Network, Qt Xml
- C++11+
- Połączenie internetowe

## Instalacja
1. Sklonuj repozytorium: `git clone <adres-repozytorium>`
2. Otwórz projekt w Qt Creator
3. Skompiluj i uruchom: `qmake && make && ./MonitorJakosciPowietrza`

## Użycie
- Wyszukaj i wybierz stację/czujnik
- Przeglądaj pomiary na wykresach/tabelach
- Korzystaj z danych historycznych i importu (JSON/XML)
- Autosave co 60 sekund
