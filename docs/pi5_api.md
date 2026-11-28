# Pi 5 REST API (alerts, location, photos)

Served by `tools/alerts_api.py` (Python stdlib + `psutil` for the rules, SQLite storage). Read by the
ESP32's `services/alerts_service.cpp` and `services/location_service.cpp` through `services/pi5_api.cpp`.

## Transport and auth

- Plain HTTP inside the LAN (HTTPS costs too much RAM on the ESP32). Bind to the Pi's LAN address only
  (`--bind 192.168.1.50`) and block the port from outside with the firewall. The token travels in clear
  text: use a random token made only for this API.
- Every request needs `Authorization: Bearer <token>`. Pi 5: env `ALERTS_API_TOKEN` (16+ characters).
  ESP32: `PI5_API_TOKEN` in `include/secrets.h`, with `PI5_API_HOST` / `PI5_API_PORT`.
- HTTP/1.1 keep-alive; the ESP32 reuses its connection (`setReuse(true)`), 5 s timeout.
- Status codes: 200 ok, 400 bad parameter, 401 bad token, 404 unknown path, 500 server error.
  Error body: `{"error":"..."}`.

## Endpoints

| Method | Path | ESP32 use | Response |
|---|---|---|---|
| GET | `/api/alerts/summary` | bell badge, every 20 s; on error 30 s → 60 s → … 5 min | `{"unacked":3,"critical":1,"latest_id":128}` |
| GET | `/api/alerts` | Alerts screen: on open, every 20 s while open, and when `latest_id` changes | `{"items":[...]}`, at most 10, newest first |
| POST | `/api/alerts/ack-all` | `Clear all`; the list is emptied only after a 200 | `{"acked":4}` |
| GET | `/api/location` | at boot, after each reconnect, every 5 min | `{"city":"TP Dong Nai","lat":10.9574,"lon":106.8427}` |
| GET | `/api/health` | System info screen (task 9): only while it is open and Wi-Fi is up, every 10 s, 3 s timeout; the round trip is shown as `OK • 12 ms` | `{"ok":true}` |

| GET | `/api/photos` | Photos tab: each time the tab opens and when the slideshow wraps to photo 1; 5 s timeout | `{"rev":"a41f9c","count":24,"items":[{"id":"p001"}]}` |
| GET | `/api/photos/{id}.jpg` | Photos tab: the shown photo and the next one (prefetch); 8 s timeout | 480x272 baseline JPEG, `Content-Length` always set, 404 if the id is gone |

`/api/health` needs the token like every other endpoint. A Pi 5 still running the task 8 version answers 404;
the ESP32 then times `GET /api/alerts/summary` instead until the next reboot.

`/api/alerts` item:

```json
{"id":128,"level":"critical","title":"Pi 5 temperature high","detail":"74°C · limit 70°C","ts":1764000000,"count":3,"ack":false}
```

- `level`: `critical`, `warning` or `info`; the ESP32 treats any other value as `info`.
- `ts`: epoch seconds, UTC. The ESP32 shows the age (`2 min`, `3 h`) from its NTP clock.
- `title` ≤ 40 characters, `detail` ≤ 60 (cut on the Pi 5). The ESP32 still keeps at most 10 items.
- Text should be ASCII without diacritics. The ESP32 keeps ASCII, `°` and `•` (`·` becomes `•`) and
  turns any other character into `?`.

## Alert lifecycle (Pi 5)

- The rule engine runs every 10 s (`--interval`). Each rule has a `key`:

  | Key | Level | Condition (thresholds in the config file) |
  |---|---|---|
  | `temp_high` | critical | CPU temperature > `temp_critical` (70 °C) |
  | `svc:<name>` | critical / warning | a unit in `services` is not `active` (its level comes from the config) |
  | `ram_high` | warning | RAM > `ram_warning` (90 %) |
  | `mqtt_down` | warning | TCP connect to `mqtt_host:mqtt_port` fails |
  | `reboot` | info | the boot time changed since the last run |

- A condition that stays true is one alert; only its `detail` is refreshed.
- When the condition clears, the alert closes (`active=0`) and leaves the list and the counts.
- When the same key fires again before a `Clear all`, it is a repeat: `count + 1`, a new `ts` and a
  new id, so `latest_id` moves and the ESP32 reloads its list.
- `Clear all` acks every row. A condition that is still true after the ack starts a new alert.
- `unacked` and the list both count only rows that are active and not acked.

## Location

`city`, `lat`, `lon` come from the config file (`--config`, example `tools/alerts.example.json`). The file
is read again on every request, so editing it needs no restart. The ESP32 then updates the Location row
and the Weather tab label, and reloads the weather for the new coordinates. If the Pi 5 can't be reached,
the ESP32 uses the copy it saved in NVS, or else `DEFAULT_CITY` / `DEFAULT_LAT` / `DEFAULT_LON` from
`include/app_config.h`.

## Try it

```sh
export ALERTS_API_TOKEN=$(openssl rand -hex 24)
sudo -E python3 tools/alerts_api.py --bind 192.168.1.50 --config alerts.json --db alerts.db
curl -H "Authorization: Bearer $ALERTS_API_TOKEN" http://192.168.1.50:8080/api/alerts/summary
curl -H "Authorization: Bearer $ALERTS_API_TOKEN" http://192.168.1.50:8080/api/alerts
curl -X POST -H "Authorization: Bearer $ALERTS_API_TOKEN" http://192.168.1.50:8080/api/alerts/ack-all
curl -H "Authorization: Bearer $ALERTS_API_TOKEN" http://192.168.1.50:8080/api/location
```

`--no-rules` serves the API without the rule engine, for testing on a machine that isn't a Pi.

## Photos (task 10)

Served by `tools/alerts_api.py` from `tools/photos.py` (needs Pillow; without it the list is empty).

- **The Pi 5 decides** which photos are shown and in what order; the ESP32 plays the list as received.
- `rev` changes when the set or the order changes. The ESP32 re-reads the list when the tab opens and when it
  wraps back to photo 1; with the same `rev` it keeps its position.
- `items`: at most 100. `id`: 1–12 ASCII chars (`[a-z0-9]`, a hash of the file path, never the file name).
- `GET /api/photos/{id}.jpg` always sends `Content-Length` (never chunked); the ESP32 refuses anything above
  96 KB without reading it. Unknown or removed id: 404, the ESP32 skips to the next photo.

Image format (the ESP32 does not scale, rotate or convert):

- exactly **480x272**, "cover" crop (fills the frame, centred)
- EXIF orientation applied, then EXIF / ICC removed; colours converted to **sRGB**
- **baseline** JPEG (no progressive: JPEGDEC on the ESP32 rejects it), 4:2:0, quality 80, lowered in steps
  until the file is **≤ 60 KB**; files that stay above **90 KB** are skipped
- processed **once** into the cache folder, again only when the source file changes (mtime + size); the
  folder is rescanned every 60 s (`--photo-scan`), never per request

Config keys (same file as `/api/location`):

| Key | Default | Meaning |
|---|---|---|
| `photos_dir` | none (= no photos) | source folder, scanned recursively; hidden files/folders ignored |
| `photos_cache` | `<photos_dir>/.esp32-cache` | processed copies + `index.json` |
| `photos_order` | `name` | `name`, `mtime` (newest first) or `shuffle` (reshuffled once a day) |
| `photos_limit` | 100 | at most 100 |

ESP32 NVS key: `photo_s` (namespace `settings`), 5 or 10 seconds per photo, default 10.

```sh
curl -H "Authorization: Bearer $ALERTS_API_TOKEN" http://192.168.1.50:8080/api/photos
curl -H "Authorization: Bearer $ALERTS_API_TOKEN" -o p.jpg http://192.168.1.50:8080/api/photos/<id>.jpg
```
