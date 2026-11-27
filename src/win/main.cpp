// Beneficio Unimax: contabilidad de compra de café (uva, pergamino húmedo y seco).
//
// Argumentos:
//   /datos "carpeta"    usa otra carpeta de datos (por defecto Documentos\BeneficioUnimax)
//   /recordatorio       solo muestra el aviso de deudas (lo usa la tarea programada de Windows)
//   /probar             junto con /recordatorio: muestra el aviso aunque no haya pendientes
//   /verrecordatorio    abre el programa mostrando el recordatorio del día
//   /noinstalar         no pregunta si se desea instalar (por ejemplo, al probarlo desde Visual Studio)
//   /actualizar <pid> "destino"   (uso interno) reemplaza la versión anterior por esta y la abre
//   /actualizado        (uso interno) muestra qué cambió en la versión nueva
#include "win/App.h"

#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>

#include "win/Actualizacion.h"
#include "win/Avisos.h"
#include "win/Impresion.h"
#include "win/Instalacion.h"
#include "win/Nube.h"
#include "win/Paginas.h"
#include "win/Sesion.h"
#include "win/Tema.h"
#include "win/Version.h"
#include "win/resource.h"

using namespace bu;
using namespace bu::win;

namespace {

struct Ventana {
    std::array<std::unique_ptr<Pagina>, NUM_PAGINAS> paginas;
    int actual = -1;
    HBITMAP logo = nullptr;  // logo en crema para la barra café
    int logoAncho = 0;
    int logoAlto = 0;
    bool datosGuardados = false;  // ya se subió a la nube antes de cerrar (actualización)
    HMENU menu = nullptr;         // menú completo (se abre desde el usuario, arriba a la derecha)
    HMENU menuEmergente = nullptr;
    std::vector<std::pair<RECT, int>> zonas;  // partes donde se puede hacer clic
    int encima = -1;                          // zona bajo el ratón
    bool compacto = false;                    // menú lateral solo con íconos (pantallas pequeñas)
    HWND ayuda = nullptr;                     // globos con el nombre de cada ícono (menú compacto)
    bool ayudaPendiente = true;               // hay que volver a crear los globos
};

Ventana g;

constexpr UINT_PTR TEMPORIZADOR_NUBE = 1;
// Zonas de clic que no son páginas.
constexpr int ZONA_USUARIO = 100;
constexpr int ZONA_NUBE = 101;
constexpr int ZONA_CAJA = 102;
constexpr int ZONA_CONSULTA = 103;

// Convierte píxeles de diseño (a 96 ppp) a píxeles de la pantalla.
int escala(int valor) {
    static const int ppp = [] {
        HDC dc = GetDC(nullptr);
        const int d = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        return d;
    }();
    return MulDiv(valor, ppp, 96);
}

int altoBarra() { return escala(64); }
int anchoMenu() { return g.compacto ? escala(68) : escala(236); }
// En pantallas o ventanas angostas el menú lateral muestra solo los íconos.
bool debeSerCompacto(int anchoCliente) { return anchoCliente < escala(1280); }
int margenContenido() { return escala(18); }

// ------------------------------------------------ Menú lateral
struct ItemMenu {
    int pagina;
    wchar_t icono;  // Segoe Fluent Icons / MDL2 Assets
    const char* nombre;
    const char* grupo;  // título encima (o nullptr)
};

const ItemMenu ITEMS[] = {
    {PAG_INICIO, 0xE80F, "Inicio", nullptr},
    {PAG_FACTURA, 0xE8A5, "Comprobante", "COMPRAS"},
    {PAG_FACTURAS, 0xE8B7, "Facturas", nullptr},
    {PAG_CLIENTES, 0xE716, "Clientes", nullptr},
    {PAG_PRECIOS, 0xE8EC, "Precio del día", nullptr},
    {PAG_CAJA, 0xE825, "Caja", "DINERO"},
    {PAG_DEUDAS, 0xE8C7, "Deudas", nullptr},
    {PAG_VENTAS, 0xE7BF, "Ventas", nullptr},
    {PAG_REPORTES, 0xE9D2, "Reportes", "INFORMES"},
    {PAG_CONFIG, 0xE713, "Configuración", nullptr},
};

// ------------------------------------------------ Tamaño de letra según la pantalla
// Todas las pestañas tienen el mismo tamaño de diseño. Se elige la letra más grande con la
// que la pestaña cabe en la pantalla: así las ventanas, los botones y las tablas llenan el espacio.
INT_PTR CALLBACK procMedir(HWND, UINT msg, WPARAM, LPARAM) { return msg == WM_INITDIALOG; }

SIZE medirPagina(HWND padre, int puntos) {
    const std::vector<BYTE> t = plantillaDialogo(IDD_PAG_FACTURA, puntos);
    SIZE r{0, 0};
    if (t.empty()) return r;
    HWND d = CreateDialogIndirectParamW(App::instancia().hInst, reinterpret_cast<LPCDLGTEMPLATEW>(t.data()), padre,
                                        procMedir, 0);
    if (!d) return r;
    RECT rc;
    GetWindowRect(d, &rc);
    DestroyWindow(d);
    r.cx = rc.right - rc.left;
    r.cy = rc.bottom - rc.top;
    return r;
}

void elegirTamanoLetra() {
    RECT trabajo;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &trabajo, 0);
    const int bordeX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
    const int bordeY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
    HWND temporal = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr,
                                    App::instancia().hInst, nullptr);
    int elegido = 8;
    g.compacto = debeSerCompacto(trabajo.right - trabajo.left);
    for (int puntos = 14; puntos >= 8; --puntos) {
        const int disponibleW = (trabajo.right - trabajo.left) - 2 * bordeX - anchoMenu() - 2 * margenContenido();
        const int disponibleH = (trabajo.bottom - trabajo.top) - GetSystemMetrics(SM_CYCAPTION) - 2 * bordeY -
                                altoBarra() - 2 * margenContenido();
        const SIZE t = medirPagina(temporal, puntos);
        if (t.cx > 0 && t.cx <= disponibleW && t.cy <= disponibleH) {
            elegido = puntos;
            break;
        }
    }
    DestroyWindow(temporal);
    establecerTamanoLetra(elegido);
}

std::string fechaLarga(const Fecha& f) {
    static const char* const DIAS[] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
    const long n = f.numeroDia();  // el 1970-01-01 fue jueves
    return std::string(DIAS[((n % 7) + 7 + 4) % 7]) + " " + f.texto();
}

// El logo es café sobre crema; para la barra café se "invierte": crema sobre café.
void prepararLogoClaro() {
    HBITMAP original = static_cast<HBITMAP>(
        LoadImageW(App::instancia().hInst, MAKEINTRESOURCEW(IDB_LOGO), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    if (!original) return;
    BITMAP bm{};
    GetObjectW(original, sizeof bm, &bm);
    const int w = bm.bmWidth, h = bm.bmHeight;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    std::vector<BYTE> px(static_cast<size_t>(w) * h * 4);
    HDC pantalla = GetDC(nullptr);
    GetDIBits(pantalla, original, 0, static_cast<UINT>(h), px.data(), &bi, DIB_RGB_COLORS);
    DeleteObject(original);
    void* bits = nullptr;
    HBITMAP claro = CreateDIBSection(pantalla, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, pantalla);
    if (!claro) return;
    const double fondo = 0.299 * 241 + 0.587 * 234 + 0.114 * 219, tinta = 0.299 * 74 + 0.587 * 30 + 0.114 * 27;
    const COLORREF barra = tema::CAFE, letra = RGB(246, 239, 226);
    auto* out = static_cast<BYTE*>(bits);
    for (size_t i = 0; i + 3 < px.size(); i += 4) {
        const double lum = 0.299 * px[i + 2] + 0.587 * px[i + 1] + 0.114 * px[i];
        const double t = std::max(0.0, std::min(1.0, (fondo - lum) / (fondo - tinta)));
        out[i] = static_cast<BYTE>(GetBValue(barra) + (GetBValue(letra) - GetBValue(barra)) * t);
        out[i + 1] = static_cast<BYTE>(GetGValue(barra) + (GetGValue(letra) - GetGValue(barra)) * t);
        out[i + 2] = static_cast<BYTE>(GetRValue(barra) + (GetRValue(letra) - GetRValue(barra)) * t);
        out[i + 3] = 255;
    }
    g.logo = claro;
    g.logoAncho = w;
    g.logoAlto = h;
}

std::string iniciales(const std::string& nombre) {
    std::string r;
    bool inicio = true;
    for (size_t i = 0; i < nombre.size() && r.size() < 2; ++i) {
        const unsigned char c = static_cast<unsigned char>(nombre[i]);
        if (c == ' ') {
            inicio = true;
        } else if (inicio && c < 0x80) {
            r += static_cast<char>(std::toupper(c));
            inicio = false;
        } else {
            inicio = false;
        }
    }
    return r.empty() ? "U" : r;
}

// Etiqueta redondeada ("chip") en la barra café; devuelve su ancho.
int chip(HDC dc, int derecha, int cy, const std::string& t, COLORREF fondo, COLORREF letra, int zona) {
    HFONT f = fuente(tamanoLetra(), true);
    const int ancho = tema::anchoTexto(dc, f, t) + escala(24), alto = escala(28);
    RECT r{derecha - ancho, cy - alto / 2, derecha, cy + alto / 2};
    tema::rectRedondeado(dc, r, alto / 2, fondo, fondo);
    tema::texto(dc, t, r, f, letra, DT_CENTER | DT_VCENTER);
    g.zonas.push_back({r, zona});
    return ancho;
}

void pintarMarco(HWND h, HDC dc) {
    RECT r;
    GetClientRect(h, &r);
    g.zonas.clear();
    const int barra = altoBarra(), menu = anchoMenu();
    // Fondo general.
    RECT contenido{menu, barra, r.right, r.bottom};
    FillRect(dc, &contenido, tema::pincel(tema::FONDO));

    // ---- Barra superior café
    RECT rb{0, 0, r.right, barra};
    FillRect(dc, &rb, tema::pincel(tema::CAFE));
    if (g.compacto) {
        // Solo el ícono del programa.
        const int lado = std::min(barra - escala(20), menu - escala(20));
        HICON icono = static_cast<HICON>(LoadImageW(App::instancia().hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, lado, lado, 0));
        if (icono) {
            DrawIconEx(dc, (menu - lado) / 2, (barra - lado) / 2, icono, lado, lado, 0, nullptr, DI_NORMAL);
            DestroyIcon(icono);
        }
    } else if (g.logo && g.logoAlto > 0) {
        const int la = barra - escala(14);
        const int an = MulDiv(g.logoAncho, la, g.logoAlto);
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ anterior = SelectObject(mem, g.logo);
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchBlt(dc, escala(16), escala(7), std::min(an, menu - escala(24)), la, mem, 0, 0,
                   std::min(g.logoAncho, MulDiv(menu - escala(24), g.logoAlto, la)), g.logoAlto, SRCCOPY);
        SelectObject(mem, anterior);
        DeleteDC(mem);
    }
    // Título de la sección.
    const char* titulo = "Beneficio Unimax";
    for (const auto& it : ITEMS)
        if (it.pagina == g.actual) titulo = it.nombre;
    if (g.actual == PAG_FACTURA) titulo = "Comprobante de compra";
    RECT rt{menu + margenContenido(), 0, r.right / 2, barra};
    tema::texto(dc, titulo, rt, fuente(tamanoLetra() * 1.7, true), RGB(255, 250, 242), DT_LEFT | DT_VCENTER);

    // A la derecha: usuario, fecha y alertas.
    int x = r.right - escala(16);
    const int cy = barra / 2;
    const Usuario& u = App::instancia().usuario();
    if (u.id) {
        const std::string nombre = u.nombre.empty() ? u.usuario : u.nombre;
        HFONT f = fuente(tamanoLetra(), true);
        const int anchoNombre = std::min(tema::anchoTexto(dc, f, nombre), escala(180));
        const int radio = escala(16);
        const int ancho = radio * 2 + escala(10) + anchoNombre + escala(22);
        RECT rz{x - ancho, cy - radio - escala(4), x, cy + radio + escala(4)};
        if (g.encima == ZONA_USUARIO) tema::rectRedondeado(dc, rz, escala(8), RGB(98, 48, 42), RGB(98, 48, 42));
        tema::circulo(dc, rz.left + escala(4) + radio, cy, radio, tema::CREMA);
        RECT ri{rz.left + escala(4), cy - radio, rz.left + escala(4) + 2 * radio, cy + radio};
        tema::texto(dc, iniciales(nombre), ri, fuente(tamanoLetra(), true), tema::CAFE, DT_CENTER | DT_VCENTER);
        RECT rn{ri.right + escala(8), 0, rz.right - escala(18), barra};
        tema::texto(dc, nombre, rn, f, RGB(255, 250, 242), DT_LEFT | DT_VCENTER);
        RECT rf{rz.right - escala(18), 0, rz.right - escala(4), barra};
        tema::texto(dc, "\xE2\x96\xBE", rf, f, tema::CREMA, DT_CENTER | DT_VCENTER);  // ▾
        g.zonas.push_back({rz, ZONA_USUARIO});
        x = rz.left - escala(16);
    }
    {
        const std::string hoy = fechaLarga(Fecha::hoy());
        HFONT f = fuente(tamanoLetra(), false);
        const int ancho = tema::anchoTexto(dc, f, hoy);
        RECT rf{x - ancho, 0, x, barra};
        tema::texto(dc, hoy, rf, f, RGB(225, 210, 190), DT_RIGHT | DT_VCENTER);
        x -= ancho + escala(16);
    }
    if (App::instancia().cajaBaja())
        x -= chip(dc, x, cy, "Caja baja: " + formatoLempiras(App::instancia().saldoCaja()), tema::ROJO, RGB(255, 255, 255),
                  ZONA_CAJA) + escala(8);
    if (Nube::instancia().soloConsulta())
        x -= chip(dc, x, cy, "SOLO CONSULTA", tema::AMBAR, RGB(255, 255, 255), ZONA_CONSULTA) + escala(8);

    // ---- Menú lateral
    RECT rm{0, barra, menu, r.bottom};
    FillRect(dc, &rm, tema::pincel(tema::TARJETA));
    RECT linea{menu - 1, barra, menu, r.bottom};
    FillRect(dc, &linea, tema::pincel(tema::BORDE));
    const int altoItem = std::max(escala(40), static_cast<int>(tamanoLetra() * 4.4));
    HFONT fItem = fuente(tamanoLetra() + 1, false), fItemSel = fuente(tamanoLetra() + 1, true);
    HFONT fGrupo = fuente(std::max(7, tamanoLetra() - 1), true);
    HFONT iconos = tema::fuenteIconos(escala(18));
    int y = barra + escala(14);
    for (const auto& it : ITEMS) {
        if (it.grupo) {
            if (g.compacto) {
                y += escala(8);
                RECT sep{escala(16), y, menu - escala(16), y + 1};
                FillRect(dc, &sep, tema::pincel(tema::BORDE));
                y += escala(9);
            } else {
                y += escala(10);
                RECT rg{escala(24), y, menu - escala(12), y + escala(20)};
                tema::texto(dc, it.grupo, rg, fGrupo, RGB(170, 155, 142), DT_LEFT | DT_VCENTER);
                y += escala(22);
            }
        }
        RECT ri{escala(g.compacto ? 8 : 10), y, menu - escala(g.compacto ? 8 : 10), y + altoItem};
        const bool elegido = it.pagina == g.actual, sobre = it.pagina == g.encima;
        if (elegido) {
            tema::rectRedondeado(dc, ri, escala(8), tema::SELECCION, tema::SELECCION);
            RECT barraSel{ri.left, ri.top + escala(8), ri.left + escala(4), ri.bottom - escala(8)};
            tema::rectRedondeado(dc, barraSel, escala(2), tema::CAFE, tema::CAFE);
        } else if (sobre) {
            tema::rectRedondeado(dc, ri, escala(8), RGB(250, 246, 240), RGB(250, 246, 240));
        }
        const COLORREF color = elegido ? tema::CAFE : tema::TEXTO;
        if (iconos) {
            RECT ric = g.compacto ? ri : RECT{ri.left + escala(14), ri.top, ri.left + escala(44), ri.bottom};
            HGDIOBJ a = SelectObject(dc, iconos);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, elegido ? tema::CAFE : tema::CAFE_CLARO);
            const wchar_t ic[2] = {it.icono, 0};
            DrawTextW(dc, ic, 1, &ric, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, a);
        }
        if (!g.compacto || !iconos) {
            RECT rtx{ri.left + (iconos ? escala(52) : escala(8)), ri.top, ri.right - escala(8), ri.bottom};
            tema::texto(dc, it.nombre, rtx, elegido ? fItemSel : fItem, color, DT_LEFT | DT_VCENTER);
        }
        g.zonas.push_back({ri, it.pagina});
        y += altoItem + escala(2);
    }
    // Abajo: estado de Google Drive y versión.
    const std::string nube = Nube::instancia().resumen();
    const int altoNube = escala(58);
    RECT rn{escala(10), r.bottom - altoNube - escala(26), menu - escala(10), r.bottom - escala(26)};
    if (g.compacto && rn.top > y + escala(10)) {
        RECT rc{escala(8), r.bottom - escala(60), menu - escala(8), r.bottom - escala(14)};
        if (g.encima == ZONA_NUBE) tema::rectRedondeado(dc, rc, escala(8), RGB(250, 246, 240), RGB(250, 246, 240));
        if (iconos) {
            HGDIOBJ a = SelectObject(dc, iconos);
            SetTextColor(dc, Nube::instancia().soloConsulta() ? tema::AMBAR
                             : (nube.find("al día") != std::string::npos ? tema::VERDE : tema::CAFE_CLARO));
            const wchar_t ic[2] = {0xE753, 0};
            DrawTextW(dc, ic, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, a);
        }
        g.zonas.push_back({rc, ZONA_NUBE});
    } else if (rn.top > y + escala(10)) {
        if (g.encima == ZONA_NUBE) tema::rectRedondeado(dc, rn, escala(8), RGB(250, 246, 240), RGB(250, 246, 240));
        if (iconos) {
            RECT ric{rn.left + escala(14), rn.top, rn.left + escala(44), rn.bottom};
            HGDIOBJ a = SelectObject(dc, iconos);
            SetTextColor(dc, tema::CAFE_CLARO);
            const wchar_t ic[2] = {0xE753, 0};  // nube
            DrawTextW(dc, ic, 1, &ric, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, a);
        }
        RECT r1{rn.left + (iconos ? escala(52) : escala(18)), rn.top + escala(8), rn.right - escala(6), rn.top + altoNube / 2};
        tema::texto(dc, "Google Drive", r1, fuente(tamanoLetra(), true), tema::TEXTO, DT_LEFT | DT_VCENTER);
        RECT r2{r1.left, rn.top + altoNube / 2, r1.right, rn.bottom - escala(6)};
        std::string estado = nube.empty() ? "Sin conectar" : nube.substr(nube.rfind("Nube: ", 0) == 0 ? 6 : 0);
        const COLORREF ce = Nube::instancia().soloConsulta() ? tema::AMBAR
                            : (nube.find("al día") != std::string::npos ? tema::VERDE : tema::TEXTO_SUAVE);
        tema::texto(dc, estado, r2, fuente(std::max(7, tamanoLetra() - 1), false), ce, DT_LEFT | DT_VCENTER);
        g.zonas.push_back({rn, ZONA_NUBE});
        RECT rv{escala(24), r.bottom - escala(24), menu - escala(12), r.bottom - escala(6)};
        tema::texto(dc, "Versión " + std::to_string(VERSION_PROGRAMA), rv, fuente(std::max(7, tamanoLetra() - 2), false),
                    RGB(175, 162, 150), DT_LEFT | DT_VCENTER);
    }
}

// Menú compacto: un globo con el nombre de cada ícono al pasar el ratón.
void actualizarAyuda(HWND h) {
    if (!g.ayudaPendiente) return;
    g.ayudaPendiente = false;
    if (!g.ayuda) {
        g.ayuda = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, h, nullptr,
                                  App::instancia().hInst, nullptr);
        SendMessageW(g.ayuda, WM_SETFONT, reinterpret_cast<WPARAM>(fuente(tamanoLetra(), false)), FALSE);
    }
    // Se quitan los globos anteriores.
    TTTOOLINFOW ti{};
    ti.cbSize = sizeof ti;
    while (SendMessageW(g.ayuda, TTM_ENUMTOOLSW, 0, reinterpret_cast<LPARAM>(&ti))) SendMessageW(g.ayuda, TTM_DELTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    if (!g.compacto) return;
    static std::vector<std::wstring> textos;
    textos.clear();
    for (const auto& z : g.zonas) {
        std::string nombre;
        for (const auto& it : ITEMS)
            if (it.pagina == z.second) nombre = it.nombre;
        if (z.second == ZONA_NUBE) nombre = "Google Drive: " + Nube::instancia().resumen();
        if (nombre.empty()) continue;
        textos.push_back(W(nombre));
    }
    size_t i = 0;
    for (const auto& z : g.zonas) {
        bool tiene = z.second == ZONA_NUBE;
        for (const auto& it : ITEMS)
            if (it.pagina == z.second) tiene = true;
        if (!tiene || i >= textos.size()) continue;
        TTTOOLINFOW t{};
        t.cbSize = sizeof t;
        t.uFlags = TTF_SUBCLASS;
        t.hwnd = h;
        t.uId = static_cast<UINT_PTR>(z.second + 1);
        t.rect = z.first;
        t.lpszText = const_cast<LPWSTR>(textos[i++].c_str());
        SendMessageW(g.ayuda, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&t));
    }
}

void repintarMarco(HWND h) {
    RECT r;
    GetClientRect(h, &r);
    RECT barra{0, 0, r.right, altoBarra()};
    InvalidateRect(h, &barra, FALSE);
    RECT menu{0, 0, anchoMenu(), r.bottom};
    InvalidateRect(h, &menu, FALSE);
}

int zonaEn(int x, int y) {
    const POINT p{x, y};
    for (const auto& z : g.zonas)
        if (PtInRect(&z.first, p)) return z.second;
    return -1;
}

HWND paginaActiva() { return g.actual >= 0 ? g.paginas[static_cast<size_t>(g.actual)]->hwnd() : nullptr; }

void ubicarPaginas(HWND h) {
    RECT r;
    GetClientRect(h, &r);
    const int m = margenContenido();
    const int x = anchoMenu() + m, y = altoBarra() + m;
    const int ancho = std::max(100, static_cast<int>(r.right) - x - m);
    const int alto = std::max(100, static_cast<int>(r.bottom) - y - m);
    for (auto& p : g.paginas)
        if (p) SetWindowPos(p->hwnd(), HWND_TOP, x, y, ancho, alto, SWP_NOACTIVATE);
    InvalidateRect(h, nullptr, FALSE);
}

void mostrarPagina(int indice) {
    if (indice < 0 || indice >= NUM_PAGINAS) return;
    for (int i = 0; i < NUM_PAGINAS; ++i)
        ShowWindow(g.paginas[static_cast<size_t>(i)]->hwnd(), i == indice ? SW_SHOW : SW_HIDE);
    g.actual = indice;
    Pagina* p = g.paginas[static_cast<size_t>(indice)].get();
    SetWindowPos(p->hwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    intentar(p->hwnd(), [&] { p->alMostrar(); });
    App::instancia().actualizarCaja();
    if (App::instancia().ventana) repintarMarco(App::instancia().ventana);
}

bool crearContenido(HWND h) {
    std::array<std::pair<int, std::unique_ptr<Pagina> (*)()>, NUM_PAGINAS> definiciones{};
    definiciones[PAG_INICIO] = {IDD_PAG_INICIO, crearPaginaInicio};
    definiciones[PAG_FACTURA] = {IDD_PAG_FACTURA, crearPaginaFactura};
    definiciones[PAG_FACTURAS] = {IDD_PAG_FACTURAS, crearPaginaFacturas};
    definiciones[PAG_CLIENTES] = {IDD_PAG_CLIENTES, crearPaginaClientes};
    definiciones[PAG_PRECIOS] = {IDD_PAG_PRECIOS, crearPaginaPrecios};
    definiciones[PAG_DEUDAS] = {IDD_PAG_DEUDAS, crearPaginaDeudas};
    definiciones[PAG_CAJA] = {IDD_PAG_CAJA, crearPaginaCaja};
    definiciones[PAG_VENTAS] = {IDD_PAG_VENTAS, crearPaginaVentas};
    definiciones[PAG_REPORTES] = {IDD_PAG_REPORTES, crearPaginaReportes};
    definiciones[PAG_CONFIG] = {IDD_PAG_CONFIG, crearPaginaConfiguracion};
    for (int i = 0; i < NUM_PAGINAS; ++i) {
        auto pagina = definiciones[static_cast<size_t>(i)].second();
        if (!pagina->crear(h, definiciones[static_cast<size_t>(i)].first)) return false;
        g.paginas[static_cast<size_t>(i)] = std::move(pagina);
    }
    return true;
}

// Menú completo (Archivo, Sesión, Ver, Nube, Ayuda) que se abre desde el nombre del usuario.
void abrirMenuUsuario(HWND h, const RECT& zona) {
    if (!g.menuEmergente) {
        g.menu = LoadMenuW(App::instancia().hInst, MAKEINTRESOURCEW(IDR_MENU));
        g.menuEmergente = CreatePopupMenu();
        const int n = GetMenuItemCount(g.menu);
        for (int i = 0; i < n; ++i) {
            wchar_t texto[128] = {0};
            GetMenuStringW(g.menu, static_cast<UINT>(i), texto, 127, MF_BYPOSITION);
            AppendMenuW(g.menuEmergente, MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(GetSubMenu(g.menu, i)), texto);
        }
    }
    POINT p{zona.right, zona.bottom};
    ClientToScreen(h, &p);
    TrackPopupMenu(g.menuEmergente, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON, p.x, p.y + escala(4), 0, h, nullptr);
}

void instalarDesdeMenu(HWND h) {
    std::string mensaje;
    const bool desdeOtraCarpeta = !ejecutandoDesdeInstalacion(App::instancia().rutas().exe);
    if (!instalar(App::instancia().rutas().exe, mensaje)) {
        mostrarError(h, mensaje);
        return;
    }
    if (!mensaje.empty()) {
        mostrarAviso(h, mensaje);
        return;
    }
    mostrarInfo(h, desdeOtraCarpeta
                       ? "Listo. Se creó el ícono \"Beneficio Unimax\" en el escritorio. Cierre esta ventana y, de "
                         "ahora en adelante, abra el programa con ese ícono."
                       : "Listo. El ícono \"Beneficio Unimax\" está en el escritorio y en el menú Inicio.");
}

void despuesDeInstalar(HWND h) {
    // Si el aviso diario ya estaba activo, se vuelve a crear para que use el programa instalado.
    if (avisoWindowsActivo()) {
        std::string error;
        activarAvisoWindows(repo().config("HoraAviso", "07:30"), error);
    }
    mostrarInfo(h, "Beneficio Unimax quedó instalado.\n\nDe ahora en adelante ábralo con el ícono \"Beneficio "
                   "Unimax\" del escritorio (también está en el menú Inicio). Ya puede borrar el archivo que "
                   "descargó.");
}

void acercaDe(HWND h) {
    const Rutas& r = App::instancia().rutas();
    mostrarInfo(h, "Beneficio Unimax, versión " + std::to_string(VERSION_PROGRAMA) + "\nContabilidad de compra y venta de café (uva, pergamino húmedo y seco).\n\n"
                   "Base de datos (Access): " + U(r.baseDatos) + "\nPlantillas de Excel: " + U(r.plantillas) +
                   "\nDocumentos generados: " + U(r.documentos) + "\n\n1 quintal = 100 libras. Moneda: Lempiras (L).");
}

bool hayTrabajoSinGuardar() {
    for (const auto& p : g.paginas)
        if (p && p->trabajoSinGuardar()) return true;
    return false;
}

void cerrarPrograma(HWND h) {
    if (hayTrabajoSinGuardar() && !confirmar(h, "Hay un comprobante a medio llenar que no se ha guardado.\n\n¿Salir de todas formas?"))
        return;
    if (!g.datosGuardados) {
        App::instancia().respaldoAutomatico();
        Nube::instancia().alSalir(h, "al salir");
    }
    g.datosGuardados = true;
    DestroyWindow(h);
}

void actualizarTitulo(HWND h) {
    std::wstring t = L"Beneficio Unimax - Contabilidad de compra de café";
    if (Nube::instancia().soloConsulta()) t += L"   [SOLO CONSULTA]";
    SetWindowTextW(h, t.c_str());
}

LRESULT CALLBACK procVentana(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            App::instancia().ventana = h;
            prepararLogoClaro();
            if (!crearContenido(h)) {
                MessageBoxW(h, L"No se pudieron crear las ventanas del programa.", L"Beneficio Unimax", MB_ICONERROR);
                return -1;
            }
            ubicarPaginas(h);
            mostrarPagina(PAG_INICIO);
            actualizarTitulo(h);
            SetTimer(h, TEMPORIZADOR_NUBE, 60 * 1000, nullptr);
            return 0;
        case WM_TIMER:
            if (wp == TEMPORIZADOR_NUBE) {
                Nube::instancia().cadaMinuto();
                repintarMarco(h);
            }
            return 0;
        case WM_CLOSE:
            cerrarPrograma(h);
            return 0;
        case WM_SIZE: {
            const bool compacto = debeSerCompacto(LOWORD(lp));
            if (compacto != g.compacto) g.compacto = compacto;
            g.ayudaPendiente = true;
            if (g.paginas[0]) ubicarPaginas(h);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;  // todo se pinta en WM_PAINT (sin parpadeo)
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT r;
            GetClientRect(h, &r);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, std::max(1L, r.right), std::max(1L, r.bottom));
            HGDIOBJ anterior = SelectObject(mem, bmp);
            pintarMarco(h, mem);
            BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
            actualizarAyuda(h);
            SelectObject(mem, anterior);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            const int z = zonaEn(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
            if (z != g.encima) {
                g.encima = z;
                TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, h, 0};
                TrackMouseEvent(&t);
                repintarMarco(h);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            if (g.encima != -1) {
                g.encima = -1;
                repintarMarco(h);
            }
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                POINT p;
                GetCursorPos(&p);
                ScreenToClient(h, &p);
                if (zonaEn(p.x, p.y) >= 0) {
                    SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32649)));  // IDC_HAND
                    return TRUE;
                }
            }
            break;
        case WM_LBUTTONUP: {
            const POINT p{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
            for (const auto& zona : g.zonas) {
                if (!PtInRect(&zona.first, p)) continue;
                const int z = zona.second;
                if (z >= 0 && z < NUM_PAGINAS) mostrarPagina(z);
                else if (z == ZONA_USUARIO) abrirMenuUsuario(h, zona.first);
                else if (z == ZONA_NUBE) Nube::instancia().dialogoNube(h);
                else if (z == ZONA_CAJA) mostrarPagina(PAG_CAJA);
                else if (z == ZONA_CONSULTA)
                    mostrarInfo(h, "Este equipo está en SOLO CONSULTA: " + Nube::instancia().motivoSoloConsulta() + ".");
                break;
            }
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
            mm->ptMinTrackSize.x = 900;
            mm->ptMinTrackSize.y = 600;
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDM_RESPALDAR:
                    App::instancia().respaldar(h);
                    return 0;
                case IDM_CARPETA:
                    abrirCarpeta(App::instancia().rutas().datos);
                    return 0;
                case IDM_PLANTILLAS:
                    abrirCarpeta(App::instancia().rutas().plantillas);
                    return 0;
                case IDM_INSTALAR:
                    instalarDesdeMenu(h);
                    return 0;
                case IDM_CAMBIAR_CLAVE:
                    dialogoCambiarMiContrasena(h);
                    return 0;
                case IDM_USUARIOS:
                    dialogoUsuarios(h);
                    return 0;
                case IDM_CERRAR_SESION:
                    ShowWindow(h, SW_HIDE);
                    if (!iniciarSesion(nullptr)) {
                        if (!g.datosGuardados) {
                            App::instancia().respaldoAutomatico();
                            Nube::instancia().alSalir(nullptr, "al salir");
                        }
                        g.datosGuardados = true;
                        DestroyWindow(h);
                        return 0;
                    }
                    ShowWindow(h, SW_SHOWMAXIMIZED);
                    InvalidateRect(h, nullptr, TRUE);
                    if (g.actual >= 0) mostrarPagina(g.actual);
                    return 0;
                case IDM_INFORME:
                    verInformeDia(h, Fecha::hoy());
                    return 0;
                case IDM_RECORDATORIO:
                    App::instancia().mostrarRecordatorio(h, true);
                    return 0;
                case IDM_ACERCA:
                    acercaDe(h);
                    return 0;
                case IDM_SALIR:
                    cerrarPrograma(h);
                    return 0;
                case IDM_NUBE_SINCRONIZAR:
                    Nube::instancia().sincronizarAhora(h);
                    return 0;
                case IDM_NUBE_RESPALDOS:
                    Nube::instancia().dialogoRespaldos(h);
                    return 0;
                case IDM_NUBE_CONFIGURAR:
                    Nube::instancia().dialogoNube(h);
                    actualizarTitulo(h);
                    return 0;
                case IDM_ACTUALIZAR:
                    if (VERSION_PROGRAMA <= 0)
                        mostrarInfo(h, "Esta copia del programa se compiló en la computadora (sin número de versión), así "
                                       "que no busca actualizaciones. Descargue la versión publicada para recibirlas.");
                    else
                        buscarActualizacion(h, true);
                    return 0;
                default:
                    break;
            }
            break;
        case WM_APP_INICIO:
            if (lp == 1) intentar(h, [&] { despuesDeInstalar(h); });
            if (lp == 2) mostrarNovedades(h);
            App::instancia().mostrarRecordatorio(h, wp != 0);
            if (g.actual >= 0) mostrarPagina(g.actual);  // refresca por si cambió algo
            if (VERSION_PROGRAMA > 0) buscarActualizacion(h, false);
            return 0;
        case WM_APP_NUBE:
            if (wp == 1) {
                Nube::instancia().perdioBloqueo(h);
                App::instancia().establecerUsuario(App::instancia().usuario());  // aplica el solo consulta
                actualizarTitulo(h);
            }
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case WM_APP_ACTUALIZACION: {
            std::unique_ptr<InfoVersion> info(reinterpret_cast<InfoVersion*>(lp));
            if (!info) return 0;
            if (info->numero < 0) {
                if (wp) mostrarAviso(h, "No se pudo revisar si hay actualizaciones:\n" + info->notas);
            } else if (info->numero <= VERSION_PROGRAMA) {
                if (wp) mostrarInfo(h, "Ya tiene la versión más reciente (" + std::to_string(VERSION_PROGRAMA) + ").");
            } else if (ofrecerActualizacion(h, *info, hayTrabajoSinGuardar())) {
                g.datosGuardados = true;
                DestroyWindow(h);
            }
            return 0;
        }
        case WM_APP_RECORDATORIO:
            App::instancia().mostrarRecordatorio(h, true);
            return 0;
        case WM_APP_IR_PAGINA:
            mostrarPagina(static_cast<int>(wp));
            return 0;
        case WM_APP_MARCA_AGUA: {
            const int marca = App::instancia().marcaAgua();
            for (auto& p : g.paginas) p->aplicarMarcaAgua(marca);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(h, TEMPORIZADOR_NUBE);
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool modoAviso = false, probar = false, verRecordatorio = false, recienInstalado = false, noInstalar = false;
    bool recienActualizado = false;
    DWORD pidAnterior = 0;
    std::wstring carpetaDatos, destinoActualizacion;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (_wcsicmp(a.c_str(), L"/recordatorio") == 0) modoAviso = true;
        else if (_wcsicmp(a.c_str(), L"/probar") == 0) probar = true;
        else if (_wcsicmp(a.c_str(), L"/verrecordatorio") == 0) verRecordatorio = true;
        else if (_wcsicmp(a.c_str(), L"/instalado") == 0) recienInstalado = true;
        else if (_wcsicmp(a.c_str(), L"/noinstalar") == 0) noInstalar = true;
        else if (_wcsicmp(a.c_str(), L"/datos") == 0 && i + 1 < argc) carpetaDatos = argv[++i];
        else if (_wcsicmp(a.c_str(), L"/actualizado") == 0) recienActualizado = true;
        else if (_wcsicmp(a.c_str(), L"/actualizar") == 0 && i + 2 < argc) {
            pidAnterior = static_cast<DWORD>(_wtoi(argv[++i]));
            destinoActualizacion = argv[++i];
        }
    }
    LocalFree(argv);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (!destinoActualizacion.empty()) {
        // Versión nueva recién descargada: reemplaza a la anterior y la abre.
        const int r = completarActualizacion(pidAnterior, destinoActualizacion, carpetaDatos);
        CoUninitialize();
        return r;
    }
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof icc;
    icc.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    App& app = App::instancia();
    app.hInst = hInst;
    tema::iniciar();

    HANDLE instancia = nullptr;
    if (!modoAviso) {
        // Una sola ventana del programa: si ya está abierto, se trae al frente.
        instancia = CreateMutexW(nullptr, TRUE, L"BeneficioUnimax.InstanciaUnica");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (HWND otra = FindWindowW(CLASE_VENTANA, nullptr)) {
                ShowWindow(otra, SW_RESTORE);
                SetForegroundWindow(otra);
                if (verRecordatorio) PostMessageW(otra, WM_APP_RECORDATORIO, 0, 0);
            }
            CoUninitialize();
            return 0;
        }

        // Abierto desde Descargas u otra carpeta: se ofrece instalarlo con un ícono en el escritorio.
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (!noInstalar && !recienInstalado && !recienActualizado && !ejecutandoDesdeInstalacion(exe) &&
            !noPreguntarInstalacion()) {
            const int respuesta = MessageBoxW(
                nullptr,
                L"¿Desea instalar Beneficio Unimax en esta computadora?\n\n"
                L"Se creará un ícono en el escritorio para abrir el programa con doble clic, sin volver a "
                L"descargarlo. Si ya estaba instalado, se actualizará a esta versión. Sus datos no cambian.\n\n"
                L"(Si elige No, el programa se abre igual y no se vuelve a preguntar; puede instalarlo después "
                L"desde el menú Archivo.)",
                L"Beneficio Unimax", MB_YESNO | MB_ICONQUESTION);
            if (respuesta == IDYES) {
                std::string mensaje;
                if (instalar(exe, mensaje)) {
                    if (!mensaje.empty()) MessageBoxW(nullptr, W(mensaje).c_str(), L"Beneficio Unimax", MB_ICONWARNING);
                    std::wstring argumentos = L"/instalado";
                    if (!carpetaDatos.empty()) argumentos += L" /datos \"" + carpetaDatos + L"\"";
                    CloseHandle(instancia);  // para que la copia instalada pueda abrirse
                    ShellExecuteW(nullptr, L"open", exeInstalado().c_str(), argumentos.c_str(),
                                  carpetaInstalacion().c_str(), SW_SHOWNORMAL);
                    CoUninitialize();
                    return 0;
                }
                MessageBoxW(nullptr, W(mensaje).c_str(), L"Beneficio Unimax", MB_ICONERROR);
            } else {
                guardarNoPreguntarInstalacion();
            }
        }
    }

    std::string error;
    if (!app.iniciar(carpetaDatos, error, !modoAviso)) {
        if (!modoAviso && !error.empty()) MessageBoxW(nullptr, W(error).c_str(), L"Beneficio Unimax", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
    if (modoAviso) {
        const int r = ejecutarModoAviso(hInst, probar);
        CoUninitialize();
        return r;
    }

    // Tamaño de letra según la pantalla (también lo usan las ventanas de ingreso).
    elegirTamanoLetra();
    // Usuario y contraseña antes de mostrar cualquier dato.
    if (!iniciarSesion(nullptr)) {
        Nube::instancia().alSalir(nullptr, "al salir");  // libera los datos para otras computadoras
        CoUninitialize();
        return 0;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = procVentana;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;  // el menú se abre desde el nombre del usuario (arriba a la derecha)
    wc.lpszClassName = CLASE_VENTANA;
    RegisterClassExW(&wc);

    HWND h = CreateWindowExW(0, CLASE_VENTANA, L"Beneficio Unimax - Contabilidad de compra de café",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 780, nullptr,
                             nullptr, hInst, nullptr);
    if (!h) {
        CoUninitialize();
        return 1;
    }
    ShowWindow(h, SW_SHOWMAXIMIZED);
    UpdateWindow(h);
    PostMessageW(h, WM_APP_INICIO, verRecordatorio ? 1 : 0, recienInstalado ? 1 : (recienActualizado ? 2 : 0));

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        HWND activa = paginaActiva();
        if (activa && IsDialogMessageW(activa, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    tema::terminar();
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
