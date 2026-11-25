"""Genera las plantillas de Excel de Beneficio Unimax (carpeta ../plantillas).

Cada plantilla tiene "nombres definidos" (Fórmulas > Administrador de nombres)
que el programa usa para saber dónde escribir cada dato. Puede cambiar el
diseño de las plantillas en Excel (colores, logotipo, posiciones) siempre que
conserve esos nombres. Las tablas crecen hacia abajo desde su celda de inicio,
así que deje libre el espacio debajo de ellas.

Uso:  python tools/generar_plantillas.py
"""
from pathlib import Path

from openpyxl import Workbook
from openpyxl.drawing.image import Image as ImagenExcel
from openpyxl.drawing.spreadsheet_drawing import AnchorMarker, OneCellAnchor
from openpyxl.drawing.xdr import XDRPositiveSize2D
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import column_index_from_string, get_column_letter
from openpyxl.utils.units import pixels_to_EMU
from openpyxl.workbook.defined_name import DefinedName

CAFE = "5B3A29"
CREMA = "F5EBDD"
GRIS = "666666"
FMT_L = '"L "#,##0.00'
FMT_PESO = "#,##0.00"
FMT_PRECIO = "#,##0.00##"
FMT_FECHA = "dd/mm/yyyy"

DELGADA = Side(style="thin", color="999999")
BORDE = Border(left=DELGADA, right=DELGADA, top=DELGADA, bottom=DELGADA)
LINEA_FIRMA = Border(top=Side(style="thin", color="000000"))

SALIDA = Path(__file__).resolve().parent.parent / "plantillas"
# Logo (se genera con tools/generar_logo.py)
LOGO = Path(__file__).resolve().parent.parent / "logo" / "logo-unimax-chico.png"


class Hoja:
    def __init__(self, wb, ws):
        self.wb = wb
        self.ws = ws

    def nombre(self, nombre, celda):
        col = "".join(ch for ch in celda if ch.isalpha())
        fila = "".join(ch for ch in celda if ch.isdigit())
        ref = f"'{self.ws.title}'!${col}${fila}"
        self.wb.defined_names[nombre] = DefinedName(nombre, attr_text=ref)

    def anchos(self, anchos):
        for col, ancho in anchos.items():
            self.ws.column_dimensions[col].width = ancho

    def texto(self, celda, valor, negrita=False, tam=10, color=None, alinear=None, fondo=None, combinar=None,
              ajustar=False):
        if combinar:
            self.ws.merge_cells(combinar)
        c = self.ws[celda]
        c.value = valor
        c.font = Font(name="Calibri", size=tam, bold=negrita, color=color)
        c.alignment = Alignment(horizontal=alinear, vertical="center", wrap_text=ajustar)
        if fondo:
            c.fill = PatternFill("solid", fgColor=fondo)
        return c

    def campo(self, nombre, celda, formato=None, negrita=False, tam=10, alinear=None, combinar=None, color=None,
              ajustar=False, borde=False):
        c = self.texto(celda, None, negrita=negrita, tam=tam, alinear=alinear, combinar=combinar, color=color,
                       ajustar=ajustar)
        # Sin formato numérico = celda de texto ("@"), para que Excel no convierta
        # "000123" en 123 ni interprete como fórmula un texto que empiece con "=" o "-".
        c.number_format = formato or "@"
        if borde:
            c.border = Border(bottom=DELGADA)
        self.nombre(nombre, celda)
        return c

    def etiqueta(self, celda, valor, combinar=None, alinear="left"):
        return self.texto(celda, valor, negrita=True, color=CAFE, alinear=alinear, combinar=combinar)

    def px_columna(self, col):
        ancho = self.ws.column_dimensions[get_column_letter(col)].width or 8.43
        return int(ancho * 7 + 5)

    def logo(self, col, desplazamiento_px, alto_px, fila_px=4):
        """Inserta el logo con la esquina superior izquierda en la columna "col" (1 = A)."""
        if not LOGO.exists():
            return
        img = ImagenExcel(str(LOGO))
        ancho_px = round(img.width * alto_px / img.height)
        img.width, img.height = ancho_px, alto_px
        img.anchor = OneCellAnchor(
            _from=AnchorMarker(col=col - 1, colOff=pixels_to_EMU(desplazamiento_px), row=0,
                               rowOff=pixels_to_EMU(fila_px)),
            ext=XDRPositiveSize2D(pixels_to_EMU(ancho_px), pixels_to_EMU(alto_px)))
        self.ws.add_image(img)

    def logo_derecha(self, ultima_col, alto_px=50):
        """Logo en la esquina superior derecha, terminando en la columna "ultima_col"."""
        if not LOGO.exists():
            return
        ancho_px = round(ImagenExcel(str(LOGO)).width * alto_px / ImagenExcel(str(LOGO)).height)
        restante = ancho_px + 6
        col = column_index_from_string(ultima_col)
        while col > 1 and restante > self.px_columna(col):
            restante -= self.px_columna(col)
            col -= 1
        self.logo(col, max(0, self.px_columna(col) - restante), alto_px)

    def encabezado_empresa(self, ultima_col, titulo, fila_titulo=4):
        self.campo("EMPRESA", "A1", negrita=True, tam=18, color=CAFE, combinar=f"A1:{ultima_col}1")
        self.campo("EMPRESA_DATOS", "A2", tam=9, color=GRIS, combinar=f"A2:{ultima_col}2")
        self.logo_derecha(ultima_col)
        self.ws.row_dimensions[1].height = 28
        self.texto(f"A{fila_titulo}", titulo, negrita=True, tam=14)

    def encabezados_tabla(self, fila, primera_col, titulos):
        for i, t in enumerate(titulos):
            col = chr(ord(primera_col) + i)
            c = self.texto(f"{col}{fila}", t, negrita=True, color="FFFFFF", fondo=CAFE, alinear="center", ajustar=True)
            c.border = BORDE
        self.ws.row_dimensions[fila].height = 30

    def pagina(self, horizontal=False, titulos=None):
        ps = self.ws.page_setup
        ps.orientation = "landscape" if horizontal else "portrait"
        ps.paperSize = self.ws.PAPERSIZE_LETTER
        ps.fitToWidth = 1
        ps.fitToHeight = 0
        self.ws.sheet_properties.pageSetUpPr.fitToPage = True
        self.ws.page_margins.left = self.ws.page_margins.right = 0.4
        self.ws.page_margins.top = self.ws.page_margins.bottom = 0.5
        if titulos:
            self.ws.print_title_rows = titulos
        self.ws.sheet_view.showGridLines = False


def nuevo(titulo_hoja):
    wb = Workbook()
    ws = wb.active
    ws.title = titulo_hoja
    return wb, Hoja(wb, ws)


# ---------------------------------------------------------------------------
def factura():
    wb, h = nuevo("Comprobante")
    ws = h.ws
    h.anchos({"A": 24, "B": 13, "C": 12, "D": 11, "E": 14, "F": 17})
    # Logo a la izquierda (filas 1 a 3) y datos de la empresa a la derecha.
    h.logo(1, 2, 58, fila_px=3)
    h.campo("EMPRESA", "B1", negrita=True, tam=18, color=CAFE, combinar="B1:F1", alinear="center")
    h.campo("EMPRESA_DATOS", "B2", tam=9, color=GRIS, combinar="B2:F2", alinear="center", ajustar=True)
    ws.row_dimensions[2].height = 24
    ws.row_dimensions[1].height = 28
    h.texto("A4", "COMPROBANTE DE COMPRA DE CAFÉ", negrita=True, tam=14, combinar="A4:D4")
    h.etiqueta("E4", "No.", alinear="right")
    h.campo("NUMERO", "F4", negrita=True, tam=14, color="C00000", alinear="center")
    h.etiqueta("E5", "Fecha:", alinear="right")
    h.campo("FECHA", "F5", formato=FMT_FECHA, alinear="center")
    h.campo("ESTADO", "A6", negrita=True, tam=16, color="C00000", combinar="A6:F6", alinear="center")

    h.etiqueta("A7", "Productor / cliente:")
    h.campo("CLIENTE", "B7", combinar="B7:F7", borde=True)
    h.etiqueta("A8", "DNI / RTN:")
    h.campo("DNI", "B8", combinar="B8:C8", borde=True)
    h.etiqueta("D8", "Teléfono:", alinear="right")
    h.campo("TELEFONO", "E8", combinar="E8:F8", borde=True)

    h.encabezados_tabla(10, "A", ["Tipo de café", "Libras", "Quintales", "Galones", "Precio por libra", "Subtotal"])
    formatos = [None, FMT_PESO, FMT_PESO, FMT_PESO, FMT_PRECIO, FMT_L]
    for fila in range(11, 21):
        for i, fmt in enumerate(formatos):
            c = ws.cell(row=fila, column=i + 1)
            c.border = BORDE
            c.font = Font(name="Calibri", size=10)
            if fmt:
                c.number_format = fmt
    h.nombre("DETALLE", "A11")

    h.texto("A21", "TOTALES", negrita=True, alinear="right")
    h.campo("TOTAL_LIBRAS", "B21", formato=FMT_PESO, negrita=True)
    h.campo("TOTAL_QUINTALES", "C21", formato=FMT_PESO, negrita=True)
    h.campo("TOTAL_GALONES", "D21", formato=FMT_PESO, negrita=True)
    h.etiqueta("E21", "TOTAL:", alinear="right")
    h.campo("TOTAL", "F21", formato=FMT_L, negrita=True, tam=12)
    for col in "ABCDEF":
        ws[f"{col}21"].fill = PatternFill("solid", fgColor=CREMA)
        ws[f"{col}21"].border = BORDE

    h.etiqueta("D23", "Pagado en efectivo:", combinar="D23:E23", alinear="right")
    h.campo("PAGADO", "F23", formato=FMT_L)
    h.etiqueta("D24", "Saldo pendiente:", combinar="D24:E24", alinear="right")
    h.campo("SALDO", "F24", formato=FMT_L, negrita=True)
    h.etiqueta("D25", "Fecha de pago del saldo:", combinar="D25:E25", alinear="right")
    h.campo("VENCE", "F25", formato=FMT_FECHA, alinear="right")

    h.etiqueta("A23", "Total en letras:")
    h.campo("TOTAL_LETRAS", "A24", combinar="A24:C25", ajustar=True)
    ws["A24"].alignment = Alignment(wrap_text=True, vertical="top")

    h.etiqueta("A27", "Observaciones:")
    h.campo("OBSERVACIONES", "B27", combinar="B27:F27", borde=True)

    h.texto("A32", "Firma del productor", alinear="center", combinar="A32:B32").border = LINEA_FIRMA
    ws["B32"].border = LINEA_FIRMA
    h.texto("D32", "Entregado por (beneficio)", alinear="center", combinar="D32:F32").border = LINEA_FIRMA
    for col in "EF":
        ws[f"{col}32"].border = LINEA_FIRMA
    h.texto("A34", "Pago calculado por peso en libras. 1 quintal = 100 libras. Los galones son un dato de referencia.",
            tam=8, color=GRIS, combinar="A34:F34", alinear="center")
    h.pagina()
    ws.print_area = "A1:F34"
    return wb


def cierre_caja():
    wb, h = nuevo("Caja")
    ws = h.ws
    h.anchos({"A": 8, "B": 9, "C": 17, "D": 40, "E": 16, "F": 15, "G": 15})
    h.encabezado_empresa("G", "CIERRE DE CAJA DIARIO")
    ws.merge_cells("A4:D4")
    h.etiqueta("E4", "Fecha:", alinear="right")
    h.campo("FECHA", "F4", formato=FMT_FECHA, negrita=True, combinar="F4:G4", alinear="center")
    h.etiqueta("E5", "Estado:", alinear="right")
    h.campo("ESTADO", "F5", combinar="F5:G5", alinear="center")

    filas = [
        ("Restante del día anterior (saldo inicial)", "SALDO_INICIAL", False),
        ("Más: entradas del día", "INGRESOS", False),
        ("Menos: salidas del día", "EGRESOS", False),
        ("Debe haber en caja", "ESPERADO", True),
        ("Dinero contado en caja", "CONTADO", False),
        ("Diferencia (contado − debe haber)", "DIFERENCIA", True),
    ]
    for i, (texto, nombre, negrita) in enumerate(filas):
        fila = 7 + i
        h.texto(f"A{fila}", texto, negrita=negrita, combinar=f"A{fila}:C{fila}", fondo=CREMA)
        h.campo(nombre, f"D{fila}", formato=FMT_L, negrita=negrita, alinear="right")
        for col in "ABCD":
            ws[f"{col}{fila}"].border = BORDE
    h.etiqueta("E7", "Notas:")
    h.campo("NOTAS", "E8", combinar="E8:G12", ajustar=True)
    ws["E8"].alignment = Alignment(wrap_text=True, vertical="top")

    h.texto("A14", "MOVIMIENTOS DEL DÍA", negrita=True, tam=12, color=CAFE)
    h.encabezados_tabla(15, "A", ["Hora", "Tipo", "Origen", "Concepto", "Referencia", "Entrada", "Salida"])
    h.nombre("MOVIMIENTOS", "A16")
    h.pagina(titulos="15:15")
    return wb


def estado_cuenta():
    wb, h = nuevo("Estado de cuenta")
    ws = h.ws
    h.anchos({"A": 7, "B": 11, "C": 15, "D": 13, "E": 36, "F": 11, "G": 14, "H": 14, "I": 14, "J": 20})
    h.encabezado_empresa("J", "ESTADO DE CUENTA DEL CLIENTE")
    ws.merge_cells("A4:F4")
    h.etiqueta("H4", "Fecha:", alinear="right")
    h.campo("FECHA", "I4", formato=FMT_FECHA, combinar="I4:J4", alinear="center")
    h.etiqueta("A6", "Cliente:", combinar="A6:B6")
    h.campo("CLIENTE", "C6", negrita=True, combinar="C6:F6", borde=True)
    h.etiqueta("G6", "DNI / RTN:", alinear="right")
    h.campo("DNI", "H6", combinar="H6:J6", borde=True)
    h.etiqueta("A7", "Teléfono:", combinar="A7:B7")
    h.campo("TELEFONO", "C7", combinar="C7:F7", borde=True)
    h.texto("A9", "Total que el beneficio le debe al cliente (por pagar):", combinar="A9:E9", fondo=CREMA)
    h.campo("POR_PAGAR", "F9", formato=FMT_L, negrita=True, combinar="F9:G9", alinear="right")
    h.texto("A10", "Total que el cliente le debe al beneficio (por cobrar):", combinar="A10:E10", fondo=CREMA)
    h.campo("POR_COBRAR", "F10", formato=FMT_L, negrita=True, combinar="F10:G10", alinear="right")
    h.campo("CONCLUSION", "A11", negrita=True, tam=11, color=CAFE, combinar="A11:J11")
    h.texto("A13", "DEUDAS", negrita=True, tam=12, color=CAFE)
    h.encabezados_tabla(14, "A", ["No.", "Fecha", "Tipo", "Origen", "Concepto", "Fecha de pago", "Monto",
                                  "Abonado", "Saldo", "Estado"])
    h.nombre("DEUDAS", "A15")
    h.pagina(horizontal=True, titulos="14:14")

    ws2 = wb.create_sheet("Abonos")
    h2 = Hoja(wb, ws2)
    h2.anchos({"A": 12, "B": 10, "C": 16, "D": 15, "E": 40, "F": 12})
    h2.texto("A1", "ABONOS REGISTRADOS", negrita=True, tam=14, color=CAFE)
    h2.encabezados_tabla(3, "A", ["Fecha", "Deuda No.", "Tipo", "Monto", "Nota", "Estado"])
    h2.nombre("ABONOS", "A4")
    h2.pagina(titulos="3:3")
    return wb


def reporte_deudas():
    wb, h = nuevo("Deudas")
    ws = h.ws
    h.anchos({"A": 7, "B": 28, "C": 17, "D": 15, "E": 13, "F": 34, "G": 11, "H": 11, "I": 14, "J": 14, "K": 20})
    h.encabezado_empresa("K", "DEUDAS PENDIENTES")
    ws.merge_cells("A4:F4")
    h.etiqueta("I4", "Al:", alinear="right")
    h.campo("FECHA", "J4", formato=FMT_FECHA, combinar="J4:K4", alinear="center")
    h.texto("A6", "El beneficio debe (por pagar):", combinar="A6:C6", fondo=CREMA)
    h.campo("POR_PAGAR", "D6", formato=FMT_L, negrita=True, alinear="right")
    h.campo("CANT_POR_PAGAR", "E6", formato='0 "deudas"')
    h.texto("A7", "Le deben al beneficio (por cobrar):", combinar="A7:C7", fondo=CREMA)
    h.campo("POR_COBRAR", "D7", formato=FMT_L, negrita=True, alinear="right")
    h.campo("CANT_POR_COBRAR", "E7", formato='0 "deudas"')
    h.texto("A8", "Deudas vencidas:", combinar="A8:C8", fondo=CREMA)
    h.campo("CANT_VENCIDAS", "D8", formato="0", negrita=True, color="C00000", alinear="right")
    h.encabezados_tabla(10, "A", ["No.", "Cliente", "DNI / RTN", "Tipo", "Origen", "Concepto", "Fecha",
                                  "Fecha de pago", "Monto", "Saldo", "Estado"])
    h.nombre("DEUDAS", "A11")
    h.pagina(horizontal=True, titulos="10:10")
    return wb


def reporte_compras():
    wb, h = nuevo("Compras")
    ws = h.ws
    h.anchos({"A": 12, "B": 11, "C": 28, "D": 17, "E": 17, "F": 12, "G": 12, "H": 15, "I": 12, "J": 15})
    h.encabezado_empresa("J", "REPORTE DE COMPRAS DE CAFÉ")
    ws.merge_cells("A4:E4")
    h.etiqueta("F4", "Del:", alinear="right")
    h.campo("DESDE", "G4", formato=FMT_FECHA, alinear="center")
    h.etiqueta("H4", "al:", alinear="right")
    h.campo("HASTA", "I4", formato=FMT_FECHA, alinear="center")
    h.texto("A6", "Comprobantes:", combinar="A6:B6", fondo=CREMA)
    h.campo("CANT_COMPROBANTES", "C6", formato="0", negrita=True, alinear="left")
    h.texto("A7", "Total libras:", combinar="A7:B7", fondo=CREMA)
    h.campo("TOTAL_LIBRAS", "C7", formato=FMT_PESO, negrita=True, alinear="left")
    h.texto("A8", "Total quintales:", combinar="A8:B8", fondo=CREMA)
    h.campo("TOTAL_QUINTALES", "C8", formato=FMT_PESO, negrita=True, alinear="left")
    h.texto("E6", "Total comprado:", fondo=CREMA)
    h.campo("TOTAL_MONTO", "F6", formato=FMT_L, negrita=True, combinar="F6:G6", alinear="right")
    h.texto("E7", "Pagado en efectivo:", fondo=CREMA)
    h.campo("TOTAL_PAGADO", "F7", formato=FMT_L, combinar="F7:G7", alinear="right")
    h.texto("E8", "Saldo pendiente:", fondo=CREMA)
    h.campo("TOTAL_SALDO", "F8", formato=FMT_L, combinar="F8:G8", alinear="right")

    h.texto("C10", "RESUMEN POR TIPO DE CAFÉ", negrita=True, tam=12, color=CAFE)
    h.encabezados_tabla(11, "C", ["Tipo de café", "Libras", "Quintales", "Galones", "Precio promedio / lb", "Monto"])
    for fila in range(12, 15):
        for col in "CDEFGH":
            ws[f"{col}{fila}"].border = BORDE
    h.nombre("RESUMEN", "C12")

    h.texto("A16", "DETALLE DE COMPRAS (no incluye comprobantes anulados)", negrita=True, tam=12, color=CAFE)
    h.encabezados_tabla(17, "A", ["Comprobante", "Fecha", "Productor", "DNI / RTN", "Tipo de café", "Libras",
                                  "Quintales", "Galones", "Precio / lb", "Subtotal"])
    h.nombre("DETALLE", "A18")
    h.pagina(horizontal=True, titulos="17:17")
    return wb


def reporte_ventas():
    wb, h = nuevo("Ventas")
    ws = h.ws
    h.anchos({"A": 8, "B": 11, "C": 28, "D": 17, "E": 17, "F": 15, "G": 12, "H": 12, "I": 15, "J": 15, "K": 15})
    h.encabezado_empresa("K", "REPORTE DE VENTAS DE CAFÉ")
    ws.merge_cells("A4:E4")
    h.etiqueta("G4", "Del:", alinear="right")
    h.campo("DESDE", "H4", formato=FMT_FECHA, alinear="center")
    h.etiqueta("I4", "al:", alinear="right")
    h.campo("HASTA", "J4", formato=FMT_FECHA, alinear="center")
    h.texto("A6", "Ventas:", combinar="A6:B6", fondo=CREMA)
    h.campo("CANT_VENTAS", "C6", formato="0", negrita=True, alinear="left")
    h.texto("A7", "Total libras:", combinar="A7:B7", fondo=CREMA)
    h.campo("TOTAL_LIBRAS", "C7", formato=FMT_PESO, negrita=True, alinear="left")
    h.texto("A8", "Total quintales:", combinar="A8:B8", fondo=CREMA)
    h.campo("TOTAL_QUINTALES", "C8", formato=FMT_PESO, negrita=True, alinear="left")
    h.texto("E6", "Total vendido:", fondo=CREMA)
    h.campo("TOTAL_MONTO", "F6", formato=FMT_L, negrita=True, alinear="right")
    h.texto("E7", "Cobrado en efectivo:", fondo=CREMA)
    h.campo("TOTAL_RECIBIDO", "F7", formato=FMT_L, alinear="right")
    h.texto("E8", "Saldo por cobrar:", fondo=CREMA)
    h.campo("TOTAL_SALDO", "F8", formato=FMT_L, alinear="right")

    h.texto("C10", "RESUMEN POR TIPO DE CAFÉ", negrita=True, tam=12, color=CAFE)
    h.encabezados_tabla(11, "C", ["Tipo de café", "Libras", "Quintales", "Monto"])
    for fila in range(12, 15):
        for col in "CDEF":
            ws[f"{col}{fila}"].border = BORDE
    h.nombre("RESUMEN", "C12")

    h.texto("A16", "DETALLE DE VENTAS (no incluye ventas anuladas)", negrita=True, tam=12, color=CAFE)
    h.encabezados_tabla(17, "A", ["Venta No.", "Fecha", "Comprador", "DNI / RTN", "Tipo de café", "Libras",
                                  "Quintales", "Precio / lb", "Total", "Recibido", "Saldo"])
    h.nombre("DETALLE", "A18")
    h.pagina(horizontal=True, titulos="17:17")
    return wb


def reporte_caja():
    wb, h = nuevo("Caja")
    ws = h.ws
    h.anchos({"A": 12, "B": 15, "C": 15, "D": 15, "E": 15, "F": 15, "G": 15, "H": 36})
    h.encabezado_empresa("H", "RESUMEN DE CAJA POR DÍA")
    ws.merge_cells("A4:D4")
    h.etiqueta("E4", "Del:", alinear="right")
    h.campo("DESDE", "F4", formato=FMT_FECHA, alinear="center")
    h.etiqueta("G4", "al:", alinear="right")
    h.campo("HASTA", "H4", formato=FMT_FECHA, alinear="left")
    h.texto("A6", "Días cerrados:", combinar="A6:B6", fondo=CREMA)
    h.campo("DIAS_CERRADOS", "C6", formato="0", negrita=True, alinear="right")
    h.texto("A7", "Total entradas:", combinar="A7:B7", fondo=CREMA)
    h.campo("TOTAL_ENTRADAS", "C7", formato=FMT_L, alinear="right")
    h.texto("A8", "Total salidas:", combinar="A8:B8", fondo=CREMA)
    h.campo("TOTAL_SALIDAS", "C8", formato=FMT_L, alinear="right")
    h.texto("A9", "Suma de diferencias:", combinar="A9:B9", fondo=CREMA)
    h.campo("TOTAL_DIFERENCIAS", "C9", formato=FMT_L, negrita=True, alinear="right")
    h.texto("A11", "Solo incluye los días con caja cerrada.", tam=9, color=GRIS, combinar="A11:E11")
    h.encabezados_tabla(12, "A", ["Fecha", "Saldo inicial", "Entradas", "Salidas", "Debe haber", "Contado",
                                  "Diferencia", "Notas"])
    h.nombre("DIAS", "A13")
    h.pagina(horizontal=True, titulos="12:12")
    return wb


def reporte_general():
    wb, h = nuevo("General")
    ws = h.ws
    h.anchos({"A": 12, "B": 13, "C": 11, "D": 13, "E": 11, "F": 13, "G": 11, "H": 13, "I": 11, "J": 12})
    h.encabezado_empresa("J", "REPORTE GENERAL DE COMPRAS POR DÍA")
    ws.merge_cells("A4:F4")
    h.etiqueta("G4", "Del:", alinear="right")
    h.campo("DESDE", "H4", formato=FMT_FECHA, alinear="center")
    h.etiqueta("I4", "al:", alinear="right")
    h.campo("HASTA", "J4", formato=FMT_FECHA, alinear="center")
    filas = [("Uva", "UVA_LB", "UVA_QQ"), ("Pergamino húmedo", "HUMEDO_LB", "HUMEDO_QQ"),
             ("Pergamino seco", "SECO_LB", "SECO_QQ")]
    h.encabezados_tabla(6, "A", ["Tipo de café", "Libras", "Quintales"])
    for i, (texto, lb, qq) in enumerate(filas):
        fila = 7 + i
        h.texto(f"A{fila}", texto, negrita=True, fondo=CREMA)
        h.campo(lb, f"B{fila}", formato=FMT_PESO, alinear="right")
        h.campo(qq, f"C{fila}", formato=FMT_PESO, negrita=True, alinear="right")
        for col in "ABC":
            ws[f"{col}{fila}"].border = BORDE
    h.texto("E7", "Total pergamino (qq):", negrita=True, combinar="E7:F7", fondo=CREMA)
    h.campo("PERGAMINO_QQ", "G7", formato=FMT_PESO, negrita=True, alinear="right")
    h.texto("E8", "Días con compras:", combinar="E8:F8", fondo=CREMA)
    h.campo("DIAS_COMPRA", "G8", formato="0", alinear="right")
    h.texto("A11", "DETALLE POR DÍA (no incluye facturas anuladas)", negrita=True, tam=12, color=CAFE)
    h.encabezados_tabla(12, "A", ["Fecha", "Uva lb", "Uva qq", "P. húmedo lb", "P. húmedo qq", "P. seco lb",
                                  "P. seco qq", "Pergamino total qq", "Facturas uva", "Facturas pergamino"])
    h.nombre("DIAS", "A13")
    h.pagina(horizontal=True, titulos="12:12")
    return wb


PLANTILLAS = {
    "Factura.xlsx": factura,
    "CierreCaja.xlsx": cierre_caja,
    "EstadoCuenta.xlsx": estado_cuenta,
    "ReporteDeudas.xlsx": reporte_deudas,
    "ReporteCompras.xlsx": reporte_compras,
    "ReporteVentas.xlsx": reporte_ventas,
    "ReporteCaja.xlsx": reporte_caja,
    "ReporteGeneral.xlsx": reporte_general,
}


def main():
    SALIDA.mkdir(exist_ok=True)
    for archivo, funcion in PLANTILLAS.items():
        wb = funcion()
        wb.properties.creator = "Beneficio Unimax"
        wb.save(SALIDA / archivo)
        print("Generada", SALIDA / archivo)


if __name__ == "__main__":
    main()
