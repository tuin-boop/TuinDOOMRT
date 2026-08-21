"""Build an RT-friendly animated-liquid companion for LTP V6.7.

GZDoom-RT does not execute LTP's fragment shaders.  This tool converts the
pack's editor-preview textures into a looping set of ordinary texture frames,
which both the normal and ray-traced render paths understand.
"""

from __future__ import annotations

import argparse
import math
import shutil
import tempfile
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image


# Doom advances at 35 tics per second. One distinct frame per tic avoids the
# visibly stepped 12-FPS motion of the first compatibility build.
FRAME_COUNT = 70
FRAME_TICS = 1

LIQUIDS = {
    "water": {
        "source": "LPWater.png", "fall": "LPWaterF.png", "prefix": "TWA", "fall_prefix": "FWA",
        "grade": 0.72,
        "flats": ["FWATER1", "FWATER2", "FWATER3", "FWATER4", "LPWATER"],
        "falls": ["WFALL1", "WFALL2", "WFALL3", "WFALL4", "LPWATERF"],
    },
    "blood": {
        "source": "LPBlood.png", "fall": "LPBloodF.png", "prefix": "TBL", "fall_prefix": "FBL",
        "grade": 0.68,
        "flats": ["BLOOD1", "BLOOD2", "BLOOD3", "LPBLOOD"],
        "falls": ["BFALL1", "BFALL2", "BFALL3", "BFALL4", "LPBLOODF"],
    },
    "slime": {
        "source": "LPSlime.png", "fall": "LPSlimeF.png", "prefix": "TSL", "fall_prefix": "FSL",
        "grade": 0.62,
        "flats": [f"SLIME{i:02d}" for i in range(1, 9)] + ["LPSLIME"],
        "falls": ["LSFALL1", "LSFALL2", "SFALL1", "SFALL2", "SFALL3", "SFALL4", "LPSLIMEF"],
    },
    "nukage": {
        "source": "LPNuke.png", "fall": "LPNukeF.png", "prefix": "TNK", "fall_prefix": "FNK",
        "grade": 0.52,
        "flats": ["NUKAGE1", "NUKAGE2", "NUKAGE3", "LPNUKE"],
        "falls": ["LTFALL1", "LTFALL2"],
    },
    "lava": {
        "source": "LPLava.png", "fall": "LPLavaF.png", "prefix": "TLV", "fall_prefix": "FLV",
        "grade": 0.68,
        "flats": ["LAVA1", "LAVA2", "LAVA3", "LAVA4", "LPLAVA"],
        "falls": ["LLFALL1", "LLFALL2", "LPLAVAF"],
    },
}


def periodic_bilinear(array: np.ndarray, sx: np.ndarray, sy: np.ndarray) -> np.ndarray:
    """Sample an image with wrapped coordinates so generated frames remain seamless."""
    h, w = array.shape[:2]
    x0 = np.floor(sx).astype(np.int32) % w
    y0 = np.floor(sy).astype(np.int32) % h
    x1 = (x0 + 1) % w
    y1 = (y0 + 1) % h
    fx = (sx - np.floor(sx))[..., None]
    fy = (sy - np.floor(sy))[..., None]
    top = array[y0, x0] * (1.0 - fx) + array[y0, x1] * fx
    bottom = array[y1, x0] * (1.0 - fx) + array[y1, x1] * fx
    return top * (1.0 - fy) + bottom * fy


def make_frame(source: Image.Image, frame: int, falling: bool, grade: float) -> Image.Image:
    src = np.asarray(source.convert("RGBA"), dtype=np.float32)
    h, w = src.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    phase = frame / FRAME_COUNT
    turn = 2.0 * math.pi * phase

    if falling:
        # Strong downward motion plus a small sideways ripple.
        sx = xx + 2.4 * np.sin((yy / h) * math.tau * 2.0 + turn)
        sy = yy - phase * h + 1.4 * np.sin((xx / w) * math.tau * 2.0 - turn)
    else:
        # LTP combines opposing scroll directions with a slowly oscillating
        # parallax layer.  Keep the loop stationary overall while ripples move
        # through it, including a gentle vertical rise/fall cue.
        breathe_x = 2.0 * math.sin(turn)
        breathe_y = 5.5 * math.cos(turn)
        sx = xx + breathe_x + 4.0 * np.sin((yy / h) * math.tau * 2.0 + turn)
        sx += 1.8 * np.sin((xx / w + yy / h) * math.tau - turn * 0.5)
        sy = yy + breathe_y + 3.5 * np.sin((xx / w) * math.tau * 2.0 - turn)
        sy += 1.5 * np.cos((xx / w - yy / h) * math.tau + turn * 0.5)

    warped = periodic_bilinear(src, sx, sy)
    warped[..., :3] *= grade
    return Image.fromarray(np.clip(warped, 0, 255).astype(np.uint8), "RGBA")


def animation_block(name: str, prefix: str) -> str:
    lines = [f"FLAT {name}"]
    lines.extend(f" pic {prefix}{i:02X} tics {FRAME_TICS}" for i in range(FRAME_COUNT))
    return "\n".join(lines)


def build(source_pk3: Path, output_pk3: Path) -> None:
    if not source_pk3.is_file():
        raise FileNotFoundError(source_pk3)

    with tempfile.TemporaryDirectory(prefix="tuindoom-ltp-") as temp_name:
        temp = Path(temp_name)
        with zipfile.ZipFile(source_pk3) as archive:
            archive.extractall(temp / "source")

        source_dir = temp / "source" / "textures" / "Map Editor Textures"
        pack = temp / "pack"
        frames_dir = pack / "textures" / "rtliquid"
        frames_dir.mkdir(parents=True)
        animdefs = ["// TuinDoom RT compatibility animations for LTP V6.7", ""]
        texturedefs = ["// Large-scale RT-visible liquid frames", ""]

        for info in LIQUIDS.values():
            flat_image = Image.open(source_dir / info["source"])
            fall_image = Image.open(source_dir / info["fall"])
            for frame in range(FRAME_COUNT):
                flat_name = f'{info["prefix"]}{frame:02X}'
                fall_name = f'{info["fall_prefix"]}{frame:02X}'
                make_frame(flat_image, frame, False, info["grade"]).save(frames_dir / f'{flat_name}.png')
                make_frame(fall_image, frame, True, info["grade"]).save(frames_dir / f'{fall_name}.png')
                texturedefs.extend([
                    f'Texture "{flat_name}", {flat_image.width}, {flat_image.height}',
                    "{", " xscale 0.25", " yscale 0.25",
                    f' patch "textures/rtliquid/{flat_name}.png", 0, 0', "}", "",
                    f'Texture "{fall_name}", {fall_image.width}, {fall_image.height}',
                    "{", " xscale 0.5", " yscale 0.5",
                    f' patch "textures/rtliquid/{fall_name}.png", 0, 0', "}", "",
                ])
            for texture in info["flats"]:
                animdefs.extend([animation_block(texture, info["prefix"]), ""])
            for texture in info["falls"]:
                animdefs.extend([animation_block(texture, info["fall_prefix"]), ""])

        (pack / "ANIMDEFS").write_text("\n".join(animdefs), encoding="ascii")
        # Extension-named root lumps avoid Windows' case-insensitive collision
        # between a TEXTURES lump and the required textures/ directory.
        (pack / "TEXTURES.rtcompat").write_text("\n".join(texturedefs), encoding="ascii")
        (pack / "README-TuinDoom.txt").write_text(
            "TuinDoom RT compatibility companion for LTP V6.7\n\n"
            "Loads after LTP.V6.7.pk3. Replaces shader-only liquid motion with ordinary\n"
            "70-frame, 35-FPS looping textures that are visible to the ray-traced renderer.\n"
            "Original liquid artwork belongs to the LTP authors and contributors.\n",
            encoding="utf-8",
        )

        output_pk3.parent.mkdir(parents=True, exist_ok=True)
        if output_pk3.exists():
            output_pk3.unlink()
        shutil.make_archive(str(output_pk3.with_suffix("")), "zip", pack)
        output_pk3.with_suffix(".zip").replace(output_pk3)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build(args.source.resolve(), args.output.resolve())
    print(args.output.resolve())


if __name__ == "__main__":
    main()
