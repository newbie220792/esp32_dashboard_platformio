"""Tests for alerts_api.py (stdlib only): python3 -m unittest discover -s tools"""
import http.client
import json
import os
import tempfile
import threading
import unittest
from http.server import ThreadingHTTPServer

import alerts_api as api

TOKEN = "test-token-0123456789"


class FakeClock:
    def __init__(self, t=1764000000):
        self.t = t

    def __call__(self):
        return self.t


class StoreTest(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.s = api.AlertStore(":memory:", clock=self.clock)

    def test_summary_empty(self):
        self.assertEqual(self.s.summary(), {"unacked": 0, "critical": 0, "latest_id": 0})
        self.assertEqual(self.s.items(), [])

    def test_steady_condition_is_one_alert(self):
        a = self.s.raise_alert("temp_high", "critical", "Pi 5 temperature high", "74C")
        b = self.s.raise_alert("temp_high", "critical", "Pi 5 temperature high", "75C")
        self.assertEqual(a, b)
        items = self.s.items()
        self.assertEqual(len(items), 1)
        self.assertEqual(items[0]["count"], 1)
        self.assertEqual(items[0]["detail"], "75C")
        self.assertEqual(self.s.summary(), {"unacked": 1, "critical": 1, "latest_id": a})

    def test_repeat_after_resolve_merges_and_moves_latest_id(self):
        a = self.s.raise_alert("svc:node-red", "warning", "node-red stopped")
        self.s.resolve("svc:node-red")
        self.assertEqual(self.s.summary()["unacked"], 0)   # closed: not listed
        self.assertEqual(self.s.items(), [])
        self.clock.t += 600
        b = self.s.raise_alert("svc:node-red", "warning", "node-red stopped")
        self.assertGreater(b, a)
        items = self.s.items()
        self.assertEqual(len(items), 1)
        self.assertEqual(items[0]["count"], 2)
        self.assertEqual(items[0]["ts"], self.clock.t)
        self.assertEqual(self.s.summary()["latest_id"], b)

    def test_never_more_than_10_newest_first(self):
        for i in range(15):
            self.clock.t += 1
            self.s.raise_alert(f"k{i}", "info", f"t{i}")
        items = self.s.items(limit=50)
        self.assertEqual(len(items), 10)
        self.assertEqual(items[0]["title"], "t14")
        self.assertEqual(self.s.summary()["unacked"], 15)

    def test_ack_all(self):
        self.s.raise_alert("a", "critical", "A")
        self.s.raise_alert("b", "warning", "B")
        self.assertEqual(self.s.ack_all(), 2)
        self.assertEqual(self.s.summary()["unacked"], 0)
        self.assertEqual(self.s.items(), [])
        # still true after the ack: a new alert, count starts over
        self.s.raise_alert("a", "critical", "A")
        self.assertEqual(self.s.items()[0]["count"], 1)

    def test_level_and_length_limits(self):
        self.s.raise_alert("x", "rain", "T" * 80, "D" * 100)
        it = self.s.items()[0]
        self.assertEqual(it["level"], "info")
        self.assertLessEqual(len(it["title"]), api.TITLE_MAX)
        self.assertLessEqual(len(it["detail"]), api.DETAIL_MAX)


class RulesTest(unittest.TestCase):
    def setUp(self):
        self.s = api.AlertStore(":memory:")
        self.cfg = dict(api.DEFAULT_CONFIG, services={"mosquitto": "critical", "nginx": "warning"})

    def run_pass(self, temp=50, ram=40, services=None, mqtt=True, boot=1000.0):
        api.evaluate(self.s, self.cfg, {"temp": temp, "ram": ram},
                     services or {"mosquitto": True, "nginx": True}, mqtt, boot)

    def levels(self):
        return sorted((i["level"], i["title"]) for i in self.s.items())

    def test_all_good(self):
        self.run_pass()
        self.assertEqual(self.s.items(), [])

    def test_thresholds_and_auto_close(self):
        self.run_pass(temp=74, ram=92, services={"mosquitto": False, "nginx": False}, mqtt=False)
        self.assertEqual(self.levels(), [
            ("critical", "Pi 5 temperature high"), ("critical", "mosquitto stopped"),
            ("warning", "MQTT broker unreachable"), ("warning", "RAM usage high"), ("warning", "nginx stopped")])
        self.assertEqual(self.s.summary()["critical"], 2)
        self.run_pass()
        self.assertEqual(self.s.items(), [])

    def test_reboot_info(self):
        self.run_pass(boot=1000.0)
        self.assertEqual(self.s.items(), [])   # first run only records the boot time
        self.run_pass(boot=5000.0)
        self.assertEqual(self.levels(), [("info", "Pi 5 rebooted")])


class LocationTest(unittest.TestCase):
    def test_payload(self):
        self.assertEqual(api.location_payload({"city": " TP Dong Nai ", "lat": 10.95741, "lon": 106.84269}),
                         {"city": "TP Dong Nai", "lat": 10.9574, "lon": 106.8427})
        self.assertIsNone(api.location_payload({"city": "", "lat": 1, "lon": 2}))
        self.assertIsNone(api.location_payload({"city": "X", "lat": 95, "lon": 2}))
        self.assertIsNone(api.location_payload({"city": "X", "lat": "a", "lon": 2}))

    def test_config_file_overrides_defaults(self):
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            json.dump({"city": "Bien Hoa"}, f)
        try:
            cfg = api.load_config(f.name)
            self.assertEqual(cfg["city"], "Bien Hoa")
            self.assertEqual(cfg["temp_critical"], 70)
        finally:
            os.unlink(f.name)
        self.assertEqual(api.load_config("/no/such/file.json")["city"], "TP Dong Nai")


class HttpTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.store = api.AlertStore(":memory:")
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), api.make_handler(cls.store, TOKEN, None))
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()
        cls.port = cls.server.server_address[1]

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()

    def call(self, method, path, token=TOKEN, conn=None):
        c = conn or http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        headers = {"Authorization": "Bearer " + token} if token else {}
        c.request(method, path, body=b"" if method == "POST" else None, headers=headers)
        r = c.getresponse()
        result = r.status, json.loads(r.read())
        if conn is None:
            c.close()
        return result

    def test_auth(self):
        self.assertEqual(self.call("GET", "/api/alerts/summary", token=None)[0], 401)
        self.assertEqual(self.call("GET", "/api/alerts/summary", token="wrong")[0], 401)
        self.assertEqual(self.call("POST", "/api/alerts/ack-all", token="wrong")[0], 401)

    def test_flow_on_one_keepalive_connection(self):
        c = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        self.store.raise_alert("t", "critical", "Pi 5 temperature high", "74°C")
        code, body = self.call("GET", "/api/alerts/summary", conn=c)
        self.assertEqual((code, body["unacked"]), (200, 1))
        code, body = self.call("GET", "/api/alerts", conn=c)
        self.assertEqual(code, 200)
        self.assertEqual(body["items"][0]["title"], "Pi 5 temperature high")
        self.assertFalse(body["items"][0]["ack"])
        code, body = self.call("POST", "/api/alerts/ack-all", conn=c)
        self.assertEqual((code, body["acked"]), (200, 1))
        self.assertEqual(self.call("GET", "/api/alerts/summary", conn=c)[1]["unacked"], 0)
        c.close()

    def test_health(self):
        self.assertEqual(self.call("GET", "/api/health"), (200, {"ok": True}))
        self.assertEqual(self.call("GET", "/api/health", token=None)[0], 401)

    def test_location_and_404(self):
        self.assertEqual(self.call("GET", "/api/location"),
                         (200, {"city": "TP Dong Nai", "lat": 10.9574, "lon": 106.8427}))
        self.assertEqual(self.call("GET", "/api/nope")[0], 404)


if __name__ == "__main__":
    unittest.main()
