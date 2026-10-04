"""Render GrokLink OS 3.8.0 marketing graphics from HTML templates."""
from __future__ import annotations

import shutil
from pathlib import Path

from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "assets"
DESKTOP = Path.home() / "Desktop"


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    jobs = [
        ("tools/render_infographic.html", OUT / "product-infographic.png", ".poster", 1400, 2200, "png"),
        ("tools/render_architecture.html", OUT / "architecture-infographic.jpg", "body", 1400, 1000, "jpeg"),
        ("tools/render_tweet_features.html", OUT / "tweet-features-v3.8.0.jpg", ".frame", 1200, 675, "jpeg"),
        ("tools/render_tweet_features.html", OUT / "tweet-features-v3.8.0-1200.jpg", ".frame", 1200, 675, "jpeg"),
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
        (OUT / "product-infographic.png", ROOT / "landing" / "public" / "assets" / "infographic.png"),
        (OUT / "product-infographic.png", DESKTOP / "02-GrokLink" / "GrokLink-OS-infographic.png"),
        (OUT / "tweet-features-v3.8.0-1200.jpg", DESKTOP / "GrokLink-tweet-ready" / "tweet-card-1200x630.jpg"),
        (OUT / "product-infographic.png", DESKTOP / "02-GrokLink" / "GrokLink-OS-infographic-v3.8.0.png"),
    ]
    for src, dst in copies:
        if not src.exists():
            print("MISSING", src)
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        print(f"COPY {src.name} -> {dst}")


if __name__ == "__main__":
    main()
