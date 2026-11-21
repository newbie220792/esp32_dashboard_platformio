#!/usr/bin/env python3
"""Fake home-switch backend for testing tile 3 without real devices.

- Publishes a sample home/config (retained).
- Subscribes home/+/+/+/set and answers on .../state (retained) after a short
  delay, like a real relay would.
- --drop N ignores every Nth command, to test the 3 s pending timeout.

  pip install paho-mqtt
  python3 home_sim.py --host <broker>
"""
import argparse
import json
import os
import threading

CONFIG = {
    "rooms": [
        {"id": "living", "name": "Living room", "panels": [
            {"id": "a", "name": "Panel A", "switches": [
                {"id": 1, "name": "Ceiling", "icon": "bulb", "state": "ON"},
                {"id": 2, "name": "Wall", "icon": "bulb", "state": "OFF"},
                {"id": 3, "name": "Fan", "icon": "fan", "state": "ON"}]},
            {"id": "b", "name": "Panel B", "switches": [
                {"id": 1, "name": "AC", "icon": "snow", "state": "OFF"},
                {"id": 2, "name": "TV", "icon": "plug", "state": "OFF"},
                {"id": 3, "name": "Lamp", "icon": "bulb", "state": "ON"}]},
            {"id": "c", "name": "Panel C", "switches": [
                {"id": 1, "name": "Porch", "icon": "bulb", "state": "OFF"},
                {"id": 2, "name": "Heater", "icon": "plug", "state": "OFF"},
                {"id": 3, "name": "Exhaust", "icon": "fan", "state": "OFF"}]}]},
        {"id": "bed", "name": "Bedroom", "panels": [
            {"id": "a", "name": "Bedside", "switches": [
                {"id": 1, "name": "Lamp", "icon": "bulb", "state": "OFF"},
                {"id": 2, "name": "AC", "icon": "snow", "state": "ON"},
                {"id": 3, "name": "Fan", "icon": "fan", "state": "OFF"}]}]},
        {"id": "kitchen", "name": "Kitchen", "panels": [
            {"id": "main", "name": "Main", "switches": [
                {"id": 1, "name": "Light", "icon": "bulb", "state": "ON"},
                {"id": 2, "name": "Kettle", "icon": "plug", "state": "OFF"}]}]},
    ]
}


def main():
    import paho.mqtt.client as mqtt

    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--host", default=os.getenv("MQTT_HOST", "localhost"))
    p.add_argument("--port", type=int, default=int(os.getenv("MQTT_PORT", "1883")))
    p.add_argument("--user", default=os.getenv("MQTT_USER"))
    p.add_argument("--password", default=os.getenv("MQTT_PASSWORD"))
    p.add_argument("--delay", type=float, default=0.3, help="reply delay (s)")
    p.add_argument("--drop", type=int, default=0, help="ignore every Nth command (0 = never)")
    args = p.parse_args()

    if hasattr(mqtt, "CallbackAPIVersion"):
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="home-sim")
    else:
        client = mqtt.Client(client_id="home-sim")
    if args.user:
        client.username_pw_set(args.user, args.password)

    count = {"n": 0}

    def on_connect(c, *_):
        c.publish("home/config", json.dumps(CONFIG), qos=1, retain=True)
        for room in CONFIG["rooms"]:
            for panel in room["panels"]:
                for sw in panel["switches"]:
                    c.publish(f"home/{room['id']}/{panel['id']}/{sw['id']}/state", sw["state"], retain=True)
        c.subscribe("home/+/+/+/set")
        print("published home/config, waiting for commands")

    def on_message(c, _userdata, msg):
        payload = msg.payload.decode(errors="replace")
        count["n"] += 1
        if args.drop and count["n"] % args.drop == 0:
            print(f"DROP {msg.topic} {payload}")
            return
        if payload not in ("ON", "OFF"):
            print(f"ignore {msg.topic} {payload!r}")
            return
        state_topic = msg.topic[: -len("/set")] + "/state"
        print(f"{msg.topic} {payload} -> {state_topic}")
        threading.Timer(args.delay, lambda: c.publish(state_topic, payload, retain=True)).start()

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.host, args.port, keepalive=30)
    client.loop_forever()


if __name__ == "__main__":
    main()
