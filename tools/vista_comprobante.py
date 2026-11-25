"""Convierte a imagen PNG el dibujo del comprobante (el mismo que el programa muestra e imprime).

Uso:  pruebas --lienzo 1 | python tools/vista_comprobante.py salida.png
      (1 = marca de agua opción 1, 2 = opción 2, 0 = sin marca)

Sirve para revisar el diseño sin Windows. Las letras son parecidas a las de Windows
(Segoe UI), no idénticas.
"""
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

RAIZ = Path(__file__).resolve().parent.parent
FUENTES = [Path("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"),
           Path("C:/Windows/Fonts/segoeui.ttf")]
FUENTES_NEGRITA = [Path("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"),
                   Path("C:/Windows/Fonts/segoeuib.ttf")]
IMAGENES = {"logo": RAIZ / "src/win/logo_documento.bmp", "marca1": RAIZ / "src/win/marca1.bmp",
            "marca2": RAIZ / "src/win/marca2.bmp"}
ESCALA = 1.2  # píxeles por unidad (1/100 de pulgada) -> 120 ppp


def fuente(puntos, negrita):
    lista = FUENTES_NEGRITA if negrita else FUENTES
    ruta = next((p for p in lista if p.exists()), None)
    tam = max(1, round(puntos * 100 / 72 * ESCALA))
    return ImageFont.truetype(str(ruta), tam) if ruta else ImageFont.load_default()


def color(valor):
    return ((valor >> 16) & 255, (valor >> 8) & 255, valor & 255)


def envolver(texto, ancho, f):
    lineas = []
    for parrafo in texto.split("\n"):
        actual = ""
        for palabra in parrafo.split(" "):
            prueba = (actual + " " + palabra).strip()
            if f.getlength(prueba) <= ancho or not actual:
                actual = prueba
            else:
                lineas.append(actual)
                actual = palabra
        lineas.append(actual)
    return lineas


def main():
    datos = json.load(sys.stdin)
    w, h = round(datos["ancho"] * ESCALA), round(datos["alto"] * ESCALA)
    hoja = Image.new("RGB", (w, h), "white")
    d = ImageDraw.Draw(hoja)
    for e in datos["elementos"]:
        x, y = e["x"] * ESCALA, e["y"] * ESCALA
        ancho, alto = e["ancho"] * ESCALA, e["alto"] * ESCALA
        if e["tipo"] == "rectangulo":
            relleno = None if e["relleno"] == 0xFFFFFFFF else color(e["relleno"])
            borde = None if e["color"] == 0xFFFFFFFF else color(e["color"])
            d.rectangle([x, y, x + ancho, y + alto], fill=relleno, outline=borde,
                        width=max(1, round(e["grosor"] * ESCALA)) if borde else 0)
        elif e["tipo"] == "linea":
            d.line([x, y, x + ancho, y + alto], fill=color(e["color"]), width=max(1, round(e["grosor"] * ESCALA)))
        elif e["tipo"] == "imagen":
            img = Image.open(IMAGENES[e["imagen"]]).convert("RGB")
            factor = min(ancho / img.width, alto / img.height)
            iw, ih = round(img.width * factor), round(img.height * factor)
            img = img.resize((iw, ih), Image.LANCZOS)
            # Como en Windows (SRCAND): la imagen sobre fondo blanco no tapa lo que ya está dibujado.
            px, py = round(x + (ancho - iw) / 2), round(y + (alto - ih) / 2)
            zona = hoja.crop((px, py, px + iw, py + ih))
            from PIL import ImageChops
            hoja.paste(ImageChops.multiply(zona, img), (px, py))
        else:
            f = fuente(e["puntos"], e["negrita"])
            lineas = envolver(e["texto"], ancho, f) if e["variasLineas"] else [e["texto"]]
            alto_linea = sum(f.getmetrics())
            for i, linea in enumerate(lineas):
                tw = f.getlength(linea)
                if e["alineacion"] == 1:
                    tx = x + (ancho - tw) / 2
                elif e["alineacion"] == 2:
                    tx = x + ancho - tw
                else:
                    tx = x
                d.text((tx, y + i * alto_linea), linea, font=f, fill=color(e["color"]))
    hoja.save(sys.argv[1] if len(sys.argv) > 1 else "comprobante.png")


if __name__ == "__main__":
    main()
