#!/usr/bin/env python3
"""Pi 5 alerts + location REST API for the ESP32 dashboard (task 8).

Endpoints (JSON, "Authorization: Bearer <token>" required on every call)
  GET  /api/alerts/summary  {"unacked":3,"critical":1,"latest_id":128}
  GET  /api/alerts          {"items":[{"id","level","title","detail","ts","count","ack"}]}
                            at most 10, newest first, only active + unacked
  POST /api/alerts/ack-all  {"acked":N}
  GET  /api/location        {"city":"TP Dong Nai","lat":10.9574,"lon":106.8427}
  GET  /api/health          {"ok":true}  (System info screen: round-trip time)
  GET  /api/photos          {"rev":"a41f9c","count":24,"items":[{"id":"p001"}]}  (task 10)
  GET  /api/photos/<id>.jpg processed 480x272 baseline JPEG, always with Content-Length
Errors: 400 / 401 / 404 / 500 with {"error":"..."}.

Alerts live in SQLite. A rule engine checks the Pi every --interval seconds:
  critical  CPU temperature above temp_critical (70 C); a "critical" service stopped
  warning   RAM above ram_warning (90 %); any other listed service stopped;
            MQTT broker unreachable
  info      the Pi 5 rebooted (boot time changed since the last run)
A repeat of the same source (same `key`) bumps `count` and `ts` instead of
adding a row; an alert closes by itself (active=0) when its condition clears,
so "unacked" is what is wrong right now and not yet cleared on the screen.

Config: a JSON file (--config, re-read on every /api/location and every rule
pass, so editing it needs no restart). Missing keys use DEFAULT_CONFIG.
Token: env ALERTS_API_TOKEN (required). Plain HTTP: bind to the LAN address
only and firewall the port from outside; the token travels in clear text.

Photos (task 10): set "photos_dir" in the config; see photos.py for the
processing and the other photos_* keys. Needs Pillow; without it the list is empty.

Setup on the Pi:
  pip install psutil pillow                # rules + photos (use a venv on Bookworm)
  export ALERTS_API_TOKEN=$(openssl rand -hex 24)
  sudo -E python3 alerts_api.py --bind 192.168.1.50 --port 8080 \\
      --config /etc/esp32-dashboard/alerts.json --db /var/lib/esp32-dashboard/alerts.db
Try it:
  curl -H "Authorization: Bearer $ALERTS_API_TOKEN" http://192.168.1.50:8080/api/alerts/summary
"""
import argparse
import hmac
import json
import os
import socket
import sqlite3
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import photos

MAX_ITEMS = 10
TITLE_MAX = 40
DETAIL_MAX = 60
LEVELS = ("critical", "warning", "info")

DEFAULT_CONFIG = {
    "city": "TP Dong Nai",          # ASCII, no diacritics (the ESP32 font has none)
    "lat": 10.9574,                 # TODO: confirm the point used for the weather
    "lon": 106.8427,
    "temp_critical": 70,            # deg C
    "ram_warning": 90,              # %
    # systemd units to watch: "critical" or "warning" when stopped
    "services": {"mosquitto": "critical", "nginx": "warning", "docker": "warning", "node-red": "warning"},
    "mqtt_host": "localhost",
    "mqtt_port": 1883,
}


# ---------------------------------------------------------------- config

def load_config(path):
    cfg = dict(DEFAULT_CONFIG)
    if path:
        try:
            with open(path, encoding="utf-8") as f:
                cfg.update(json.load(f))
        except FileNotFoundError:
            pass
        except (OSError, ValueError) as e:
            print(f"config {path}: {e}; using defaults", file=sys.stderr)
    return cfg


def location_payload(cfg):
    """Validated /api/location body, or None if the config is unusable."""
    try:
        city = str(cfg["city"]).strip()
        lat, lon = float(cfg["lat"]), float(cfg["lon"])
    except (KeyError, TypeError, ValueError):
        return None
    if not city or not (-90 <= lat <= 90) or not (-180 <= lon <= 180):
        return None
    return {"city": city, "lat": round(lat, 4), "lon": round(lon, 4)}


# ---------------------------------------------------------------- store

def clip(text, n):
    text = " ".join(str(text or "").split())
    return text if len(text) <= n else text[: n - 1].rstrip() + "…"


class AlertStore:
    """SQLite alert table. Thread-safe (one connection, one lock)."""

    def __init__(self, path=":memory:", clock=time.time):
        self._db = sqlite3.connect(path, check_same_thread=False)
        self._lock = threading.Lock()
        self._clock = clock
        with self._lock, self._db:
            self._db.execute(
                """CREATE TABLE IF NOT EXISTS alerts (
                       id     INTEGER PRIMARY KEY AUTOINCREMENT,
                       key    TEXT NOT NULL,
                       level  TEXT NOT NULL,
                       title  TEXT NOT NULL,
                       detail TEXT NOT NULL DEFAULT '',
                       ts     INTEGER NOT NULL,
                       count  INTEGER NOT NULL DEFAULT 1,
                       ack    INTEGER NOT NULL DEFAULT 0,
                       active INTEGER NOT NULL DEFAULT 1)""")
            self._db.execute("CREATE INDEX IF NOT EXISTS alerts_open ON alerts(ack, active, ts)")
            self._db.execute("CREATE TABLE IF NOT EXISTS meta (k TEXT PRIMARY KEY, v TEXT)")

    # --- rule engine side
    def raise_alert(self, key, level, title, detail="", one_shot=False):
        """Condition `key` is true now. Returns the row id.

        Still active: refresh the detail only (a condition that stays true is one
        alert, not a repeat). Closed but not yet cleared on the screen: a repeat,
        count + 1, new ts (and a new id). Otherwise a new row. `one_shot` alerts
        (events, not conditions) always count as a repeat."""
        level = level if level in LEVELS else "info"
        title, detail = clip(title, TITLE_MAX), clip(detail, DETAIL_MAX)
        now = int(self._clock())
        with self._lock, self._db:
            row = self._db.execute(
                "SELECT id, active FROM alerts WHERE key=? AND ack=0 ORDER BY id DESC LIMIT 1", (key,)).fetchone()
            if row and row[1] and not one_shot:
                self._db.execute("UPDATE alerts SET level=?, title=?, detail=? WHERE id=?",
                                 (level, title, detail, row[0]))
                return row[0]
            count = 1
            if row:
                # A repeat moves to a fresh id so the summary's latest_id moves
                # and the ESP32 reloads its list.
                count += self._db.execute("SELECT count FROM alerts WHERE id=?", (row[0],)).fetchone()[0]
                self._db.execute("DELETE FROM alerts WHERE id=?", (row[0],))
            cur = self._db.execute(
                "INSERT INTO alerts (key, level, title, detail, ts, count) VALUES (?, ?, ?, ?, ?, ?)",
                (key, level, title, detail, now, count))
            return cur.lastrowid

    def resolve(self, key):
        """Condition `key` is false now: close its alert (it leaves the list)."""
        with self._lock, self._db:
            self._db.execute("UPDATE alerts SET active=0 WHERE key=? AND active=1", (key,))

    def get_meta(self, k, default=None):
        with self._lock:
            row = self._db.execute("SELECT v FROM meta WHERE k=?", (k,)).fetchone()
        return row[0] if row else default

    def set_meta(self, k, v):
        with self._lock, self._db:
            self._db.execute("INSERT OR REPLACE INTO meta (k, v) VALUES (?, ?)", (k, str(v)))

    # --- API side
    def summary(self):
        with self._lock:
            unacked, critical = self._db.execute(
                "SELECT COUNT(*), COALESCE(SUM(level='critical'), 0) FROM alerts WHERE ack=0 AND active=1").fetchone()
            latest = self._db.execute(
                "SELECT COALESCE(MAX(id), 0) FROM alerts WHERE ack=0 AND active=1").fetchone()[0]
        return {"unacked": unacked, "critical": critical, "latest_id": latest}

    def items(self, limit=MAX_ITEMS):
        limit = max(0, min(int(limit), MAX_ITEMS))   # never more than 10, whatever is asked
        with self._lock:
            rows = self._db.execute(
                "SELECT id, level, title, detail, ts, count FROM alerts WHERE ack=0 AND active=1 "
                "ORDER BY ts DESC, id DESC LIMIT ?", (limit,)).fetchall()
        return [{"id": r[0], "level": r[1], "title": r[2], "detail": r[3], "ts": r[4], "count": r[5], "ack": False}
                for r in rows]

    def ack_all(self):
        with self._lock, self._db:
            return self._db.execute("UPDATE alerts SET ack=1 WHERE ack=0").rowcount


# ---------------------------------------------------------------- rules

def evaluate(store, cfg, stats, services, mqtt_ok, boot_time):
    """One rule pass. stats: {"temp", "ram"}; services: {name: running(bool)}."""
    temp_lim = float(cfg.get("temp_critical", 70))
    if stats.get("temp") is not None and stats["temp"] > temp_lim:
        store.raise_alert("temp_high", "critical", "Pi 5 temperature high",
                          f"{stats['temp']:.0f}°C · limit {temp_lim:.0f}°C")
    else:
        store.resolve("temp_high")

    ram_lim = float(cfg.get("ram_warning", 90))
    if stats.get("ram") is not None and stats["ram"] > ram_lim:
        store.raise_alert("ram_high", "warning", "RAM usage high", f"{stats['ram']:.0f}% · limit {ram_lim:.0f}%")
    else:
        store.resolve("ram_high")

    for name, level in (cfg.get("services") or {}).items():
        key = "svc:" + name
        if services.get(name) is False:
            store.raise_alert(key, "critical" if level == "critical" else "warning", f"{name} stopped", "systemd unit inactive")
        else:
            store.resolve(key)

    if mqtt_ok is False:
        store.raise_alert("mqtt_down", "warning", "MQTT broker unreachable",
                          f"{cfg.get('mqtt_host')}:{cfg.get('mqtt_port')}")
    else:
        store.resolve("mqtt_down")

    if boot_time:
        last = store.get_meta("boot_time")
        if last is not None and abs(float(last) - boot_time) > 60:
            store.raise_alert("reboot", "info", "Pi 5 rebooted",
                              time.strftime("Up since %H:%M %d/%m", time.localtime(boot_time)), one_shot=True)
        store.set_meta("boot_time", int(boot_time))


def tcp_ok(host, port, timeout=2.0):
    try:
        with socket.create_connection((host, int(port)), timeout=timeout):
            return True
    except OSError:
        return False


def rule_loop(store, config_path, interval, stop):
    import psutil

    from pi5_stats_publisher import read_temp_c, status_from_is_active, unit_name, _run

    while not stop.is_set():
        cfg = load_config(config_path)
        try:
            stats = {"temp": read_temp_c(psutil), "ram": psutil.virtual_memory().percent}
            services = {n: status_from_is_active(_run(["systemctl", "is-active", unit_name(n)])) == "running"
                        for n in (cfg.get("services") or {})}
            mqtt = tcp_ok(cfg.get("mqtt_host", "localhost"), cfg.get("mqtt_port", 1883))
            evaluate(store, cfg, stats, services, mqtt, psutil.boot_time())
        except Exception as e:   # a broken rule pass must not kill the API
            print(f"rule pass failed: {e}", file=sys.stderr)
        stop.wait(interval)


# ---------------------------------------------------------------- HTTP

def make_handler(store, token, config_path, library=None):
    expected = ("Bearer " + token).encode()

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"    # keep-alive: the ESP32 reuses its connection
        server_version = "alerts-api/1"

        def log_message(self, fmt, *args):   # quieter than the default
            if os.getenv("ALERTS_API_DEBUG"):
                super().log_message(fmt, *args)

        def _send(self, code, obj):
            body = json.dumps(obj, separators=(",", ":"), ensure_ascii=True).encode()
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def _send_jpeg(self, data, pid):
            self.send_response(200)
            self.send_header("Content-Type", "image/jpeg")
            self.send_header("Content-Length", str(len(data)))   # the ESP32 sizes its read from this
            self.send_header("Cache-Control", "max-age=3600")
            self.send_header("ETag", '"%s-%d"' % (pid, len(data)))
            self.end_headers()
            self.wfile.write(data)

        def _authorized(self):
            got = (self.headers.get("Authorization") or "").encode()
            if hmac.compare_digest(got, expected):
                return True
            self._send(401, {"error": "bad or missing token"})
            return False

        def _drain(self):
            n = int(self.headers.get("Content-Length") or 0)
            if n:
                self.rfile.read(min(n, 4096))

        def do_GET(self):
            if not self._authorized():
                return
            path = self.path.split("?", 1)[0]
            try:
                if path == "/api/health":
                    self._send(200, {"ok": True})
                elif path == "/api/alerts/summary":
                    self._send(200, store.summary())
                elif path == "/api/alerts":
                    self._send(200, {"items": store.items()})
                elif path == "/api/photos":
                    self._send(200, library.listing() if library else {"rev": "0", "count": 0, "items": []})
                elif path.startswith("/api/photos/") and path.endswith(".jpg"):
                    pid = path[len("/api/photos/"):-len(".jpg")]
                    data = library.image(pid) if library else None
                    if data is None:
                        self._send(404, {"error": "no such photo"})
                    else:
                        self._send_jpeg(data, pid)
                elif path == "/api/location":
                    loc = location_payload(load_config(config_path))
                    if loc is None:
                        self._send(500, {"error": "location not configured"})
                    else:
                        self._send(200, loc)
                else:
                    self._send(404, {"error": "not found"})
            except Exception as e:
                self._send(500, {"error": str(e)[:100]})

        def do_POST(self):
            self._drain()
            if not self._authorized():
                return
            if self.path.split("?", 1)[0] == "/api/alerts/ack-all":
                try:
                    self._send(200, {"acked": store.ack_all()})
                except Exception as e:
                    self._send(500, {"error": str(e)[:100]})
            else:
                self._send(404, {"error": "not found"})

    return Handler


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--bind", default=os.getenv("ALERTS_API_BIND", "127.0.0.1"),
                   help="address to listen on: the Pi's LAN IP, never 0.0.0.0 on a public interface")
    p.add_argument("--port", type=int, default=int(os.getenv("ALERTS_API_PORT", "8080")))
    p.add_argument("--db", default=os.getenv("ALERTS_API_DB", "alerts.db"))
    p.add_argument("--config", default=os.getenv("ALERTS_API_CONFIG"))
    p.add_argument("--interval", type=float, default=10.0, help="rule pass period (s)")
    p.add_argument("--no-rules", action="store_true", help="API only (testing)")
    p.add_argument("--photo-scan", type=float, default=60.0, help="photo folder rescan period (s)")
    args = p.parse_args()

    token = os.getenv("ALERTS_API_TOKEN", "")
    if len(token) < 16:
        sys.exit("set ALERTS_API_TOKEN (at least 16 characters, e.g. `openssl rand -hex 24`)")

    store = AlertStore(args.db)
    stop = threading.Event()
    if not args.no_rules:
        threading.Thread(target=rule_loop, args=(store, args.config, args.interval, stop), daemon=True).start()

    library = photos.PhotoLibrary()
    try:
        import PIL  # noqa: F401
        threading.Thread(target=photos.scan_loop, daemon=True,
                         args=(library, lambda: load_config(args.config), args.photo_scan, stop)).start()
    except ImportError:
        print("Pillow not installed: /api/photos stays empty (pip install pillow)", file=sys.stderr)

    server = ThreadingHTTPServer((args.bind, args.port), make_handler(store, token, args.config, library))
    print(f"alerts API on http://{args.bind}:{args.port}", file=sys.stderr)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        server.server_close()


if __name__ == "__main__":
    main()
