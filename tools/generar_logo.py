"""Genera el logo de Beneficio Unimax: solo texto, como el rótulo de la fachada
("BENEFICIO DE CAFÉ" en negro y "UNIMAX" en letra gruesa inclinada color café),
con detalles discretos de granos de café.

Archivos que genera:
  logo/logo-unimax.svg        logo principal (vectorial, para imprenta o rótulos)
  logo/logo-unimax.png        logo principal en alta resolución (fondo transparente)
  logo/logo-unimax-chico.png  logo para las plantillas de Excel
  logo/icono-unimax.svg/.png  ícono cuadrado
  src/win/app.ico             ícono del programa (varios tamaños)
  src/win/logo_encabezado.bmp logo para el encabezado de la ventana del programa
  src/win/logo_documento.bmp  logo del comprobante (pantalla, impresora y PDF)
  logo/marca-agua-1/2.png     marcas de agua (PNG transparente) y src/win/marca1/2.bmp

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

CAFE = "#4A1E1B"      # "UNIMAX" (café oscuro del rótulo)
NEGRO = "#1B1B1B"     # "BENEFICIO DE CAFÉ"
GRANO = "#8A5A3C"     # granos de café (tono más claro, para que sean discretos)
CREMA = "#F1EADB"     # fondo del ícono y del encabezado del programa

# Grano de café de 2 unidades de alto centrado en (0, 0): un óvalo con la hendidura en "S"
# recortada (máscara), así se ve bien sobre fondo transparente.
DEFS_GRANO = (
    '<defs><mask id="hendidura" maskContentUnits="userSpaceOnUse" x="-2" y="-2" width="4" height="4">'
    '<rect x="-2" y="-2" width="4" height="4" fill="#fff"/>'
    '<path d="M0.05,-1.02 C-0.42,-0.42 0.42,0.42 -0.05,1.02" fill="none" stroke="#000" stroke-width="0.16" '
    'stroke-linecap="round"/></mask>'
    '<g id="grano"><ellipse rx="0.66" ry="1" mask="url(#hendidura)"/></g></defs>'
)


def grano(x, y, alto, angulo, color=GRANO):
    return (f'<use href="#grano" fill="{color}" '
            f'transform="translate({x:.1f},{y:.1f}) rotate({angulo}) scale({alto / 2:.2f})"/>')


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
    cx = 600
    linea1, ancho1 = texto_svg(NEGRA, "BENEFICIO DE CAFÉ", 58, cx, 118, espaciado=9, color=NEGRO)
    linea2, ancho2 = texto_svg(NEGRA_ITALICA, "UNIMAX", 210, cx, 318, espaciado=10, color=CAFE)
    piezas = [DEFS_GRANO, linea1, linea2]
    # Un grano a cada lado de "BENEFICIO DE CAFÉ"
    sep = ancho1 / 2 + 46
    piezas.append(grano(cx - sep, 97, 46, -32))
    piezas.append(grano(cx + sep, 97, 46, 32))
    # Línea fina bajo "UNIMAX" interrumpida por tres granos pequeños
    y = 352
    mitad = ancho2 / 2
    piezas.append(f'<rect x="{cx - mitad:.1f}" y="{y - 2.5}" width="{mitad - 70:.1f}" height="5" rx="2.5" fill="{CAFE}"/>')
    piezas.append(f'<rect x="{cx + 70:.1f}" y="{y - 2.5}" width="{mitad - 70:.1f}" height="5" rx="2.5" fill="{CAFE}"/>')
    for dx, ang in ((-38, -70), (0, 90), (38, 70)):
        piezas.append(grano(cx + dx, y, 26, ang))
    izquierda = cx - max(sep + 30, mitad) - 24
    ancho = 2 * (cx - izquierda)
    cuerpo = "\n  ".join(piezas)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
            f'viewBox="{izquierda:.0f} 40 {ancho:.0f} 340" width="{ancho:.0f}" height="340">\n'
            f'  <title>Beneficio de Café Unimax</title>\n  {cuerpo}\n</svg>\n')


def icono_svg(pequeno=False):
    if pequeno:
        # Para 16-32 px: cuadro café con la U crema (los detalles no se verían).
        u, _ = texto_svg(NEGRA_ITALICA, "U", 400, 262, 404, color=CREMA)
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">'
                f'<rect x="8" y="8" width="496" height="496" rx="88" fill="{CAFE}"/>{u}</svg>')
    # U grande con un grano de café dentro de su curva.
    u, _ = texto_svg(NEGRA_ITALICA, "U", 420, 262, 432, color=CAFE)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">'
            f'{DEFS_GRANO}<rect x="8" y="8" width="496" height="496" rx="88" fill="{CREMA}"/>{u}'
            f'{grano(277, 222, 72, 13)}</svg>')


def texto_en_arco(fuente, texto, cx, cy, radio, tam, color, arriba=True, espaciado=0.0):
    """Texto curvo centrado arriba (se lee en el sentido del reloj) o abajo (se lee derecho).
    En ambos casos las letras ocupan la franja entre "radio" y "radio + alto de mayúscula"."""
    import math
    f = TTFont(fuente)
    glifos = f.getGlyphSet()
    cmap = f.getBestCmap()
    escala = tam / f["head"].unitsPerEm
    mayuscula = f["OS/2"].sCapHeight * escala
    nombres = [cmap[ord(c)] for c in texto]
    avances = [glifos[n].width * escala for n in nombres]
    r = radio if arriba else radio + mayuscula  # radio de la línea base
    total = sum(avances) + espaciado * (len(nombres) - 1)
    pen = SVGPathPen(glifos)
    recorrido = -total / 2
    for n, a in zip(nombres, avances):
        centro = recorrido + a / 2
        if arriba:
            theta = -math.pi / 2 + centro / r
            phi = theta + math.pi / 2
        else:
            theta = math.pi / 2 - centro / r
            phi = theta - math.pi / 2
        tx, ty = cx + r * math.cos(theta), cy + r * math.sin(theta)
        cos_p, sin_p = math.cos(phi), math.sin(phi)
        matriz = (cos_p * escala, sin_p * escala, sin_p * escala, -cos_p * escala,
                  -cos_p * a / 2 + tx, -sin_p * a / 2 + ty)
        glifos[n].draw(TransformPen(pen, matriz))
        recorrido += a + espaciado
    return f'<path fill="{color}" d="{pen.getCommands()}"/>'


def marca_diagonal_svg(opacidad=0.10):
    """Opción 1: UNIMAX en diagonal con granos de café."""
    linea1, ancho1 = texto_svg(NEGRA, "BENEFICIO DE CAFÉ", 40, 300, 228, espaciado=6, color=CAFE)
    linea2, ancho2 = texto_svg(NEGRA_ITALICA, "UNIMAX", 150, 300, 368, espaciado=6, color=CAFE)
    sep = ancho1 / 2 + 32
    granos = [grano(300 - sep, 214, 34, -32, CAFE), grano(300 + sep, 214, 34, 32, CAFE)]
    granos += [grano(312 + dx, 410, 22, ang, CAFE) for dx, ang in ((-30, -70), (0, 90), (30, 70))]
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 600 600" width="600" height="600">{DEFS_GRANO}'
            f'<g opacity="{opacidad}" transform="rotate(-32 300 300)">{linea1}{linea2}{"".join(granos)}</g></svg>')


def marca_sello_svg(opacidad=0.10):
    """Opción 2: sello redondo con un grano de café al centro."""
    arriba = texto_en_arco(NEGRA, "BENEFICIO DE CAFÉ", 300, 300, 214, 40, CAFE, arriba=True, espaciado=5)
    abajo = texto_en_arco(NEGRA_ITALICA, "UNIMAX", 300, 300, 214, 46, CAFE, arriba=False, espaciado=12)
    separadores = grano(300 - 235, 300, 26, 0, CAFE) + grano(300 + 235, 300, 26, 0, CAFE)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 600 600" width="600" height="600">{DEFS_GRANO}'
            f'<g opacity="{opacidad}">'
            f'<circle cx="300" cy="300" r="284" fill="none" stroke="{CAFE}" stroke-width="10"/>'
            f'<circle cx="300" cy="300" r="196" fill="none" stroke="{CAFE}" stroke-width="4"/>'
            f'{arriba}{abajo}{separadores}{grano(300, 300, 240, 22, CAFE)}</g></svg>')


def bmp_8bits(imagen, ruta):
    """BMP de 256 colores (más liviano dentro del .exe); GDI lo dibuja igual en pantalla e impresora."""
    imagen.convert("RGB").quantize(colors=256, method=Image.Quantize.MEDIANCUT).save(ruta)


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

    # Encabezado de la ventana: fondo crema, grande para que se vea nítido con escala de pantalla.
    bmp_8bits(rasterizar(svg, 560, fondo=CREMA), WIN / "logo_encabezado.bmp")
    # Logo del comprobante (pantalla, impresora y PDF): fondo blanco, buena resolución para imprimir.
    bmp_8bits(rasterizar(svg, 900, fondo="#FFFFFF"), WIN / "logo_documento.bmp")

    # Marcas de agua: PNG transparente (Excel y uso general) y BMP sobre blanco (programa e impresión).
    for numero, crear in ((1, marca_diagonal_svg), (2, marca_sello_svg)):
        marca = crear()
        (LOGO / f"marca-agua-{numero}.svg").write_text(marca, encoding="utf-8")
        rasterizar(marca, 900).save(LOGO / f"marca-agua-{numero}.png", optimize=True)
        bmp_8bits(rasterizar(marca, 600, fondo="#FFFFFF"), WIN / f"marca{numero}.bmp")
        # Versión un poco más visible para ver el diseño (no se usa en el programa).
        rasterizar(crear(opacidad=0.9), 600, fondo="#FFFFFF").save(LOGO / f"marca-agua-{numero}-diseno.png")
    print("Logo generado en", LOGO, "y", WIN)


if __name__ == "__main__":
    main()
