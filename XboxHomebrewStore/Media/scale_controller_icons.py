"""
Scale all PNGs in the Controller folder so max height is 24px (aspect ratio preserved).
Run from repo root: python XboxHomebrewStore/Media/scale_controller_icons.py
Or: python scale_controller_icons.py
"""
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Install Pillow: pip install Pillow")
    raise

MAX_HEIGHT = 24
# Script lives in Media/, so Controller is next to this script
CONTROLLER_DIR = Path(__file__).resolve().parent / "Controller"

def main():
    if not CONTROLLER_DIR.is_dir():
        print(f"Folder not found: {CONTROLLER_DIR}")
        return
    for path in sorted(CONTROLLER_DIR.glob("*.png")):
        img = Image.open(path).convert("RGBA")
        w, h = img.size
        if h <= 0:
            continue
        if h <= MAX_HEIGHT:
            print(f"Skip (already <= {MAX_HEIGHT}px tall): {path.name}")
            continue
        new_h = MAX_HEIGHT
        new_w = max(1, round(w * MAX_HEIGHT / h))
        resized = img.resize((new_w, new_h), Image.Resampling.LANCZOS)
        resized.save(path)
        print(f"Scaled {path.name}: {w}x{h} -> {new_w}x{new_h}")

if __name__ == "__main__":
    main()
