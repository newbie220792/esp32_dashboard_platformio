"""Photo library for the ESP32 Photos tab (task 10), served by alerts_api.py.

The Pi 5 does all image work so the ESP32 only downloads, decodes and shows:
every source photo is processed ONCE into a cache file and re-processed only
when the source changes (mtime + size):

  * EXIF orientation applied, then all metadata (EXIF, ICC) dropped
  * converted to sRGB (embedded ICC profile honoured when Pillow has LittleCMS)
  * "cover" crop to exactly 480x272 (fills the frame, centred crop)
  * baseline JPEG (never progressive), 4:2:0, quality 80 lowered in steps
    until <= 60 KB; files still over 90 KB are skipped

Which photos and in what order is decided here only (config keys):
  photos_dir    source folder (recursive); unset = no photos
  photos_cache  processed copies (default: <photos_dir>/.esp32-cache)
  photos_order  "name" (default), "mtime" (newest first) or "shuffle"
                (reshuffled once a day, so `rev` stays stable within a day)
  photos_limit  at most 100

`rev` changes whenever the set or the order changes; the ESP32 compares it to
decide whether to reload the list.
"""
import hashlib
import io
import json
import os
import random
import re
import threading
import time

WIDTH, HEIGHT = 480, 272
TARGET_BYTES = 60 * 1024
MAX_BYTES = 90 * 1024
MAX_ITEMS = 100
EXTENSIONS = (".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tif", ".tiff")
ID_RE = re.compile(r"^[a-z0-9]{1,12}$")


def photo_id(rel_path):
    """Short ASCII id (10 hex chars) that does not reveal the file name."""
    return hashlib.sha1(rel_path.encode("utf-8")).hexdigest()[:10]


def valid_id(pid):
    return bool(ID_RE.match(pid or ""))


# ---------------------------------------------------------------- processing

def _to_srgb(img):
    from PIL import Image
    icc = img.info.get("icc_profile")
    if icc:
        try:
            from PIL import ImageCms
            src = ImageCms.ImageCmsProfile(io.BytesIO(icc))
            dst = ImageCms.createProfile("sRGB")
            img = ImageCms.profileToProfile(img.convert("RGB") if img.mode not in ("RGB", "RGBA") else img,
                                            src, dst, outputMode="RGB")
        except Exception:   # no LittleCMS or a broken profile: treat the pixels as sRGB
            pass
    if img.mode in ("RGBA", "LA", "P"):
        img = img.convert("RGBA")
        bg = Image.new("RGB", img.size, (11, 15, 20))   # transparent areas -> dashboard background
        bg.paste(img, mask=img.split()[-1])
        return bg
    return img.convert("RGB")


def process_image(src_path):
    """Source file -> bytes of a 480x272 baseline JPEG, or None if it can't fit MAX_BYTES."""
    from PIL import Image, ImageOps
    with Image.open(src_path) as im:
        im = ImageOps.exif_transpose(im)
        im = _to_srgb(im)
        im = ImageOps.fit(im, (WIDTH, HEIGHT), Image.LANCZOS, centering=(0.5, 0.5))
    best = None
    for q in range(80, 35, -5):
        buf = io.BytesIO()
        # no exif= / icc_profile= arguments: the output carries no metadata
        im.save(buf, "JPEG", quality=q, subsampling=2, progressive=False, optimize=True)
        best = buf.getvalue()
        if len(best) <= TARGET_BYTES:
            break
    return best if best is not None and len(best) <= MAX_BYTES else None


# ---------------------------------------------------------------- library

class PhotoLibrary:
    """Scans the source folder in the background and answers from a snapshot.

    Requests never trigger processing: listing() and image() only read the last
    scan result, so a slow first scan just means a shorter list for a while.
    """

    def __init__(self, process=process_image, clock=time.time):
        self._process = process
        self._clock = clock
        self._lock = threading.Lock()
        self._items = []          # ordered ids
        self._rev = "0"
        self._cache_dir = None
        self._index = {}          # id -> {"src", "mtime_ns", "size"} for files in the cache

    # --- scanning (background thread or tests)

    def scan(self, cfg):
        src_dir = cfg.get("photos_dir")
        if not src_dir or not os.path.isdir(src_dir):
            self._publish([], None, {})
            return
        cache_dir = cfg.get("photos_cache") or os.path.join(src_dir, ".esp32-cache")
        os.makedirs(cache_dir, exist_ok=True)
        index = self._load_index(cache_dir)

        found = []   # (id, rel, mtime_ns, size, abs)
        for root, dirs, files in os.walk(src_dir):
            dirs[:] = [d for d in dirs if not d.startswith(".") and os.path.join(root, d) != cache_dir]
            for name in files:
                if name.startswith(".") or not name.lower().endswith(EXTENSIONS):
                    continue
                path = os.path.join(root, name)
                try:
                    st = os.stat(path)
                except OSError:
                    continue
                rel = os.path.relpath(path, src_dir)
                found.append((photo_id(rel), rel, st.st_mtime_ns, st.st_size, path))

        new_index = {}
        for pid, rel, mtime, size, path in found:
            old = index.get(pid)
            out = os.path.join(cache_dir, pid + ".jpg")
            if old and old["mtime_ns"] == mtime and old["size"] == size and os.path.exists(out):
                new_index[pid] = old
                continue
            try:
                data = self._process(path)
            except Exception as e:   # corrupt / unsupported file: skip, try again when it changes
                print(f"[photos] skip {rel}: {e}", flush=True)
                data = None
            if data is None:
                continue
            tmp = out + ".tmp"
            with open(tmp, "wb") as f:
                f.write(data)
            os.replace(tmp, out)
            new_index[pid] = {"src": rel, "mtime_ns": mtime, "size": size}

        # cache files whose source is gone
        for pid in set(index) - set(new_index):
            try:
                os.remove(os.path.join(cache_dir, pid + ".jpg"))
            except OSError:
                pass
        self._save_index(cache_dir, new_index)

        ok = [f for f in found if f[0] in new_index]
        order = cfg.get("photos_order", "name")
        if order == "mtime":
            ok.sort(key=lambda f: f[2], reverse=True)
        elif order == "shuffle":
            ok.sort(key=lambda f: f[1])
            random.Random(time.strftime("%Y-%m-%d", time.localtime(self._clock()))).shuffle(ok)
        else:
            ok.sort(key=lambda f: f[1].lower())
        limit = max(0, min(int(cfg.get("photos_limit", MAX_ITEMS)), MAX_ITEMS))
        ids = [f[0] for f in ok[:limit]]
        self._publish(ids, cache_dir, new_index)

    def _publish(self, ids, cache_dir, index):
        stamp = ",".join(f"{i}:{index[i]['mtime_ns']}" for i in ids)
        rev = hashlib.sha1(stamp.encode()).hexdigest()[:8] if ids else "0"
        with self._lock:
            self._items, self._rev, self._cache_dir, self._index = ids, rev, cache_dir, index

    @staticmethod
    def _load_index(cache_dir):
        try:
            with open(os.path.join(cache_dir, "index.json"), encoding="utf-8") as f:
                return json.load(f)
        except (OSError, ValueError):
            return {}

    @staticmethod
    def _save_index(cache_dir, index):
        tmp = os.path.join(cache_dir, "index.json.tmp")
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(index, f)
        os.replace(tmp, os.path.join(cache_dir, "index.json"))

    # --- requests

    def listing(self):
        with self._lock:
            return {"rev": self._rev, "count": len(self._items), "items": [{"id": i} for i in self._items]}

    def image(self, pid):
        """Bytes of a processed photo, or None (unknown / removed id)."""
        if not valid_id(pid):
            return None
        with self._lock:
            if pid not in self._items or not self._cache_dir:
                return None
            path = os.path.join(self._cache_dir, pid + ".jpg")
        try:
            with open(path, "rb") as f:
                return f.read()
        except OSError:
            return None


def scan_loop(lib, load_config, interval, stop):
    """Rescan every `interval` seconds; the config is re-read each time."""
    while not stop.is_set():
        try:
            lib.scan(load_config())
        except Exception as e:
            print(f"[photos] scan failed: {e}", flush=True)
        stop.wait(interval)
