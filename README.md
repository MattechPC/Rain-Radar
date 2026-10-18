# Weather Radar — wtyczka Tesserae

Statyczny widżet radarowy dla Tesserae. Pobiera metadane z publicznego API RainViewer i składa mapę z kafelków: jasny podkład CARTO bez etykiet, przezroczysta warstwa radarowa RainViewer oraz warstwa samych etykiet CARTO. Obraz jest komponowany w przeglądarce podczas renderowania widżetu, bez dodatkowych bibliotek Pythona.

## Opcje
- `latitude`, `longitude`: środek mapy, stopnie dziesiętne
- `zoom`: 1–7 (RainViewer ogranicza maksymalne powiększenie do 7)
- `title`: nagłówek

## Instalacja lokalna
1. Skopiuj katalog `weather_radar` do katalogu wtyczek Tesserae, zgodnie z metodą instalacji używaną w Twojej instancji.
2. Upewnij się, że instancja ma dostęp do `api.rainviewer.com`.
3. Widżet pobiera kafelki z:
   - `tilecache.rainviewer.com`
   - `basemaps.cartocdn.com`
4. Jeśli Twoja wersja Tesserae egzekwuje egress dla żądań wykonywanych przez serwer, dodaj do `requires` odpowiednie hosty dla połączeń serwerowych. `requires` w manifeście obecnie deklaruje `api.rainviewer.com`; przeglądarka renderująca musi również móc pobierać obrazki z hostów kafelków.
5. Przetestuj przez `/_test/render?plugin=weather_radar&size=md`.

## Ważne uwagi
- Ten projekt to punkt wyjścia i nie został uruchomiony na konkretnej instalacji Tesserae ani zweryfikowany na fizycznym ekranie ESP32.
- Klatki RainViewer są historycznymi/ostatnimi dostępnymi obserwacjami, nie prognozą. Dane mają zwykle interwał 10 minut.
- Wymagane jest oznaczenie źródła RainViewer. CARTO/OpenStreetMap mogą mieć dodatkowe warunki atrybucji; przed publikacją w katalogu sprawdź aktualne warunki wybranych kafelków.
- Przeglądarka renderująca musi zezwalać na CORS dla kafelków, aby móc narysować je na canvasie. W razie problemu można przenieść kompozycję obrazu na stronę serwera.
- Publiczne API RainViewer może się zmieniać i nie zapewnia SLA. Widżet buforuje metadane przez 5 minut, aby nie odpytwać API przy każdym renderze.

## Odświeżanie
Manifest nie wymusza cyklu co 10 minut samodzielnie. Ustaw odświeżanie dashboardu/urządzenia Tesserae na 10 minut w konfiguracji hosta. Widżet wyświetla tę wartość jako informację, nie jako mechanizm harmonogramu.
