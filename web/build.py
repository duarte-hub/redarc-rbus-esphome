#!/usr/bin/env python3
"""Build web/redarc_ui.js from redarc_ui.src.js.

The page uses the RedVision app's own icons. They are REDARC's artwork, so they are not
stored in this repository: point this script at the assets of an APK you extracted yourself
(the folder that contains images/icons_controls, images/icons, ...):

    unzip your.apk 'assets/flutter_assets/assets/*'
    python3 web/build.py path/to/assets/flutter_assets/assets

The generated redarc_ui.js is git-ignored. Reference it from the ESPHome config:

    web_server:
      version: 2
      js_include: redarc_ui.js   # copy next to your device yaml
      js_url: ""
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

# icon key -> path relative to the assets folder
ICONS = {
    "light": "images/icons_controls/light.svg",
    "power12": "images/icons_controls/12v_power.svg",
    "power240": "images/icons_controls/240v_power.svg",
    "compressor": "images/icons_controls/compressor.svg",
    "fridge": "images/icons_controls/fridge.svg",
    "stereo": "images/icons_controls/uhf_radio.svg",
    "speaker": "images/icons_controls/stereo.svg",
    "tap": "images/icons_controls/tap.svg",
    "heater": "images/icons_controls/heater.svg",
    "fan": "images/icons_controls/fan.svg",
    "usb": "images/icons_controls/usb.svg",
    "output": "images/icons_controls/output.svg",
    "temp": "images/icons_controls/temp.svg",
    "car": "images/icons/car.svg",
    "sun": "images/icons/sun.svg",
    "plug": "images/icons/plug.svg",
}


def clean(svg: str) -> str:
    svg = re.sub(r"<\?xml.*?\?>", "", svg, flags=re.S)
    svg = re.sub(r"<!--.*?-->", "", svg, flags=re.S)
    # the app tints its monochrome icons; do the same through currentColor
    svg = re.sub(r'(fill|stroke)="#(?!fff)[0-9A-Fa-f]{3,6}"', r'\1="currentColor"', svg)
    svg = re.sub(r'\s(fill|stroke)-opacity="[^"]*"', "", svg)
    head = re.search(r"<svg[^>]*>", svg).group(0)
    vb = re.search(r'viewBox="([^"]+)"', head).group(1)
    body = svg[svg.index(head) + len(head):svg.rindex("</svg>")]
    body = re.sub(r"\s+", " ", body).strip()
    return f'<svg viewBox="{vb}" fill="none">{body}</svg>'


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    assets = Path(sys.argv[1])
    icons = {k: clean((assets / p).read_text()) for k, p in ICONS.items()}
    src = (HERE / "redarc_ui.src.js").read_text()
    out = src.replace("/*ICONS*/{}", json.dumps(icons, separators=(",", ":")))
    (HERE / "redarc_ui.js").write_text(out)
    print(f"redarc_ui.js: {len(out) / 1024:.1f} KiB, {len(icons)} icons")


if __name__ == "__main__":
    main()
