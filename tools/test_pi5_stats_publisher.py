"""Unit tests for the pure helpers: python3 -m unittest discover -s tools"""
import json
import unittest

import pi5_stats_publisher as pub

SS_SAMPLE = """\
LISTEN 0 511 0.0.0.0:80 0.0.0.0:* users:(("nginx",pid=812,fd=6),("nginx",pid=811,fd=6))
LISTEN 0 511 [::]:80 [::]:* users:(("nginx",pid=811,fd=7))
LISTEN 0 100 0.0.0.0:1883 0.0.0.0:* users:(("mosquitto",pid=640,fd=5))
LISTEN 0 100 *:8883 *:* users:(("mosquitto",pid=640,fd=7))
LISTEN 0 511 *:1880 *:* users:(("node-red",pid=901,fd=21))
LISTEN 0 4096 127.0.0.53%lo:53 0.0.0.0:* users:(("systemd-resolve",pid=400,fd=14))
LISTEN 0 128 0.0.0.0:22 0.0.0.0:*
garbage line
"""


class SsParsing(unittest.TestCase):
    def test_parse(self):
        l = pub.parse_ss_listeners(SS_SAMPLE)
        self.assertEqual(l[811], {80})
        self.assertEqual(l[812], {80})
        self.assertEqual(l[640], {1883, 8883})
        self.assertEqual(l[400], {53})
        self.assertNotIn(0, l)

    def test_pick_lowest_port_across_cgroup(self):
        l = pub.parse_ss_listeners(SS_SAMPLE)
        self.assertEqual(pub.pick_port([640], l), 1883)
        self.assertEqual(pub.pick_port([811, 812, 9999], l), 80)
        self.assertIsNone(pub.pick_port([1234], l))
        self.assertIsNone(pub.pick_port([], l))


class Payload(unittest.TestCase):
    def test_status(self):
        self.assertEqual(pub.status_from_is_active("active\n"), "running")
        for s in ("inactive", "failed", "activating", ""):
            self.assertEqual(pub.status_from_is_active(s), "stopped")

    def test_payload_matches_firmware_format(self):
        payload = pub.build_services_payload([("nginx", "running", 80), ("docker", "running", None)])
        data = json.loads(pub.to_json(payload))
        self.assertEqual(data, {"services": [
            {"name": "nginx", "status": "running", "port": 80},
            {"name": "docker", "status": "running", "port": None},
        ]})
        self.assertIn('"port":null', pub.to_json(payload))

    def test_unit_name(self):
        self.assertEqual(pub.unit_name("nginx"), "nginx.service")
        self.assertEqual(pub.unit_name("docker.socket"), "docker.socket")


if __name__ == "__main__":
    unittest.main()
