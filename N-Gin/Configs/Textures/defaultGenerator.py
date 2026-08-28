from pathlib import Path
from PIL import Image

OUTPUT_DIR = Path("Assets/DefaultTextures")

textures = {
    # name                  RGBA
    "defaultTexture_albedo.png":     (255, 255, 255, 255),  # white
    "defaultTexture_normal.png":     (128, 128, 255, 255),  # tangent-space +Z
    "defaultTexture_smoothness.png": (128, 128, 128, 255),  # 0.5 smoothness
    "defaultTexture_height.png":     (128, 128, 128, 255),  # neutral midpoint
}


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    for filename, color in textures.items():
        image = Image.new("RGBA", (1, 1), color)

        path = OUTPUT_DIR / filename
        image.save(path)

        print(f"Created: {path}")


if __name__ == "__main__":
    main()