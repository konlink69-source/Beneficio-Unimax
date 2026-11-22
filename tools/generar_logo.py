"""Genera el logo de Beneficio Unimax inspirado en la fachada del beneficio:
muro crema con marco café, techo a dos aguas, "BENEFICIO DE CAFÉ" en negro y
"UNIMAX" en letra gruesa inclinada color café.

Archivos que genera:
  logo/logo-unimax.svg        logo principal (vectorial, para imprenta o rótulos)
  logo/logo-unimax.png        logo principal en alta resolución (fondo transparente)
  logo/logo-unimax-chico.png  logo para las plantillas de Excel
  logo/icono-unimax.svg/.png  ícono cuadrado
  src/win/app.ico             ícono del programa (varios tamaños)
  src/win/logo_encabezado.bmp logo para el encabezado de la ventana del programa

Requisitos: fonttools, Pillow, rsvg-convert (librsvg2-bin) y la fuente Roboto
(paquete fonts-roboto-unhinted).

Uso:  python tools/generar_logo.py
"""
import io
import subprocess
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from PIL import Image

RAIZ = Path(__file__).resolve().parent.parent
LOGO = RAIZ / "logo"
WIN = RAIZ / "src" / "win"
FUENTES = Path("/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF")
NEGRA = FUENTES / "Roboto-Black.ttf"
NEGRA_ITALICA = FUENTES / "Roboto-BlackItalic.ttf"

CAFE = "#4A1E1B"      # café de los pilares y de "UNIMAX"
NEGRO = "#1B1B1B"     # "BENEFICIO DE CAFÉ"
CREMA = "#F1EADB"     # muro
TECHO = "#6F787F"     # lámina del techo


def texto_svg(fuente, texto, tam, centro_x, base_y, espaciado=0.0, color=NEGRO):
    """Convierte un texto en un <path> SVG centrado en centro_x (las letras quedan como trazos)."""
    f = TTFont(fuente)
    glifos = f.getGlyphSet()
    cmap = f.getBestCmap()
    escala = tam / f["head"].unitsPerEm
    nombres = [cmap[ord(c)] for c in texto]
    ancho = sum(glifos[n].width for n in nombres) * escala + espaciado * (len(nombres) - 1)
    pen = SVGPathPen(glifos)
    x = centro_x - ancho / 2
    for n in nombres:
        glifos[n].draw(TransformPen(pen, (escala, 0, 0, -escala, x, base_y)))
        x += glifos[n].width * escala + espaciado
    return f'<path fill="{color}" d="{pen.getCommands()}"/>', ancho


def logo_svg():
    piezas = []
    # Muro crema (el borde superior queda bajo el techo)
    piezas.append(f'<polygon fill="{CREMA}" points="112,170 600,100 1088,170 1088,440 112,440"/>')
    # Texto
    linea1, ancho1 = texto_svg(NEGRA, "BENEFICIO DE CAFÉ", 60, 600, 252, espaciado=7, color=NEGRO)
    linea2, ancho2 = texto_svg(NEGRA_ITALICA, "UNIMAX", 172, 600, 396, espaciado=8, color=CAFE)
    assert ancho1 < 840 and ancho2 < 860, (ancho1, ancho2)
    piezas += [linea1, linea2]
    # Marco: pilares y viga, como la fachada
    piezas.append(f'<rect x="112" y="150" width="42" height="300" fill="{CAFE}"/>')
    piezas.append(f'<rect x="1046" y="150" width="42" height="300" fill="{CAFE}"/>')
    piezas.append(f'<rect x="112" y="414" width="976" height="36" fill="{CAFE}"/>')
    # Techo de lámina a dos aguas (poca pendiente) y su cenefa café
    piezas.append(f'<polygon fill="{TECHO}" points="40,152 600,70 1160,152 1160,178 600,96 40,178"/>')
    piezas.append(f'<polygon fill="{CAFE}" points="112,167.5 600,96 1088,167.5 1088,181.5 600,110 112,181.5"/>')
    cuerpo = "\n  ".join(piezas)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="24 54 1152 412" width="1152" height="412">\n'
            f'  <title>Beneficio de Café Unimax</title>\n  {cuerpo}\n</svg>\n')


def icono_svg(pequeno=False):
    if pequeno:
        # Para 16-32 px: cuadro café con la U crema (los detalles no se verían).
        u, _ = texto_svg(NEGRA_ITALICA, "U", 400, 262, 404, color=CREMA)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">'
                f'<rect x="8" y="8" width="496" height="496" rx="88" fill="{CAFE}"/>{u}</svg>')
    u, _ = texto_svg(NEGRA_ITALICA, "U", 300, 262, 398, color=CAFE)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">'
            f'<rect x="8" y="8" width="496" height="496" rx="72" fill="{CREMA}"/>'
            f'{u}'
            f'<rect x="70" y="150" width="44" height="304" fill="{CAFE}"/>'
            f'<rect x="398" y="150" width="44" height="304" fill="{CAFE}"/>'
            f'<rect x="70" y="414" width="372" height="40" fill="{CAFE}"/>'
            f'<polygon fill="{CAFE}" points="30,170 256,56 482,170 482,206 256,92 30,206"/>'
            f'</svg>')


def rasterizar(svg, ancho, fondo=None):
    cmd = ["rsvg-convert", "-w", str(ancho), "-f", "png"]
    if fondo:
        cmd += ["-b", fondo]
    png = subprocess.run(cmd, input=svg.encode("utf-8"), capture_output=True, check=True).stdout
    return Image.open(io.BytesIO(png)).convert("RGBA")


def main():
    LOGO.mkdir(exist_ok=True)
    svg = logo_svg()
    (LOGO / "logo-unimax.svg").write_text(svg, encoding="utf-8")
    rasterizar(svg, 1600).save(LOGO / "logo-unimax.png", optimize=True)
    rasterizar(svg, 520).save(LOGO / "logo-unimax-chico.png", optimize=True)

    icono = icono_svg()
    (LOGO / "icono-unimax.svg").write_text(icono, encoding="utf-8")
    rasterizar(icono, 512).save(LOGO / "icono-unimax.png", optimize=True)

    # Ícono de Windows: versión simple en tamaños chicos, completa en grandes.
    pequeno = icono_svg(pequeno=True)
    tamanos = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
    imagenes = [rasterizar(pequeno if t <= 32 else icono, t) for t in tamanos]
    imagenes[-1].save(WIN / "app.ico", format="ICO", sizes=[(t, t) for t in tamanos], append_images=imagenes[:-1])

    # Encabezado de la ventana: fondo crema (sin transparencia, para GDI), al doble de tamaño
    # para que se vea nítido en pantallas con escala.
    encabezado = rasterizar(svg, 300, fondo=CREMA).convert("RGB")
    encabezado.save(WIN / "logo_encabezado.bmp")
    print("Logo generado en", LOGO, "y", WIN)


if __name__ == "__main__":
    main()
