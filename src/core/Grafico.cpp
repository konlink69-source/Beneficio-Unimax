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

double altoTexto(double puntos) { return puntos * 100.0 / 72.0 * 1.35; }

std::string precio(double p) {
    if (redondear(p, 2) == redondear(p, 4)) return formatoDecimal(p, 2);
    std::string s = formatoDecimal(p, 4);
    while (!s.empty() && s.back() == '0') s.pop_back();
    return s;
}

std::string numero6(long n) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%06ld", n);
    return buf;
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
Lienzo lienzoComprobante(const Empresa& e, const Factura& f, const Cliente& c, int marcaAgua) {
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
    l.texto(izq, y + 4, 470, "COMPROBANTE DE COMPRA DE CAFÉ", 16, true, NEGRO);
    l.texto(540, y, 100, "No.", 11, true, CAFE, ALINEAR_DERECHA);
    l.texto(645, y - 4, der - 645, numero6(f.numero), 18, true, ROJO, ALINEAR_DERECHA);
    l.texto(540, y + 30, 100, "Fecha:", 11, true, CAFE, ALINEAR_DERECHA);
    l.texto(645, y + 30, der - 645, f.fecha.texto(), 11, false, NEGRO, ALINEAR_DERECHA);
    if (f.anulada) l.texto(izq, y + 50, der - izq, "ANULADO", 24, true, ROJO, ALINEAR_CENTRO);

    // Cliente.
    y = 300;
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

    // Tabla de café pesado.
    const double col[] = {izq, 240, 340, 430, 520, 650, der};
    const char* const titulos[] = {"Tipo de café", "Libras", "Quintales", "Galones", "Precio por libra", "Subtotal"};
    y = 385;
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

    // Pie.
    l.texto(izq, 1005, der - izq,
            "Pago calculado por peso en libras. 1 quintal = 100 libras. Los galones son un dato de referencia.", 8, false,
            GRIS, ALINEAR_CENTRO);
    std::string pie = "Comprobante interno de compra";
    if (!f.usuario.empty()) pie += "  ·  Registrado por: " + f.usuario;
    if (!f.hora.empty()) pie += "  ·  Hora: " + f.hora.substr(0, 5);
    l.texto(izq, 1025, der - izq, pie, 8, false, GRIS, ALINEAR_CENTRO);
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
