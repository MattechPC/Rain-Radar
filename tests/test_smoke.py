"""Lightweight tests for validation and RainViewer metadata handling."""
import importlib.util
import json
from pathlib import Path

PLUGIN_DIR = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("weather_radar_server", PLUGIN_DIR / "server.py")
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


def test_invalid_latitude_returns_actionable_error(tmp_path):
    result = server.fetch(
        {"latitude": 91, "longitude": 16.9, "zoom": 7},
        {},
        ctx={"data_dir": str(tmp_path)},
    )
    assert "error" in result
    assert "Szerokość" in result["error"]


def test_uses_cached_rainviewer_metadata(tmp_path, monkeypatch):
    payload = {
        "host": "https://tilecache.rainviewer.com",
        "radar": {"past": [{"time": 1770000000, "path": "/v2/radar/1770000000"}]},
    }
    (tmp_path / "rainviewer_metadata.json").write_text(json.dumps(payload), encoding="utf-8")
    monkeypatch.setattr(server.time, "time", lambda: 1770000100)
    result = server.fetch(
        {"latitude": 51.3, "longitude": 16.9, "zoom": 7, "title": "Radar"},
        {},
        ctx={"data_dir": str(tmp_path)},
    )
    assert result["error"] is None
    assert result["latitude"] == 51.3
    assert "/v2/radar/1770000000/256/{z}/{x}/{y}/2/1_0.png" in result["radar_tile_template"]
