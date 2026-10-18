"""RainViewer metadata fetcher for Tesserae weather_radar.

The client composes public raster tiles so it can adapt the crop to each cell's
actual aspect ratio. Tile hosts used by the browser are declared in plugin.json
as network capabilities if your Tesserae version enforces browser-side hosts;
the server itself only fetches api.rainviewer.com.
"""
import json
import time
import urllib.error
import urllib.request
from pathlib import Path

API_URL = "https://api.rainviewer.com/public/weather-maps.json"
USER_AGENT = "tesserae-weather-radar/0.1 (+https://docs.tesserae.ink/widgets/)"
CACHE_SECONDS = 300


def _get_json(url):
    request = urllib.request.Request(
        url,
        headers={"User-Agent": USER_AGENT, "Accept": "application/json"},
    )
    with urllib.request.urlopen(request, timeout=8) as response:
        return json.loads(response.read().decode("utf-8"))


def fetch(options: dict, settings: dict, *, ctx: dict) -> dict:
    try:
        lat = float(options.get("latitude", 51.30))
        lon = float(options.get("longitude", 16.90))
        zoom = int(float(options.get("zoom", 7)))
        title = str(options.get("title") or "Radar opadów")[:80]

        if not (-85.0511 <= lat <= 85.0511):
            return {"error": "Szerokość geograficzna musi być w zakresie -85,05…85,05."}
        if not (-180 <= lon <= 180):
            return {"error": "Długość geograficzna musi być w zakresie -180…180."}
        zoom = max(1, min(7, zoom))

        data_dir = Path(ctx.get("data_dir") or ".")
        data_dir.mkdir(parents=True, exist_ok=True)
        cache_path = data_dir / "rainviewer_metadata.json"
        payload = None

        try:
            if cache_path.exists() and time.time() - cache_path.stat().st_mtime < CACHE_SECONDS:
                payload = json.loads(cache_path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            payload = None

        if payload is None:
            try:
                payload = _get_json(API_URL)
                cache_path.write_text(json.dumps(payload), encoding="utf-8")
            except urllib.error.HTTPError as exc:
                if exc.code == 429:
                    return {"error": "RainViewer ograniczył chwilowo liczbę zapytań. Spróbuj później."}
                return {"error": "RainViewer chwilowo nie udostępnia danych radarowych."}
            except Exception:
                return {"error": "Nie można połączyć się z RainViewer. Sprawdź połączenie serwera."}

        frames = payload.get("radar", {}).get("past", [])
        if not frames:
            return {"error": "RainViewer nie zwrócił dostępnych klatek radarowych."}

        frame = frames[-1]
        host = str(payload.get("host", "https://tilecache.rainviewer.com")).rstrip("/")
        path = str(frame.get("path", ""))
        if not path.startswith("/"):
            return {"error": "RainViewer zwrócił nieprawidłową ścieżkę klatki."}

        return {
            "title": title,
            "latitude": lat,
            "longitude": lon,
            "zoom": zoom,
            "radar_tile_template": f"{host}{path}/256/{{z}}/{{x}}/{{y}}/2/1_0.png",
            "frame_time": int(frame.get("time", 0)),
            "attribution": "Weather data by RainViewer",
            "error": None
        }
    except (TypeError, ValueError):
        return {"error": "Współrzędne i przybliżenie muszą być liczbami."}
    except Exception:
        return {"error": "Nie udało się przygotować radaru opadów."}
