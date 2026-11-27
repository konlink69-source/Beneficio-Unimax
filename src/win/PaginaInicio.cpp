// Pestaña "Inicio": panel con el resumen del día (compras, caja, deudas y gráficos).
#include <algorithm>
#include <cmath>

#include "core/Panel.h"
#include "win/App.h"
#include "win/Impresion.h"
#include "win/Paginas.h"
#include "win/Tema.h"
#include "win/resource.h"

namespace bu::win {

namespace {

constexpr COLORREF COLOR_UVA = tema::VINO;
constexpr COLORREF COLOR_HUMEDO = tema::CAFE_CLARO;
constexpr COLORREF COLOR_SECO = tema::CAFE;
constexpr COLORREF COLOR_PERGAMINO = tema::CAFE_CLARO;

enum Zona { Z_UVA = 1, Z_PERGAMINO, Z_CAJA, Z_DEUDAS, Z_GRAFICO, Z_TIPOS, Z_VENCIMIENTOS, Z_PRODUCTORES };

int px(int v) {
    static const int ppp = [] {
        HDC dc = GetDC(nullptr);
        const int d = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        return d;
    }();
    return MulDiv(v, ppp, 96);
}

class PaginaInicio : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Inicio"; }

    void alMostrar() override {
        const Fecha hoy = Fecha::hoy();
        datos_ = calcularPanel(repo().detalleCompras(inicioCosecha(hoy), hoy), hoy);
        caja_ = repo().resumenCaja(hoy);
        saldoCaja_ = caja_.cierre ? caja_.cierre->efectivoContado : caja_.esperado;
        minimo_ = App::instancia().minimoCaja();
        rec_ = repo().recordatorio(hoy);
        cargado_ = true;
        InvalidateRect(h_, nullptr, FALSE);
    }

    void alRedimensionar(int, int) override { InvalidateRect(h_, nullptr, FALSE); }

    bool alPintar(HDC dc, const RECT& area) override {
        zonas_.clear();
        if (!cargado_) return false;
        const int g = px(16);
        const int W = area.right, H = area.bottom;
        const int alto1 = std::max(px(150), std::min(px(190), H * 28 / 100));
        const int resto = H - alto1 - 2 * g;
        const int alto2 = resto * 58 / 100;
        const int alto3 = resto - alto2;
        const int ancho4 = (W - 3 * g) / 4;

        // ---- Fila 1: cuatro tarjetas
        int x = 0;
        tarjetaUva(dc, {x, 0, x + ancho4, alto1});
        x += ancho4 + g;
        tarjetaPergamino(dc, {x, 0, x + ancho4, alto1});
        x += ancho4 + g;
        tarjetaCaja(dc, {x, 0, x + ancho4, alto1});
        x += ancho4 + g;
        tarjetaDeudas(dc, {x, 0, W, alto1});

        // ---- Fila 2: gráfico de barras y anillo
        const int y2 = alto1 + g;
        const int anchoGrafico = (W - g) * 60 / 100;
        grafico(dc, {0, y2, anchoGrafico, y2 + alto2});
        tipos(dc, {anchoGrafico + g, y2, W, y2 + alto2});

        // ---- Fila 3: vencimientos y mejores productores
        const int y3 = y2 + alto2 + g;
        if (alto3 > px(90)) {
            vencimientos(dc, {0, y3, anchoGrafico, H});
            productores(dc, {anchoGrafico + g, y3, W, H});
        }
        // Logo del beneficio de fondo, tenue, encima de todo (como en las demás pestañas).
        if (App::instancia().marcaAgua() != 0) tema::logoMarca(dc, area);
        return true;
    }

    bool alRaton(UINT msg, int x, int y) override {
        const POINT p{x, y};
        int z = 0;
        for (const auto& zona : zonas_)
            if (PtInRect(&zona.first, p)) z = zona.second;
        if (msg == WM_SETCURSOR) {
            if (!z) return false;
            SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32649)));  // mano
            return true;
        }
        if (msg == WM_MOUSELEAVE) {
            encima_ = 0;
            InvalidateRect(h_, nullptr, FALSE);
            return true;
        }
        if (msg == WM_MOUSEMOVE) {
            if (z != encima_) {
                encima_ = z;
                InvalidateRect(h_, nullptr, FALSE);
                TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, h_, 0};
                TrackMouseEvent(&t);
            }
            return true;
        }
        if (msg == WM_LBUTTONUP && z) {
            switch (z) {
                case Z_UVA:
                case Z_PERGAMINO: verInformeDia(h_, Fecha::hoy()); break;
                case Z_CAJA: App::instancia().irAPagina(PAG_CAJA); break;
                case Z_DEUDAS:
                case Z_VENCIMIENTOS: App::instancia().irAPagina(PAG_DEUDAS); break;
                case Z_GRAFICO:
                case Z_TIPOS: App::instancia().irAPagina(PAG_REPORTES); break;
                case Z_PRODUCTORES: App::instancia().irAPagina(PAG_CLIENTES); break;
                default: break;
            }
            return true;
        }
        return false;
    }

private:
    // ---- Ayudantes de dibujo
    HFONT f(double factor, bool negrita) const { return fuente(tamanoLetra() * factor, negrita); }

    void marco(HDC dc, const RECT& r, int zona) {
        tema::tarjeta(dc, r);
        if (zona && zona == encima_) {
            RECT b = r;
            tema::rectRedondeado(dc, b, px(10), tema::TARJETA, tema::CAFE_CLARO, 2);
        }
        zonas_.push_back({r, zona});
    }

    int tituloTarjeta(HDC dc, const RECT& r, const std::string& t, const std::string& derecha = "") {
        const int pad = px(18);
        RECT rt{r.left + pad, r.top + px(14), r.right - pad, r.top + px(14) + tema::altoTexto(dc, f(1.1, true))};
        tema::texto(dc, t, rt, f(1.1, true), tema::TEXTO, DT_LEFT | DT_VCENTER);
        if (!derecha.empty()) tema::texto(dc, derecha, rt, f(0.95, false), tema::TEXTO_SUAVE, DT_RIGHT | DT_VCENTER);
        return rt.bottom;
    }

    struct Columna {
        std::string etiqueta, valor;
        COLORREF color = tema::TEXTO;
    };

    // Tarjeta de indicador: título, número grande, detalle y columnas abajo (como "IVA / IRPF / Total").
    void indicador(HDC dc, const RECT& r, int zona, const std::string& titulo, const std::string& etiquetaGrande,
                   const std::string& grande, COLORREF colorGrande, const std::string& detalle,
                   const std::vector<Columna>& columnas) {
        marco(dc, r, zona);
        const int pad = px(18);
        int y = tituloTarjeta(dc, r, titulo) + px(4);
        RECT re{r.left + pad, y, r.right - pad, y + tema::altoTexto(dc, f(0.95, false))};
        tema::texto(dc, etiquetaGrande, re, f(0.95, false), tema::TEXTO_SUAVE);
        y = re.bottom;
        // Número grande: se achica si no cabe.
        double factor = 2.5;
        while (factor > 1.4 && tema::anchoTexto(dc, f(factor, true), grande) > r.right - r.left - 2 * pad) factor -= 0.1;
        RECT rg{r.left + pad, y, r.right - pad, y + tema::altoTexto(dc, f(factor, true))};
        tema::texto(dc, grande, rg, f(factor, true), colorGrande);
        y = rg.bottom;
        if (!detalle.empty()) {
            RECT rd{r.left + pad, y, r.right - pad, y + tema::altoTexto(dc, f(0.95, false))};
            tema::texto(dc, detalle, rd, f(0.95, false), tema::TEXTO_SUAVE);
        }
        if (columnas.empty()) return;
        const int altoEt = tema::altoTexto(dc, f(0.9, false)), altoVal = tema::altoTexto(dc, f(1.0, false));
        const int yc = r.bottom - px(14) - altoEt - altoVal;
        if (yc < y + px(4)) return;
        const int anchoCol = (r.right - r.left - 2 * pad) / static_cast<int>(columnas.size());
        for (size_t i = 0; i < columnas.size(); ++i) {
            const int xc = r.left + pad + static_cast<int>(i) * anchoCol;
            RECT r1{xc, yc, xc + anchoCol - px(6), yc + altoEt};
            tema::texto(dc, columnas[i].etiqueta, r1, f(0.9, false), tema::TEXTO_SUAVE);
            RECT r2{xc, yc + altoEt, xc + anchoCol - px(6), yc + altoEt + altoVal};
            tema::texto(dc, columnas[i].valor, r2, f(1.0, false), columnas[i].color);
        }
    }

    void tarjetaUva(HDC dc, const RECT& r) {
            const ResumenDia& h = datos_.deHoy;
            indicador(dc, r, Z_UVA, "Café uva", "Comprado hoy", formatoLibras(h.libras[CAFE_UVA]), COLOR_UVA,
                      formatoDecimal(aQuintales(h.libras[CAFE_UVA]), 2) + " qq  ·  " + formatoLempiras(h.monto[CAFE_UVA]),
                      {{"Este mes", formatoLibras(datos_.delMes.libras[CAFE_UVA])},
                       {"Cosecha", formatoLibras(datos_.deCosecha.libras[CAFE_UVA])},
                       {"Comprobantes", std::to_string(h.comprobantesUva)}});
    }

    void tarjetaPergamino(HDC dc, const RECT& r) {
            const ResumenDia& h = datos_.deHoy;
            indicador(dc, r, Z_PERGAMINO, "Café pergamino", "Comprado hoy", formatoLibras(h.librasPergamino()),
                      COLOR_PERGAMINO,
                      "Húmedo " + formatoLibras(h.libras[CAFE_HUMEDO]) + "  ·  Seco " + formatoLibras(h.libras[CAFE_SECO]),
                      {{"Este mes", formatoLibras(datos_.delMes.librasPergamino())},
                       {"Cosecha", formatoLibras(datos_.deCosecha.librasPergamino())},
                       {"Comprobantes", std::to_string(h.comprobantesPergamino)}});
    }

    void tarjetaCaja(HDC dc, const RECT& r) {
            const bool baja = saldoCaja_ < minimo_;
            indicador(dc, r, Z_CAJA, "Caja", caja_.cierre ? "Cerrada con" : "Hay en caja", formatoLempiras(saldoCaja_),
                      baja ? tema::ROJO : tema::VERDE,
                      baja ? "¡Caja baja! El mínimo es " + formatoLempiras(minimo_) : "Mínimo " + formatoLempiras(minimo_),
                      {{"Inicial", formatoLempiras(caja_.saldoInicial)},
                       {"Entradas", formatoLempiras(caja_.ingresos), tema::VERDE},
                       {"Salidas", formatoLempiras(caja_.egresos), tema::ROJO}});
    }

    void tarjetaDeudas(HDC dc, const RECT& r) {
            const int urgentes = static_cast<int>(rec_.vencidas.size() + rec_.venceHoy.size());
            indicador(dc, r, Z_DEUDAS, "Deudas", "Nos deben", formatoLempiras(rec_.totales.porCobrar), tema::VERDE,
                      std::to_string(rec_.totales.cantPorCobrar) + " pendientes",
                      {{"Debemos", formatoLempiras(rec_.totales.porPagar), tema::ROJO},
                       {"Vencidas / hoy", std::to_string(urgentes), urgentes ? tema::ROJO : tema::TEXTO},
                       {"Próximos días", std::to_string(rec_.proximas.size())}});
    }

    void leyenda(HDC dc, int derecha, int cy, const std::vector<std::pair<std::string, COLORREF>>& items) {
        int x = derecha;
        for (auto it = items.rbegin(); it != items.rend(); ++it) {
            const int ancho = tema::anchoTexto(dc, f(0.95, false), it->first);
            RECT rt{x - ancho, cy - px(10), x, cy + px(10)};
            tema::texto(dc, it->first, rt, f(0.95, false), tema::TEXTO, DT_LEFT | DT_VCENTER);
            tema::circulo(dc, x - ancho - px(10), cy, px(5), it->second);
            x -= ancho + px(28);
        }
    }

    void grafico(HDC dc, const RECT& r) {
        marco(dc, r, Z_GRAFICO);
        const int pad = px(18);
        const int yt = tituloTarjeta(dc, r, "Compras de la cosecha por mes");
        leyenda(dc, r.right - pad, (r.top + px(14) + yt) / 2, {{"Uva", COLOR_UVA}, {"Pergamino", COLOR_PERGAMINO}});
        Centavos maximo = 0;
        for (int i = 0; i < 12; ++i)
            maximo = std::max({maximo, datos_.montoUva[static_cast<size_t>(i)], datos_.montoPergamino[static_cast<size_t>(i)]});
        const int altoEtiqueta = tema::altoTexto(dc, f(0.85, false));
        RECT area{r.left + pad + px(64), yt + px(18), r.right - pad, r.bottom - px(12) - altoEtiqueta};
        if (area.bottom - area.top < px(40)) return;
        if (maximo == 0) {
            tema::texto(dc, "Todavía no hay compras en esta cosecha.", area, f(1.0, false), tema::TEXTO_SUAVE,
                        DT_CENTER | DT_VCENTER);
            return;
        }
        // Escala "redonda" para las líneas guía.
        const double maxL = static_cast<double>(maximo) / 100.0;
        const double paso10 = std::pow(10.0, std::floor(std::log10(maxL / 4)));
        double paso = paso10;
        for (const double m : {1.0, 2.0, 2.5, 5.0, 10.0})
            if (maxL / (paso10 * m) <= 4.0) {
                paso = paso10 * m;
                break;
            }
        const int lineas = static_cast<int>(std::ceil(maxL / paso));
        const double tope = paso * lineas;
        const int altoArea = area.bottom - area.top;
        for (int i = 0; i <= lineas; ++i) {
            const int y = area.bottom - static_cast<int>(altoArea * i / static_cast<double>(lineas));
            tema::linea(dc, area.left, y, area.right, y, i == 0 ? tema::BORDE : RGB(240, 234, 225));
            RECT re{r.left + pad, y - altoEtiqueta / 2, area.left - px(8), y + altoEtiqueta / 2};
            tema::texto(dc, montoCorto(static_cast<Centavos>(paso * i * 100)), re, f(0.85, false), tema::TEXTO_SUAVE,
                        DT_RIGHT | DT_VCENTER);
        }
        const double anchoMes = (area.right - area.left) / 12.0;
        const int anchoBarra = std::max(px(4), static_cast<int>(anchoMes * 0.28));
        for (int i = 0; i < 12; ++i) {
            const int centro = area.left + static_cast<int>(anchoMes * i + anchoMes / 2);
            const Centavos valores[2] = {datos_.montoUva[static_cast<size_t>(i)], datos_.montoPergamino[static_cast<size_t>(i)]};
            const COLORREF colores[2] = {COLOR_UVA, COLOR_PERGAMINO};
            for (int k = 0; k < 2; ++k) {
                if (valores[k] <= 0) continue;
                const int alto = std::max(px(2), static_cast<int>(altoArea * (valores[k] / 100.0) / tope));
                const int x0 = k == 0 ? centro - anchoBarra - px(1) : centro + px(1);
                RECT rb{x0, area.bottom - alto, x0 + anchoBarra, area.bottom};
                tema::rectRedondeado(dc, rb, px(3), colores[k], colores[k]);
            }
            RECT rm{centro - static_cast<int>(anchoMes / 2), area.bottom + px(6), centro + static_cast<int>(anchoMes / 2),
                    area.bottom + px(6) + altoEtiqueta};
            const bool actual = i == datos_.mesActual;
            tema::texto(dc, mesCosecha(i), rm, f(0.85, actual), actual ? tema::CAFE : tema::TEXTO_SUAVE, DT_CENTER);
        }
    }

    void tipos(HDC dc, const RECT& r) {
        marco(dc, r, Z_TIPOS);
        const int pad = px(18);
        const int yt = tituloTarjeta(dc, r, "Compras por tipo de café", "Cosecha");
        const ResumenDia& c = datos_.deCosecha;
        const Centavos total = c.monto[CAFE_UVA] + c.monto[CAFE_HUMEDO] + c.monto[CAFE_SECO];
        const int disponible = std::min(r.bottom - yt - px(24), (r.right - r.left) * 42 / 100);
        if (disponible < px(60)) return;
        const int radio = disponible / 2;
        const int cx = r.left + pad + radio, cy = (yt + px(8) + r.bottom - px(12)) / 2;
        const int grosor = std::max(px(12), radio / 4);
        struct Parte {
            const char* nombre;
            Centavos monto;
            double libras;
            COLORREF color;
        };
        const Parte partes[] = {{"Uva", c.monto[CAFE_UVA], c.libras[CAFE_UVA], COLOR_UVA},
                                {"Pergamino húmedo", c.monto[CAFE_HUMEDO], c.libras[CAFE_HUMEDO], COLOR_HUMEDO},
                                {"Pergamino seco", c.monto[CAFE_SECO], c.libras[CAFE_SECO], COLOR_SECO}};
        tema::arco(dc, cx, cy, radio, grosor, 0, 360, RGB(240, 234, 225));
        if (total > 0) {
            double angulo = 0;
            for (const auto& p : partes) {
                const double barrido = 360.0 * static_cast<double>(p.monto) / static_cast<double>(total);
                tema::arco(dc, cx, cy, radio, grosor, angulo, std::max(0.0, barrido - (barrido > 2 ? 1.2 : 0)), p.color);
                angulo += barrido;
            }
        }
        const int altoT = tema::altoTexto(dc, f(1.0, true));
        RECT r1{cx - radio + grosor, cy - altoT, cx + radio - grosor, cy};
        tema::texto(dc, "Total", r1, f(0.95, false), tema::TEXTO_SUAVE, DT_CENTER | DT_BOTTOM);
        RECT r2{cx - radio + grosor, cy, cx + radio - grosor, cy + altoT};
        tema::texto(dc, montoCorto(total), r2, f(1.05, true), tema::TEXTO, DT_CENTER | DT_TOP);
        // Leyenda con montos y porcentajes.
        const int xl = cx + radio + px(24);
        const int altoFila = tema::altoTexto(dc, f(1.0, false)) * 2 + px(8);
        int y = cy - altoFila * 3 / 2;
        for (const auto& p : partes) {
            tema::circulo(dc, xl + px(5), y + tema::altoTexto(dc, f(1.0, false)) / 2, px(5), p.color);
            RECT rn{xl + px(18), y, r.right - pad, y + tema::altoTexto(dc, f(1.0, false))};
            tema::texto(dc, p.nombre, rn, f(1.0, false), tema::TEXTO);
            const std::string pct =
                total > 0 ? formatoDecimal(100.0 * static_cast<double>(p.monto) / static_cast<double>(total), 1) + " %" : "0 %";
            tema::texto(dc, pct, rn, f(1.0, true), tema::TEXTO, DT_RIGHT);
            RECT rv{rn.left, rn.bottom, rn.right, rn.bottom + tema::altoTexto(dc, f(0.9, false))};
            tema::texto(dc, formatoLempiras(p.monto) + "  ·  " + formatoDecimal(aQuintales(p.libras), 1) + " qq", rv,
                        f(0.9, false), tema::TEXTO_SUAVE);
            y += altoFila;
        }
    }

    void vencimientos(HDC dc, const RECT& r) {
        marco(dc, r, Z_VENCIMIENTOS);
        const int pad = px(18);
        int y = tituloTarjeta(dc, r, "Deudas que vencen pronto", "Próximos " + std::to_string(rec_.diasAnticipacion) + " días") +
                px(10);
        std::vector<std::pair<const Deuda*, int>> lista;  // -1 vencida, 0 hoy, 1 próxima
        for (const auto& d : rec_.vencidas) lista.push_back({&d, -1});
        for (const auto& d : rec_.venceHoy) lista.push_back({&d, 0});
        for (const auto& d : rec_.proximas) lista.push_back({&d, 1});
        const int altoFila = tema::altoTexto(dc, f(1.0, false)) + px(10);
        if (lista.empty()) {
            RECT rv{r.left + pad, y, r.right - pad, r.bottom - px(12)};
            tema::texto(dc, "No hay deudas vencidas ni por vencer. ¡Todo al día!", rv, f(1.0, false), tema::VERDE,
                        DT_CENTER | DT_VCENTER);
            return;
        }
        const int ancho = r.right - r.left - 2 * pad;
        for (const auto& item : lista) {
            if (y + altoFila > r.bottom - px(10)) break;
            const Deuda& d = *item.first;
            RECT rf{r.left + pad, y, r.right - pad, y + altoFila};
            if (item.second < 0) tema::rectRedondeado(dc, rf, px(6), RGB(253, 242, 240), RGB(253, 242, 240));
            RECT rn{rf.left + px(8), rf.top, rf.left + ancho * 45 / 100, rf.bottom};
            tema::texto(dc, d.clienteNombre, rn, f(1.0, false), tema::TEXTO, DT_LEFT | DT_VCENTER);
            RECT rtp{rn.right, rf.top, rf.left + ancho * 63 / 100, rf.bottom};
            const bool cobrar = d.tipo == TipoDeuda::PorCobrar;
            tema::texto(dc, cobrar ? "Nos debe" : "Debemos", rtp, f(0.9, false), cobrar ? tema::VERDE : tema::ROJO,
                        DT_LEFT | DT_VCENTER);
            RECT rm{rtp.right, rf.top, rf.left + ancho * 84 / 100, rf.bottom};
            tema::texto(dc, formatoLempiras(d.saldo), rm, f(1.0, true), tema::TEXTO, DT_RIGHT | DT_VCENTER);
            RECT rv{rm.right, rf.top, rf.right - px(8), rf.bottom};
            const std::string cuando = item.second < 0 ? "Vencida" : (item.second == 0 ? "Hoy" : d.vencimiento ? d.vencimiento->texto().substr(0, 5) : "");
            tema::texto(dc, cuando, rv, f(0.9, item.second <= 0), item.second <= 0 ? tema::ROJO : tema::TEXTO_SUAVE,
                        DT_RIGHT | DT_VCENTER);
            y += altoFila + px(2);
        }
    }

    void productores(HDC dc, const RECT& r) {
        marco(dc, r, Z_PRODUCTORES);
        const int pad = px(18);
        int y = tituloTarjeta(dc, r, "Mejores productores del mes") + px(10);
        const auto& lista = datos_.mejoresDelMes;
        if (lista.empty()) {
            RECT rv{r.left + pad, y, r.right - pad, r.bottom - px(12)};
            tema::texto(dc, "Todavía no hay compras este mes.", rv, f(1.0, false), tema::TEXTO_SUAVE, DT_CENTER | DT_VCENTER);
            return;
        }
        const Centavos maximo = std::max<Centavos>(1, lista.front().monto);
        const int altoNombre = tema::altoTexto(dc, f(1.0, false));
        const int altoFila = altoNombre + px(14);
        int puesto = 1;
        for (const auto& p : lista) {
            if (y + altoFila > r.bottom - px(10)) break;
            RECT rn{r.left + pad, y, r.right - pad, y + altoNombre};
            tema::texto(dc, std::to_string(puesto++) + ".  " + p.nombre, rn, f(1.0, false), tema::TEXTO);
            tema::texto(dc, formatoLempiras(p.monto) + "  ·  " + formatoLibras(p.libras), rn, f(0.9, false),
                        tema::TEXTO_SUAVE, DT_RIGHT);
            const int anchoTotal = r.right - r.left - 2 * pad;
            RECT fondo{rn.left, rn.bottom + px(4), rn.left + anchoTotal, rn.bottom + px(9)};
            tema::rectRedondeado(dc, fondo, px(3), RGB(242, 236, 227), RGB(242, 236, 227));
            RECT barra = fondo;
            barra.right = barra.left + std::max(px(6), static_cast<int>(anchoTotal * static_cast<double>(p.monto) / maximo));
            tema::rectRedondeado(dc, barra, px(3), tema::CAFE_CLARO, tema::CAFE_CLARO);
            y += altoFila + px(4);
        }
    }

    DatosPanel datos_;
    ResumenCaja caja_;
    Centavos saldoCaja_ = 0;
    Centavos minimo_ = 0;
    Recordatorio rec_;
    bool cargado_ = false;
    std::vector<std::pair<RECT, int>> zonas_;
    int encima_ = 0;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaInicio() { return std::make_unique<PaginaInicio>(); }

}  // namespace bu::win
