"""Tests for photos.py and the /api/photos endpoints: python3 -m unittest discover -s tools

The library tests use a fake processor (no Pillow needed). The real image
pipeline is tested only when Pillow is installed (it is on the Pi)."""
import http.client
import io
import json
import os
import tempfile
import threading
import unittest
from http.server import ThreadingHTTPServer

import alerts_api as api
import photos

TOKEN = "test-token-0123456789"

try:
    from PIL import Image
    HAVE_PIL = True
except ImportError:
    HAVE_PIL = False


def fake_process(path):
    with open(path, "rb") as f:
        data = f.read()
    if data.startswith(b"BAD"):
        raise ValueError("corrupt")
    return b"\xff\xd8JPEG:" + data


class LibraryTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.src = os.path.join(self.tmp.name, "src")
        self.cache = os.path.join(self.tmp.name, "cache")
        os.makedirs(os.path.join(self.src, "sub"))
        self.calls = []

        def proc(path):
            self.calls.append(os.path.basename(path))
            return fake_process(path)

        self.lib = photos.PhotoLibrary(process=proc, clock=lambda: 1764000000)
        self.cfg = {"photos_dir": self.src, "photos_cache": self.cache}

    def tearDown(self):
        self.tmp.cleanup()

    def put(self, rel, data=b"x"):
        with open(os.path.join(self.src, rel), "wb") as f:
            f.write(data)

    def test_no_dir_is_empty(self):
        self.lib.scan({})
        self.assertEqual(self.lib.listing(), {"rev": "0", "count": 0, "items": []})

    def test_scan_order_ids_and_skip(self):
        self.put("b.jpg")
        self.put("a.PNG")
        self.put("sub/c.jpeg")
        self.put("notes.txt")
        self.put(".hidden.jpg")
        self.put("broken.jpg", b"BAD")
        self.lib.scan(self.cfg)
        lst = self.lib.listing()
        self.assertEqual(lst["count"], 3)
        ids = [i["id"] for i in lst["items"]]
        self.assertEqual(ids, [photos.photo_id("a.PNG"), photos.photo_id("b.jpg"), photos.photo_id(os.path.join("sub", "c.jpeg"))])
        for i in ids:
            self.assertTrue(photos.valid_id(i))
            self.assertLessEqual(len(i), 12)
        self.assertEqual(self.lib.image(ids[1]), b"\xff\xd8JPEG:x")

    def test_processed_once_and_rev(self):
        self.put("a.jpg")
        self.lib.scan(self.cfg)
        rev1 = self.lib.listing()["rev"]
        self.lib.scan(self.cfg)
        self.assertEqual(self.calls, ["a.jpg"])            # unchanged source: not processed again
        self.assertEqual(self.lib.listing()["rev"], rev1)

        self.put("b.jpg")
        self.lib.scan(self.cfg)
        self.assertEqual(self.calls, ["a.jpg", "b.jpg"])
        self.assertNotEqual(self.lib.listing()["rev"], rev1)

        os.remove(os.path.join(self.src, "a.jpg"))
        self.lib.scan(self.cfg)
        self.assertEqual(self.lib.listing()["count"], 1)
        self.assertFalse(os.path.exists(os.path.join(self.cache, photos.photo_id("a.jpg") + ".jpg")))

    def test_limit_and_shuffle_stable_within_a_day(self):
        for n in range(8):
            self.put(f"{n}.jpg")
        self.lib.scan(dict(self.cfg, photos_limit=5))
        self.assertEqual(self.lib.listing()["count"], 5)
        self.lib.scan(dict(self.cfg, photos_limit=500))
        self.assertEqual(self.lib.listing()["count"], 8)
        self.lib.scan(dict(self.cfg, photos_order="shuffle"))
        a = self.lib.listing()
        self.lib.scan(dict(self.cfg, photos_order="shuffle"))
        self.assertEqual(self.lib.listing(), a)

    def test_image_rejects_bad_and_unlisted_ids(self):
        self.put("a.jpg")
        self.lib.scan(self.cfg)
        for bad in ("../index", "", "x" * 13, "ABC", "a/b"):
            self.assertIsNone(self.lib.image(bad))
        self.assertIsNone(self.lib.image("0123456789"))


@unittest.skipUnless(HAVE_PIL, "Pillow not installed")
class ProcessTest(unittest.TestCase):
    def make(self, size, mode="RGB", exif_orientation=None, ext=".jpg"):
        d = tempfile.mkdtemp()
        path = os.path.join(d, "img" + ext)
        img = Image.new(mode, size, (200, 30, 40) if mode == "RGB" else (200, 30, 40, 128))
        kw = {}
        if exif_orientation:
            exif = Image.Exif()
            exif[0x0112] = exif_orientation
            kw["exif"] = exif.tobytes()
        img.save(path, **kw)
        return path

    def check(self, data):
        self.assertIsNotNone(data)
        self.assertLessEqual(len(data), photos.MAX_BYTES)
        im = Image.open(io.BytesIO(data))
        self.assertEqual(im.size, (480, 272))
        self.assertEqual(im.format, "JPEG")
        self.assertFalse(im.info.get("progressive") or im.info.get("progression"))
        self.assertNotIn("exif", im.info)
        self.assertNotIn("icc_profile", im.info)
        return im

    def test_cover_crop_any_aspect(self):
        for size in ((4000, 3000), (1080, 1920), (300, 100), (480, 272)):
            self.check(photos.process_image(self.make(size)))

    def test_exif_rotation_applied_then_dropped(self):
        # 6 = rotate 90: a 200x400 portrait stored sideways as 400x200
        im = self.check(photos.process_image(self.make((400, 200), exif_orientation=6)))
        self.assertEqual(im.mode, "RGB")

    def test_png_with_alpha(self):
        self.check(photos.process_image(self.make((800, 600), mode="RGBA", ext=".png")))

    def test_noisy_photo_fits_target(self):
        import random
        d = tempfile.mkdtemp()
        path = os.path.join(d, "noise.png")
        rnd = random.Random(1)
        img = Image.frombytes("RGB", (960, 544), bytes(rnd.randrange(256) for _ in range(960 * 544 * 3)))
        img.save(path)
        data = photos.process_image(path)
        if data is not None:   # pure noise may not fit 90 KB at all; then it is skipped
            self.check(data)


class HttpTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        src = os.path.join(self.tmp.name, "src")
        os.makedirs(src)
        with open(os.path.join(src, "a.jpg"), "wb") as f:
            f.write(b"hello")
        self.lib = photos.PhotoLibrary(process=fake_process)
        self.lib.scan({"photos_dir": src, "photos_cache": os.path.join(self.tmp.name, "c")})
        store = api.AlertStore(":memory:")
        self.srv = ThreadingHTTPServer(("127.0.0.1", 0), api.make_handler(store, TOKEN, None, self.lib))
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def tearDown(self):
        self.srv.shutdown()
        self.srv.server_close()
        self.tmp.cleanup()

    def get(self, path, token=TOKEN):
        c = http.client.HTTPConnection("127.0.0.1", self.srv.server_address[1], timeout=5)
        c.request("GET", path, headers={"Authorization": "Bearer " + token})
        r = c.getresponse()
        body = r.read()
        c.close()
        return r, body

    def test_list_and_photo(self):
        r, body = self.get("/api/photos")
        self.assertEqual(r.status, 200)
        lst = json.loads(body)
        self.assertEqual(lst["count"], 1)
        pid = lst["items"][0]["id"]
        r, body = self.get(f"/api/photos/{pid}.jpg")
        self.assertEqual(r.status, 200)
        self.assertEqual(r.getheader("Content-Type"), "image/jpeg")
        self.assertEqual(int(r.getheader("Content-Length")), len(body))   # never chunked
        self.assertIsNone(r.getheader("Transfer-Encoding"))
        self.assertEqual(body, b"\xff\xd8JPEG:hello")

    def test_unknown_photo_404_and_auth(self):
        self.assertEqual(self.get("/api/photos/0123456789.jpg")[0].status, 404)
        self.assertEqual(self.get("/api/photos/..%2Fx.jpg")[0].status, 404)
        self.assertEqual(self.get("/api/photos", token="wrong")[0].status, 401)


if __name__ == "__main__":
    unittest.main()
