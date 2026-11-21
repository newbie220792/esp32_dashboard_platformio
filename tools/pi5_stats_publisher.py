#!/usr/bin/env python3
"""Publish Raspberry Pi 5 stats and service status to MQTT for the ESP32 dashboard.

Topics
  pi5/stats     every 2 s, not retained
                {"cpu":37,"ram":52,"ram_used":4.1,"ram_total":8,"temp":58}
  pi5/services  on change and every 10 s, retained
                {"services":[{"name":"nginx","status":"running","port":80}, ...]}

Service status comes from `systemctl is-active`; the port is the lowest TCP
port that any process in the unit's cgroup listens on (`ss -ltnp`), or null.
Run as root (or with sudo) so `ss -p` can see other users' processes.

Setup on the Pi:
  pip install psutil paho-mqtt                   # (use a venv on Bookworm)
  sudo python3 pi5_stats_publisher.py --host localhost \\
      --services nginx,mosquitto,docker,node-red

Env vars MQTT_HOST / MQTT_PORT / MQTT_USER / MQTT_PASSWORD / PI5_SERVICES
work as defaults.
"""
import argparse
import json
import os
import re
import signal
import subprocess
import sys
import time

DEFAULT_SERVICES = "nginx,mosquitto,docker,node-red"
SS_PID_RE = re.compile(r"pid=(\d+)")


# ---------------------------------------------------------------- pure helpers
# (no psutil / paho imports, so they can be unit-tested anywhere)

def parse_ss_listeners(text):
    """Parse `ss -H -ltnp` output into {pid: set(ports)}."""
    listeners = {}
    for line in text.splitlines():
        cols = line.split()
        if len(cols) < 5:
            continue
        local = cols[3]                       # 0.0.0.0:80, [::]:1883, *:22, 127.0.0.1%lo:53
        port_str = local.rsplit(":", 1)[-1]
        if not port_str.isdigit():
            continue
        port = int(port_str)
        for pid in SS_PID_RE.findall(line):
            listeners.setdefault(int(pid), set()).add(port)
    return listeners


def pick_port(pids, listeners):
    ports = set()
    for pid in pids:
        ports |= listeners.get(pid, set())
    return min(ports) if ports else None


def status_from_is_active(output):
    return "running" if output.strip() == "active" else "stopped"


def build_services_payload(entries):
    """entries: iterable of (name, status, port)."""
    return {"services": [{"name": n, "status": s, "port": p} for n, s, p in entries]}


def to_json(obj):
    return json.dumps(obj, separators=(",", ":"))


# ------------------------------------------------------------ system queries

def _run(cmd):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=5).stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def unit_name(name):
    return name if "." in name else name + ".service"


def service_pids(name):
    cgroup = _run(["systemctl", "show", "-p", "ControlGroup", "--value", unit_name(name)]).strip()
    if not cgroup:
        return []
    try:
        with open("/sys/fs/cgroup" + cgroup + "/cgroup.procs") as f:
            return [int(x) for x in f.read().split()]
    except OSError:
        return []


def collect_services(names):
    listeners = parse_ss_listeners(_run(["ss", "-H", "-ltnp"]))
    out = []
    for name in names:
        status = status_from_is_active(_run(["systemctl", "is-active", unit_name(name)]))
        port = pick_port(service_pids(name), listeners) if status == "running" else None
        out.append((name, status, port))
    return build_services_payload(out)


def read_temp_c(psutil):
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


def collect_stats(psutil):
    gib = 1024 ** 3
    vm = psutil.virtual_memory()
    return {
        "cpu": round(psutil.cpu_percent(interval=None)),
        "ram": round(vm.percent),
        "ram_used": round((vm.total - vm.available) / gib, 1),
        "ram_total": round(vm.total / gib),
        "temp": round(read_temp_c(psutil)),
    }


# ---------------------------------------------------------------------- main

def make_client(mqtt, client_id):
    # paho-mqtt 2.x requires a callback API version; 1.x does not accept it.
    if hasattr(mqtt, "CallbackAPIVersion"):
        return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    return mqtt.Client(client_id=client_id)


def main():
    import paho.mqtt.client as mqtt
    import psutil

    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--host", default=os.getenv("MQTT_HOST", "localhost"))
    p.add_argument("--port", type=int, default=int(os.getenv("MQTT_PORT", "1883")))
    p.add_argument("--user", default=os.getenv("MQTT_USER"))
    p.add_argument("--password", default=os.getenv("MQTT_PASSWORD"))
    p.add_argument("--services", default=os.getenv("PI5_SERVICES", DEFAULT_SERVICES),
                   help="comma-separated systemd units to report")
    p.add_argument("--interval", type=float, default=2.0, help="pi5/stats period (s)")
    p.add_argument("--services-interval", type=float, default=10.0, help="pi5/services repeat period (s)")
    args = p.parse_args()
    names = [n.strip() for n in args.services.split(",") if n.strip()]

    client = make_client(mqtt, "pi5-stats")
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
    last_services = None
    last_services_t = 0.0
    while running:
        time.sleep(args.interval)
        if not client.is_connected():
            print("MQTT not connected, waiting...", file=sys.stderr)
            last_services = None          # republish once we're back
            continue
        client.publish("pi5/stats", to_json(collect_stats(psutil)), qos=0)

        services = to_json(collect_services(names))
        now = time.monotonic()
        if services != last_services or now - last_services_t >= args.services_interval:
            client.publish("pi5/services", services, qos=1, retain=True)
            last_services, last_services_t = services, now

    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    main()
