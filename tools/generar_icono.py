"""Genera src/win/app.ico (un grano de café) en varios tamaños.

Uso:  python tools/generar_icono.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

SALIDA = Path(__file__).resolve().parent.parent / "src" / "win" / "app.ico"


def grano(tam):
    escala = 4  # se dibuja grande y se reduce para suavizar los bordes
    t = tam * escala
    img = Image.new("RGBA", (t, t), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    m = t * 0.08
    d.ellipse([m, m * 1.4, t - m, t - m * 1.4], fill=(91, 58, 41, 255), outline=(60, 36, 24, 255),
              width=max(1, t // 32))
    # Hendidura central del grano
    ancho = max(2, t // 14)
    d.arc([t * 0.30, t * 0.12, t * 0.70, t * 0.88], start=250, end=110, fill=(240, 220, 190, 255), width=ancho)
    return img.resize((tam, tam), Image.LANCZOS)


def main():
    tamanos = [16, 24, 32, 48, 64, 256]
    imagenes = [grano(t) for t in tamanos]
    imagenes[-1].save(SALIDA, format="ICO", sizes=[(t, t) for t in tamanos], append_images=imagenes[:-1])
    print("Generado", SALIDA)


if __name__ == "__main__":
    main()
