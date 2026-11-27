#include "core/Grafico.h"

#include <algorithm>
#include <cstdio>

#include "core/NumeroLetras.h"
#include "core/Texto.h"

namespace bu {

namespace {

constexpr uint32_t CAFE = 0x4A1E1B;
constexpr uint32_t NEGRO = 0x1B1B1B;
constexpr uint32_t GRIS = 0x666666;
constexpr uint32_t GRIS_CLARO = 0x9A9A9A;
constexpr uint32_t CREMA = 0xF5EBDD;
constexpr uint32_t ROJO = 0xC00000;
constexpr uint32_t BLANCO = 0xFFFFFF;
constexpr uint32_t UVA = 0x9B1B30;        // rojo cereza
constexpr uint32_t PERGAMINO = 0x8A5A3C;  // café claro

double altoTexto(double puntos) { return puntos * 100.0 / 72.0 * 1.35; }

std::string precio(double p) {
    if (redondear(p, 2) == redondear(p, 4)) return formatoDecimal(p, 2);
    std::string s = formatoDecimal(p, 4);
    while (!s.empty() && s.back() == '0') s.pop_back();
    return s;
}


}  // namespace

// ================================================================ Lienzo
void Lienzo::texto(double x, double y, double ancho, const std::string& t, double puntos, bool negrita, uint32_t color,
                   int alineacion) {
    ElementoGrafico e;
    e.tipo = TipoElemento::Texto;
    e.x = x;
    e.y = y;
    e.ancho = ancho;
    e.alto = altoTexto(puntos);
    e.texto = t;
    e.puntos = puntos;
    e.negrita = negrita;
    e.color = color;
    e.alineacion = alineacion;
    elementos.push_back(e);
}

void Lienzo::parrafo(double x, double y, double ancho, double alto, const std::string& t, double puntos, uint32_t color) {
    texto(x, y, ancho, t, puntos, false, color);
    elementos.back().alto = alto;
    elementos.back().variasLineas = true;
}

void Lienzo::linea(double x1, double y1, double x2, double y2, uint32_t color, double grosor) {
    ElementoGrafico e;
    e.tipo = TipoElemento::Linea;
    e.x = x1;
    e.y = y1;
    e.ancho = x2 - x1;
    e.alto = y2 - y1;
    e.color = color;
    e.grosor = grosor;
    elementos.push_back(e);
}

void Lienzo::rectangulo(double x, double y, double ancho, double alto, uint32_t relleno, uint32_t borde, double grosor) {
    ElementoGrafico e;
    e.tipo = TipoElemento::Rectangulo;
    e.x = x;
    e.y = y;
    e.ancho = ancho;
    e.alto = alto;
    e.relleno = relleno;
    e.color = borde;
    e.grosor = grosor;
    elementos.push_back(e);
}

void Lienzo::imagen(double x, double y, double ancho, double alto, const std::string& nombre) {
    ElementoGrafico e;
    e.tipo = TipoElemento::Imagen;
    e.x = x;
    e.y = y;
    e.ancho = ancho;
    e.alto = alto;
    e.imagen = nombre;
    elementos.push_back(e);
}

// ================================================================ Comprobante de compra
std::vector<DisenoDocumento> disenosComprobante() {
    return {{1, "Noviembre 2026 (actual)"}};
}

Lienzo lienzoComprobante(const Empresa& e, const Factura& f, const Cliente& c, int marcaAgua, int diseno) {
    (void)diseno;  // por ahora hay un solo diseño; los siguientes se agregan sin borrar este
    Lienzo l;
    const double izq = 50, der = 800;

    // Marca de agua primero, para que todo lo demás quede encima.
    if (marcaAgua == MARCA_DIAGONAL) l.imagen(115, 330, 620, 620, "marca1");
    if (marcaAgua == MARCA_SELLO) l.imagen(175, 360, 500, 500, "marca2");

    // Encabezado: logo grande al centro y datos de la empresa.
    l.imagen(245, 28, 360, 138, "logo");
    double y = 172;
    if (normalizarBusqueda(e.nombre).find("unimax") == std::string::npos) {
        l.texto(izq, y, der - izq, e.nombre, 14, true, CAFE, ALINEAR_CENTRO);
        y += 26;
    }
    if (!e.lineaDatos().empty()) l.texto(izq, y, der - izq, e.lineaDatos(), 9, false, GRIS, ALINEAR_CENTRO);

    // Título, número y fecha.
    y = 222;
    l.linea(izq, y - 8, der, y - 8, CAFE, 3);
    l.texto(izq, y, 470, "COMPROBANTE DE COMPRA DE CAFÉ", 15, true, NEGRO);
    // Tipo de café bien visible: franja de color con "CAFÉ UVA" o "CAFÉ PERGAMINO".
    const bool uva = f.categoria == CATEGORIA_UVA;
    l.rectangulo(izq, y + 28, 330, 36, uva ? UVA : PERGAMINO);
    l.texto(izq, y + 33, 330, uva ? "CAFÉ UVA" : "CAFÉ PERGAMINO", 18, true, BLANCO, ALINEAR_CENTRO);
    l.texto(540, y, 100, "No.", 11, true, CAFE, ALINEAR_DERECHA);
    l.texto(645, y - 4, der - 645, f.numeroTexto(), 18, true, ROJO, ALINEAR_DERECHA);
    l.texto(540, y + 30, 100, "Fecha:", 11, true, CAFE, ALINEAR_DERECHA);
    l.texto(645, y + 30, der - 645, f.fecha.texto(), 11, false, NEGRO, ALINEAR_DERECHA);
    if (f.anulada) l.texto(390, y + 30, 150, "ANULADO", 20, true, ROJO, ALINEAR_CENTRO);

    // Cliente.
    y = 298;
    l.texto(izq, y, 150, "Productor / cliente:", 10.5, true, CAFE);
    l.texto(205, y - 2, der - 205, c.nombre, 12);
    l.linea(205, y + 22, der, y + 22, GRIS_CLARO);
    y += 34;
    l.texto(izq, y, 150, "DNI / RTN:", 10.5, true, CAFE);
    l.texto(205, y - 2, 220, c.dni, 12);
    l.linea(205, y + 22, 430, y + 22, GRIS_CLARO);
    l.texto(445, y, 80, "Teléfono:", 10.5, true, CAFE, ALINEAR_DERECHA);
    l.texto(535, y - 2, der - 535, c.telefono, 12);
    l.linea(535, y + 22, der, y + 22, GRIS_CLARO);
    y += 34;
    l.texto(izq, y, 150, "RTN:", 10.5, true, CAFE);
    l.texto(205, y - 2, 220, c.rtn, 12);
    l.linea(205, y + 22, 430, y + 22, GRIS_CLARO);
    l.texto(435, y, 90, "Carnet IHCAFE:", 10.5, true, CAFE, ALINEAR_DERECHA);
    l.texto(535, y - 2, der - 535, c.carnet, 12);
    l.linea(535, y + 22, der, y + 22, GRIS_CLARO);

    // Tabla de café pesado.
    const double col[] = {izq, 240, 340, 430, 520, 650, der};
    const char* const titulos[] = {"Tipo de café", "Libras", "Quintales", "Galones", "Precio por libra", "Subtotal"};
    y = 418;
    const double altoEnc = 30, altoFila = 26;
    l.rectangulo(izq, y, der - izq, altoEnc, CAFE);
    for (int i = 0; i < 6; ++i) l.texto(col[i], y + 6, col[i + 1] - col[i], titulos[i], 9.5, true, BLANCO, ALINEAR_CENTRO);
    double fila = y + altoEnc;
    const int filas = std::max<int>(6, static_cast<int>(f.lineas.size()));
    double libras = 0, galones = 0;
    for (int i = 0; i < filas; ++i) {
        const double yf = fila + i * altoFila;
        if (i < static_cast<int>(f.lineas.size())) {
            const LineaFactura& ln = f.lineas[static_cast<size_t>(i)];
            libras += ln.libras;
            galones += ln.galones;
            l.texto(col[0] + 8, yf + 5, col[1] - col[0] - 10, nombreTipoCafe(ln.tipoCafe), 10);
            l.texto(col[1], yf + 5, col[2] - col[1] - 8, formatoDecimal(ln.libras, 2), 10, false, NEGRO, ALINEAR_DERECHA);
            l.texto(col[2], yf + 5, col[3] - col[2] - 8, formatoDecimal(ln.quintales(), 2), 10, false, NEGRO,
                    ALINEAR_DERECHA);
            if (ln.galones > 0)
                l.texto(col[3], yf + 5, col[4] - col[3] - 8, formatoDecimal(ln.galones, 2), 10, false, NEGRO,
                        ALINEAR_DERECHA);
            l.texto(col[4], yf + 5, col[5] - col[4] - 8, "L " + precio(ln.precioLibra), 10, false, NEGRO,
                    ALINEAR_DERECHA);
            l.texto(col[5], yf + 5, col[6] - col[5] - 8, formatoLempiras(ln.subtotal), 10, false, NEGRO,
                    ALINEAR_DERECHA);
        }
        l.linea(izq, yf + altoFila, der, yf + altoFila, GRIS_CLARO);
    }
    const double finFilas = fila + filas * altoFila;
    // Fila de totales.
    l.rectangulo(izq, finFilas, der - izq, 30, CREMA);
    l.texto(col[0], finFilas + 7, col[1] - col[0] - 8, "TOTALES", 10, true, NEGRO, ALINEAR_DERECHA);
    l.texto(col[1], finFilas + 7, col[2] - col[1] - 8, formatoDecimal(libras, 2), 10, true, NEGRO, ALINEAR_DERECHA);
    l.texto(col[2], finFilas + 7, col[3] - col[2] - 8, formatoDecimal(aQuintales(libras), 2), 10, true, NEGRO,
            ALINEAR_DERECHA);
    if (galones > 0)
        l.texto(col[3], finFilas + 7, col[4] - col[3] - 8, formatoDecimal(galones, 2), 10, true, NEGRO, ALINEAR_DERECHA);
    l.texto(col[4], finFilas + 7, col[5] - col[4] - 8, "TOTAL:", 10.5, true, CAFE, ALINEAR_DERECHA);
    l.texto(col[5], finFilas + 5, col[6] - col[5] - 8, formatoLempiras(f.total), 12, true, NEGRO, ALINEAR_DERECHA);
    const double finTabla = finFilas + 30;
    // Bordes de la tabla.
    l.rectangulo(izq, y, der - izq, finTabla - y, SIN_COLOR, GRIS, 1.2);
    for (int i = 1; i < 6; ++i) l.linea(col[i], y, col[i], finTabla, GRIS_CLARO);

    // Totales en letras y pagos.
    y = finTabla + 24;
    l.texto(izq, y, 300, "Total en letras:", 10, true, CAFE);
    l.parrafo(izq, y + 22, 400, 60, montoEnLetras(f.total), 10.5);
    const double et = 470, ancho = 175;
    l.texto(et, y, ancho, "Pagado en efectivo:", 10, true, CAFE, ALINEAR_DERECHA);
    l.texto(et + ancho, y, der - et - ancho, formatoLempiras(f.pagado), 11, false, NEGRO, ALINEAR_DERECHA);
    l.texto(et, y + 26, ancho, "Saldo pendiente:", 10, true, CAFE, ALINEAR_DERECHA);
    l.texto(et + ancho, y + 25, der - et - ancho, formatoLempiras(f.saldo), 11.5, true, f.saldo > 0 ? ROJO : NEGRO,
            ALINEAR_DERECHA);
    l.texto(et, y + 52, ancho, "Fecha de pago del saldo:", 10, true, CAFE, ALINEAR_DERECHA);
    l.texto(et + ancho, y + 52, der - et - ancho,
            f.saldo > 0 ? (f.vencimientoSaldo ? f.vencimientoSaldo->texto() : std::string("Sin fecha")) : "—", 11, false,
            NEGRO, ALINEAR_DERECHA);

    // Observaciones.
    y += 96;
    l.texto(izq, y, 120, "Observaciones:", 10, true, CAFE);
    l.parrafo(175, y, der - 175, 40, f.observaciones, 10.5);
    l.linea(175, y + 22, der, y + 22, GRIS_CLARO);

    // Firmas.
    y = 930;
    l.linea(70, y, 330, y, NEGRO);
    l.linea(520, y, 780, y, NEGRO);
    l.texto(70, y + 6, 260, "Firma del productor", 10, false, NEGRO, ALINEAR_CENTRO);
    l.texto(520, y + 6, 260, "Entregado por (beneficio)", 10, false, NEGRO, ALINEAR_CENTRO);

    return l;
}

// ================================================================ Informe del día
Lienzo lienzoInformeDia(const Empresa& e, const Fecha& dia, const ResumenDia& hoy, const ResumenDia& mes,
                        const ResumenDia& cosecha, const Fecha& desdeCosecha) {
    Lienzo l;
    const double izq = 50, der = 800;
    l.imagen(275, 28, 300, 115, "logo");
    if (!e.lineaDatos().empty()) l.texto(izq, 148, der - izq, e.lineaDatos(), 9, false, GRIS, ALINEAR_CENTRO);
    l.linea(izq, 178, der, 178, CAFE, 3);
    l.texto(izq, 190, der - izq, "INFORME DEL DÍA", 20, true, NEGRO, ALINEAR_CENTRO);
    l.texto(izq, 226, der - izq, dia.texto(), 13, false, CAFE, ALINEAR_CENTRO);

    // Dos recuadros grandes: lo comprado hoy de uva y de pergamino.
    auto recuadro = [&](double x, const char* titulo, uint32_t color, double libras, int comprobantes) {
        l.rectangulo(x, 268, 360, 150, color);
        l.texto(x, 280, 360, titulo, 15, true, BLANCO, ALINEAR_CENTRO);
        l.texto(x, 312, 360, formatoDecimal(aQuintales(libras), 2) + " qq", 30, true, BLANCO, ALINEAR_CENTRO);
        l.texto(x, 364, 360, formatoDecimal(libras, 2) + " libras", 13, false, BLANCO, ALINEAR_CENTRO);
        l.texto(x, 388, 360, std::to_string(comprobantes) + (comprobantes == 1 ? " factura" : " facturas"), 11, false,
                BLANCO, ALINEAR_CENTRO);
    };
    recuadro(izq, "UVA HOY", UVA, hoy.libras[CAFE_UVA], hoy.comprobantesUva);
    recuadro(440, "PERGAMINO HOY", PERGAMINO, hoy.librasPergamino(), hoy.comprobantesPergamino);

    // Tabla: hoy, mes y cosecha, cada tipo por separado.
    double y = 450;
    const double col[] = {izq, 240, 335, 425, 520, 610, 705, der};
    l.texto(izq, y, der - izq, "Libras y quintales por tipo de café", 12, true, CAFE);
    y += 28;
    l.rectangulo(izq, y, der - izq, 26, CAFE);
    const std::string periodos[] = {"Hoy", "Este mes", "Cosecha (desde " + desdeCosecha.texto() + ")"};
    for (int p = 0; p < 3; ++p)
        l.texto(col[1 + 2 * p], y + 5, col[3 + 2 * p] - col[1 + 2 * p], periodos[p], 9.5, true, BLANCO, ALINEAR_CENTRO);
    y += 26;
    l.rectangulo(izq, y, der - izq, 22, CREMA);
    l.texto(col[0] + 8, y + 4, col[1] - col[0], "Tipo de café", 9.5, true, NEGRO);
    for (int p = 0; p < 3; ++p) {
        l.texto(col[1 + 2 * p], y + 4, col[2 + 2 * p] - col[1 + 2 * p] - 6, "Libras", 9.5, true, NEGRO, ALINEAR_DERECHA);
        l.texto(col[2 + 2 * p], y + 4, col[3 + 2 * p] - col[2 + 2 * p] - 6, "Quintales", 9.5, true, NEGRO, ALINEAR_DERECHA);
    }
    y += 22;
    const ResumenDia* datos[] = {&hoy, &mes, &cosecha};
    struct Fila {
        const char* nombre;
        int tipo;  // 0 = total pergamino
        bool negrita;
    };
    const Fila filas[] = {{"Uva", CAFE_UVA, true}, {"Pergamino húmedo", CAFE_HUMEDO, false},
                          {"Pergamino seco", CAFE_SECO, false}, {"Total pergamino", 0, true}};
    const double inicioTabla = y - 48;
    for (const auto& f : filas) {
        l.texto(col[0] + 8, y + 6, col[1] - col[0], f.nombre, 10.5, f.negrita, f.tipo == CAFE_UVA ? UVA : NEGRO);
        for (int p = 0; p < 3; ++p) {
            const double lb = f.tipo ? datos[p]->libras[f.tipo] : datos[p]->librasPergamino();
            l.texto(col[1 + 2 * p], y + 6, col[2 + 2 * p] - col[1 + 2 * p] - 6, formatoDecimal(lb, 2), 10.5, f.negrita,
                    NEGRO, ALINEAR_DERECHA);
            l.texto(col[2 + 2 * p], y + 6, col[3 + 2 * p] - col[2 + 2 * p] - 6, formatoDecimal(aQuintales(lb), 2), 10.5,
                    f.negrita, NEGRO, ALINEAR_DERECHA);
        }
        y += 30;
        l.linea(izq, y, der, y, GRIS_CLARO);
    }
    l.rectangulo(izq, inicioTabla, der - izq, y - inicioTabla, SIN_COLOR, GRIS, 1.2);
    for (int i = 1; i < 7; i += 2) l.linea(col[i], inicioTabla, col[i], y, GRIS);
    y += 20;
    l.texto(izq, y, der - izq,
            "Facturas del mes: " + std::to_string(mes.comprobantesUva) + " de uva y " +
                std::to_string(mes.comprobantesPergamino) + " de pergamino.   Cosecha: " +
                std::to_string(cosecha.comprobantesUva) + " de uva y " + std::to_string(cosecha.comprobantesPergamino) +
                " de pergamino.",
            10, false, GRIS);
    l.texto(izq, y + 22, der - izq, "No incluye facturas anuladas. 1 quintal = 100 libras.", 9, false, GRIS);
    l.linea(70, 960, 330, 960, NEGRO);
    l.texto(70, 966, 260, "Revisado por", 10, false, NEGRO, ALINEAR_CENTRO);
    return l;
}

Lienzo lienzoInventario(const Empresa& e, const Fecha& hasta, const Inventario& inv) {
    Lienzo l;
    const double izq = 50, der = 800;
    l.imagen(275, 28, 300, 115, "logo");
    if (!e.lineaDatos().empty()) l.texto(izq, 148, der - izq, e.lineaDatos(), 9, false, GRIS, ALINEAR_CENTRO);
    l.linea(izq, 178, der, 178, CAFE, 3);
    l.texto(izq, 190, der - izq, "CAFÉ EN BODEGA", 20, true, NEGRO, ALINEAR_CENTRO);
    l.texto(izq, 226, der - izq, "Al " + hasta.texto(), 13, false, CAFE, ALINEAR_CENTRO);

    // Recuadros: uva y pergamino en bodega.
    auto recuadro = [&](double x, const char* titulo, uint32_t color, double libras) {
        l.rectangulo(x, 268, 360, 130, color);
        l.texto(x, 280, 360, titulo, 15, true, BLANCO, ALINEAR_CENTRO);
        l.texto(x, 310, 360, formatoDecimal(aQuintales(libras), 2) + " qq", 30, true, BLANCO, ALINEAR_CENTRO);
        l.texto(x, 362, 360, formatoDecimal(libras, 2) + " libras", 13, false, BLANCO, ALINEAR_CENTRO);
    };
    recuadro(izq, "UVA EN BODEGA", UVA, inv.existencia(CAFE_UVA));
    recuadro(440, "PERGAMINO EN BODEGA", PERGAMINO, inv.existencia(CAFE_HUMEDO) + inv.existencia(CAFE_SECO));

    double y = 430;
    const double col[] = {izq, 230, 325, 420, 515, 610, 705, der};
    l.rectangulo(izq, y, der - izq, 26, CAFE);
    const char* grupos[] = {"Comprado", "Vendido", "En bodega"};
    for (int p = 0; p < 3; ++p) l.texto(col[1 + 2 * p], y + 5, col[3 + 2 * p] - col[1 + 2 * p], grupos[p], 9.5, true, BLANCO, ALINEAR_CENTRO);
    y += 26;
    l.rectangulo(izq, y, der - izq, 22, CREMA);
    l.texto(col[0] + 8, y + 4, col[1] - col[0], "Tipo de café", 9.5, true, NEGRO);
    for (int p = 0; p < 3; ++p) {
        l.texto(col[1 + 2 * p], y + 4, col[2 + 2 * p] - col[1 + 2 * p] - 6, "Libras", 9.5, true, NEGRO, ALINEAR_DERECHA);
        l.texto(col[2 + 2 * p], y + 4, col[3 + 2 * p] - col[2 + 2 * p] - 6, "Quintales", 9.5, true, NEGRO, ALINEAR_DERECHA);
    }
    y += 22;
    const double inicioTabla = y - 48;
    struct Fila {
        const char* nombre;
        int tipo;  // 0 = total
    };
    const Fila filas[] = {{"Uva", CAFE_UVA}, {"Pergamino húmedo", CAFE_HUMEDO}, {"Pergamino seco", CAFE_SECO}, {"Total", 0}};
    for (const auto& f : filas) {
        double v[3] = {0, 0, 0};
        for (int t = 1; t <= NUM_TIPOS_CAFE; ++t) {
            if (f.tipo && t != f.tipo) continue;
            v[0] += inv.comprado[t];
            v[1] += inv.vendido[t];
            v[2] += inv.existencia(t);
        }
        const bool negrita = f.tipo == 0;
        l.texto(col[0] + 8, y + 6, col[1] - col[0], f.nombre, 10.5, negrita, f.tipo == CAFE_UVA ? UVA : NEGRO);
        for (int p = 0; p < 3; ++p) {
            const uint32_t color = (p == 2 && v[2] < -0.005) ? 0xC00000 : NEGRO;
            l.texto(col[1 + 2 * p], y + 6, col[2 + 2 * p] - col[1 + 2 * p] - 6, formatoDecimal(v[p], 2), 10.5,
                    negrita || p == 2, color, ALINEAR_DERECHA);
            l.texto(col[2 + 2 * p], y + 6, col[3 + 2 * p] - col[2 + 2 * p] - 6, formatoDecimal(aQuintales(v[p]), 2), 10.5,
                    negrita || p == 2, color, ALINEAR_DERECHA);
        }
        y += 30;
        l.linea(izq, y, der, y, GRIS_CLARO);
    }
    l.rectangulo(izq, inicioTabla, der - izq, y - inicioTabla, SIN_COLOR, GRIS, 1.2);
    for (int i = 1; i < 7; i += 2) l.linea(col[i], inicioTabla, col[i], y, GRIS);
    y += 20;
    l.texto(izq, y, der - izq,
            "Según las compras y ventas registradas (sin anuladas). No incluye mermas, secado ni el paso de uva a pergamino.",
            9.5, false, GRIS);
    l.texto(izq, y + 20, der - izq, "Si en bodega sale negativo (en rojo), se vendió más de lo que se registró como compra.",
            9.5, false, GRIS);
    l.linea(70, 960, 330, 960, NEGRO);
    l.texto(70, 966, 260, "Revisado por", 10, false, NEGRO, ALINEAR_CENTRO);
    return l;
}

// ================================================================ JSON (para revisar)
namespace {
std::string json(const std::string& s) {
    std::string r = "\"";
    for (char ch : s) {
        switch (ch) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': break;
            case '\t': r += "\\t"; break;
            default: r += ch;
        }
    }
    return r + "\"";
}
std::string num(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    return buf;
}
}  // namespace

std::string lienzoJson(const Lienzo& l) {
    std::string r = "{\"ancho\":" + num(l.ancho) + ",\"alto\":" + num(l.alto) + ",\"elementos\":[\n";
    for (size_t i = 0; i < l.elementos.size(); ++i) {
        const auto& e = l.elementos[i];
        const char* tipo = e.tipo == TipoElemento::Texto ? "texto"
                           : e.tipo == TipoElemento::Linea ? "linea"
                           : e.tipo == TipoElemento::Rectangulo ? "rectangulo"
                                                                : "imagen";
        r += "{\"tipo\":\"" + std::string(tipo) + "\",\"x\":" + num(e.x) + ",\"y\":" + num(e.y) + ",\"ancho\":" +
             num(e.ancho) + ",\"alto\":" + num(e.alto) + ",\"texto\":" + json(e.texto) + ",\"puntos\":" + num(e.puntos) +
             ",\"negrita\":" + (e.negrita ? "true" : "false") + ",\"alineacion\":" + std::to_string(e.alineacion) +
             ",\"variasLineas\":" + (e.variasLineas ? "true" : "false") + ",\"color\":" + std::to_string(e.color) +
             ",\"relleno\":" + std::to_string(e.relleno) + ",\"grosor\":" + num(e.grosor) + ",\"imagen\":" +
             json(e.imagen) + "}";
        r += i + 1 < l.elementos.size() ? ",\n" : "\n";
    }
    return r + "]}\n";
}

}  // namespace bu
