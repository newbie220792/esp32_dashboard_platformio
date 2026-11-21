#!/usr/bin/env python3
"""Publish Raspberry Pi 5 stats to MQTT every 2 s for the ESP32 dashboard.

Payload on topic pi5/stats:
  {"cpu":37,"ram":52,"ram_used":4.1,"ram_total":8,"temp":58}

Setup on the Pi:
  sudo apt install mosquitto mosquitto-clients   # if the broker runs on the Pi
  pip install psutil paho-mqtt                   # (use a venv on Bookworm)
  python3 pi5_stats_publisher.py --host localhost

Env vars MQTT_HOST / MQTT_PORT / MQTT_USER / MQTT_PASSWORD work as defaults.
"""
import argparse
import json
import os
import signal
import sys
import time

import paho.mqtt.client as mqtt
import psutil


def read_temp_c():
    try:
        temps = psutil.sensors_temperatures()
        for key in ("cpu_thermal", "coretemp", "soc_thermal"):
            if temps.get(key):
                return temps[key][0].current
    except (AttributeError, OSError):
        pass
    try:
        with open("/sys/class/thermal/thermal_zone0/temp") as f:
            return int(f.read().strip()) / 1000.0
    except OSError:
        return 0.0


def make_client(client_id):
    # paho-mqtt 2.x requires a callback API version; 1.x does not accept it.
    if hasattr(mqtt, "CallbackAPIVersion"):
        return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    return mqtt.Client(client_id=client_id)


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--host", default=os.getenv("MQTT_HOST", "localhost"))
    p.add_argument("--port", type=int, default=int(os.getenv("MQTT_PORT", "1883")))
    p.add_argument("--user", default=os.getenv("MQTT_USER"))
    p.add_argument("--password", default=os.getenv("MQTT_PASSWORD"))
    p.add_argument("--topic", default="pi5/stats")
    p.add_argument("--interval", type=float, default=2.0)
    args = p.parse_args()

    client = make_client("pi5-stats")
    if args.user:
        client.username_pw_set(args.user, args.password)
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    client.connect_async(args.host, args.port, keepalive=30)
    client.loop_start()

    running = True

    def stop(*_):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    psutil.cpu_percent(interval=None)  # prime: first call always returns 0
    gib = 1024 ** 3
    while running:
        time.sleep(args.interval)
        vm = psutil.virtual_memory()
        payload = {
            "cpu": round(psutil.cpu_percent(interval=None)),
            "ram": round(vm.percent),
            "ram_used": round((vm.total - vm.available) / gib, 1),
            "ram_total": round(vm.total / gib),
            "temp": round(read_temp_c()),
        }
        if client.is_connected():
            client.publish(args.topic, json.dumps(payload, separators=(",", ":")), qos=0)
        else:
            print("not connected, skipping", payload, file=sys.stderr)

    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    main()
