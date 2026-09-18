"""Render GrokLink OS 3.9.1 host/CI honesty marketing graphics from HTML templates."""
from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "assets"
LAND = ROOT / "landing" / "public"
DESKTOP = Path.home() / "Desktop"


def main() -> None:
    gate = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "assert_public_version.py")],
        check=False,
    )
    if gate.returncode != 0:
        raise SystemExit("refusing to render: public version gate failed")

    OUT.mkdir(parents=True, exist_ok=True)
    jobs = [
        ("tools/render_infographic.html", OUT / "product-infographic.png", ".poster", 1400, 2200, "png"),
        ("tools/render_architecture.html", OUT / "architecture-infographic.jpg", "body", 1600, 1000, "jpeg"),
        ("tools/render_tweet_features.html", OUT / "tweet-features-v3.9.1.jpg", ".frame", 1200, 675, "jpeg"),
        ("tools/render_tweet_features.html", OUT / "tweet-features-v3.9.1-1200.jpg", ".frame", 1200, 630, "jpeg"),
    ]

    with sync_playwright() as p:
        browser = p.chromium.launch()
        for rel, out, sel, w, h, fmt in jobs:
            page = browser.new_page(viewport={"width": w, "height": h}, device_scale_factor=2)
            url = (ROOT / rel).resolve().as_uri()
            page.goto(url, wait_until="networkidle", timeout=60000)
            page.wait_for_timeout(900)
            loc = page.locator(sel).first
            loc.wait_for(state="visible", timeout=15000)
            box = loc.bounding_box()
            if box and box["height"] > h:
                page.set_viewport_size({"width": w, "height": int(box["height"]) + 40})
                page.wait_for_timeout(200)
                loc = page.locator(sel).first
            kwargs: dict = {"path": str(out), "type": fmt}
            if fmt == "jpeg":
                kwargs["quality"] = 92
            loc.screenshot(**kwargs)
            print(f"OK {out.name} bytes={out.stat().st_size}")
            page.close()
        browser.close()

    copies = [
        (OUT / "product-infographic.png", ROOT / "agent-skill" / "groklink-os" / "assets" / "GrokLink-OS-infographic.png"),
        (OUT / "product-infographic.png", LAND / "assets" / "infographic.png"),
        (OUT / "architecture-infographic.jpg", LAND / "assets" / "architecture.jpg"),
        (OUT / "tweet-features-v3.9.1-1200.jpg", LAND / "og.jpg"),
        (OUT / "tweet-features-v3.9.1-1200.jpg", LAND / "share-card.jpg"),
        (OUT / "tweet-features-v3.9.1-1200.jpg", LAND / "assets" / "share-card.jpg"),
        (OUT / "product-infographic.png", DESKTOP / "02-GrokLink" / "GrokLink-OS-infographic.png"),
        (OUT / "tweet-features-v3.9.1-1200.jpg", DESKTOP / "GrokLink-tweet-ready" / "tweet-card-1200x630.jpg"),
        (OUT / "product-infographic.png", DESKTOP / "02-GrokLink" / "GrokLink-OS-infographic-v3.9.1.png"),
    ]
    for src, dst in copies:
        if not src.exists():
            print("MISSING", src)
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        print(f"COPY {src.name} -> {dst}")

    stamp = LAND / "assets" / "GRAPHICS-VERSION.txt"
    stamp.write_text("3.9.1\n", encoding="utf-8", newline="\n")
    print("STAMP", stamp)

    jpg = OUT / "tweet-features-v3.9.1-1200.jpg"
    if jpg.exists():
        try:
            from PIL import Image

            im = Image.open(jpg).convert("RGB")
            for png in (LAND / "share-card.png", LAND / "assets" / "share-card.png"):
                im.save(png, "PNG")
                print(f"PNG {png}")
        except Exception as e:  # noqa: BLE001
            print("PNG twin skip", e)

    tweet_dir = DESKTOP / "GrokLink-tweet-ready"
    tweet_dir.mkdir(parents=True, exist_ok=True)
    (tweet_dir / "tweet-body.txt").write_text(
        "GrokLink OS 3.9.1 is a host/CI honesty cut.\n"
        "\n"
        "GLKFS CRC-on-read. Honest SD probe. BLE tick is a no-op. PC bridge skips USB banners.\n"
        "This tag does not recut DFU. Last packaged OsRadio is v3.8.0.\n"
        "Authorized research only. MIT.\n"
        "\n"
        "https://groklink.jonbailey.xyz/\n"
        "https://github.com/Pitchfork-and-Torch/GrokLink-OS\n",
        encoding="utf-8",
        newline="\n",
    )
    print("TWEET BODY", tweet_dir / "tweet-body.txt")


if __name__ == "__main__":
    main()
