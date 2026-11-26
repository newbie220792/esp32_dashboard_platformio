# MQTT topics

One client on the ESP32 (`services/mqtt_service.cpp`, PubSubClient). It subscribes with QoS 0 and
publishes with QoS 0, not retained. Topics are routed to the data service that registered them with
`mqtt_service_route()`.

| Topic | Direction | Service | Sender / retain / QoS | Payload |
|---|---|---|---|---|
| `pi5/stats` | in, every 2 s | `pi5_service` | `tools/pi5_stats_publisher.py`, not retained, QoS 0 | `{"cpu":37,"ram":52,"ram_used":4.1,"ram_total":8,"temp":58}` |
| `pi5/services` | in, on change + every 10 s | `pi5_service` | `tools/pi5_stats_publisher.py`, **retained**, QoS 1 | `{"services":[{"name":"nginx","status":"running","port":80}]}` |
| `home/config` | in | `home_service` | `tools/home_sim.py` (or the real controller), **retained**, QoS 1 | `{"rooms":[{"id","name","panels":[{"id","name","switches":[{"id","name","icon","state"}]}]}]}` |
| `home/<room>/<panel>/<id>/state` | in | `home_service` | controller, **retained** | `ON` / `OFF` |
| `home/<room>/<panel>/<id>/set` | out | `home_service` | ESP32, not retained, QoS 0 | `ON` / `OFF` |

## Payload rules

- `pi5/services`: at most 10 entries (`MAX_SERVICES`); `port` may be `null`; any `status` other than
  `running` counts as stopped.
- `home/config`: at most 8 rooms, 6 panels per room, exactly 3 switches per panel (missing ones become
  empty slots, extra ones are dropped). Room or panel ids containing `/`, `+` or `#` are skipped.
  `icon` is `bulb`, `fan`, `plug`, `snow` (or `ac`). Up to 4096 bytes (MQTT buffer size).
- Malformed JSON is ignored and the previous data is kept.

## Switch round trip

1. A tap publishes the opposite state to `.../set` and marks the switch pending (dimmed, `...`).
2. The switch changes colour only when `.../state` arrives.
3. No `.../state` within `SWITCH_PENDING_MS` (3 s), or MQTT drops: the pending mark is cleared.

## Adding a topic

Add a handler in its service and register it in that service's `*_init()` with
`mqtt_service_route(filter, handler)` (`+` and `#` wildcards work). `mqtt_service` doesn't change.
