#!/usr/bin/env python3
"""Convert recursively all TGA images found in a directory to PNG."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, UnidentifiedImageError


def convert_tga(path: Path, overwrite: bool = False) -> bool:
    """Convert one TGA file, returning False when it cannot be read."""
    output = path.with_suffix(".png")

    if output.exists() and not overwrite:
        print(f"Ignoré (PNG déjà présent) : {output}")
        return False

    try:
        # verify() detects truncated/corrupt files without writing anything.
        with Image.open(path) as image:
            image.verify()

        # verify() invalidates the image object, so reopen it for conversion.
        with Image.open(path) as image:
            image.load()
            # PNG does not support every Pillow mode (for example CMYK).
            if image.mode not in {"1", "L", "LA", "P", "RGB", "RGBA", "I", "F"}:
                image = image.convert("RGBA" if "A" in image.getbands() else "RGB")
            image.save(output, format="PNG")
    except (OSError, ValueError, UnidentifiedImageError) as error:
        print(f"Ignoré (fichier corrompu ou illisible) : {path} — {error}")
        return False

    print(f"Converti : {path} -> {output}")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convertit récursivement les fichiers .TGA en .PNG."
    )
    parser.add_argument(
        "directory",
        nargs="?",
        type=Path,
        default=Path(__file__).resolve().parent,
        help="Dossier à parcourir (par défaut : racine du script).",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Remplace les PNG déjà existants.",
    )
    args = parser.parse_args()

    if not args.directory.is_dir():
        parser.error(f"Dossier introuvable : {args.directory}")

    files = (
        path
        for path in args.directory.rglob("*")
        if path.is_file() and path.suffix.lower() == ".tga"
    )
    converted = 0
    for path in files:
        converted += convert_tga(path, args.overwrite)

    print(f"{converted} fichier(s) converti(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
