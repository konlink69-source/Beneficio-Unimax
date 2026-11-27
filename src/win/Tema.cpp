#include "win/Tema.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

// GDI+ usa min/max sin prefijo.
namespace Gdiplus {
using std::max;
using std::min;
}  // namespace Gdiplus
#include <objidl.h>
#include <gdiplus.h>
#include <uxtheme.h>

namespace bu::win::tema {

namespace {

ULONG_PTR g_gdiplus = 0;

enum TipoBoton { BOTON_NORMAL = 0, BOTON_PRINCIPAL = 1, BOTON_PELIGRO = 2 };

struct Datos {
    COLORREF fondo = FONDO;
    bool fondoPropio = false;
    std::vector<HWND> marcos;  // GROUPBOX ocultos: se dibujan como tarjetas
    std::vector<HWND> tablas;  // tablas con tarjeta alrededor
};
std::map<HWND, Datos> g_datos;

struct EstadoBoton {
    int tipo = BOTON_NORMAL;
    bool encima = false;
};
std::map<HWND, EstadoBoton> g_botones;

Gdiplus::Color colorG(COLORREF c, BYTE alfa = 255) { return Gdiplus::Color(alfa, GetRValue(c), GetGValue(c), GetBValue(c)); }

void caminoRedondeado(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
    r = std::min(r, std::min(w, h) / 2);
    if (r <= 0.5f) {
        p.AddRectangle(Gdiplus::RectF(x, y, w, h));
        return;
    }
    const float d = r * 2;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

RECT rectEn(HWND padre, HWND control) {
    RECT r;
    GetWindowRect(control, &r);
    MapWindowPoints(nullptr, padre, reinterpret_cast<POINT*>(&r), 2);
    return r;
}

int escalaPx(int v) {
    static const int ppp = [] {
        HDC dc = GetDC(nullptr);
        const int d = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        return d;
    }();
    return MulDiv(v, ppp, 96);
}

std::wstring textoVentana(HWND h) {
    const int n = GetWindowTextLengthW(h);
    std::wstring t(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(h, &t[0], n + 1);
    t.resize(static_cast<size_t>(n));
    return t;
}

bool empiezaCon(const std::wstring& t, const wchar_t* p) { return t.rfind(p, 0) == 0; }

int tipoDeBoton(HWND b, LONG estilo) {
    const std::wstring t = textoVentana(b);
    const int id = GetDlgCtrlID(b);
    if ((estilo & BS_TYPEMASK) == BS_DEFPUSHBUTTON || id == IDOK) return BOTON_PRINCIPAL;
    static const wchar_t* const principales[] = {L"Guardar", L"Registrar", L"Nuevo", L"Nueva", L"Cerrar caja",
                                                 L"Entrar", L"Actualizar ahora", L"Conectar", L"Generar"};
    for (const auto* p : principales)
        if (empiezaCon(t, p)) return BOTON_PRINCIPAL;
    static const wchar_t* const peligro[] = {L"Anular", L"Eliminar", L"Borrar", L"Desconectar", L"Reabrir"};
    for (const auto* p : peligro)
        if (empiezaCon(t, p)) return BOTON_PELIGRO;
    return BOTON_NORMAL;
}

// Color del fondo en el punto donde está un control (blanco si cae dentro de una tarjeta).
COLORREF fondoEn(HWND dlg, HWND control) {
    auto it = g_datos.find(dlg);
    const COLORREF base = it == g_datos.end() ? FONDO : it->second.fondo;
    if (it == g_datos.end()) return base;
    const RECT rc = rectEn(dlg, control);
    const POINT centro{(rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2};
    for (HWND m : it->second.marcos) {
        const RECT rm = rectEn(dlg, m);
        if (PtInRect(&rm, centro)) return TARJETA;
    }
    return base;
}

LRESULT CALLBACK procBoton(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_MOUSEMOVE: {
            auto& e = g_botones[h];
            if (!e.encima) {
                e.encima = true;
                TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, h, 0};
                TrackMouseEvent(&t);
                InvalidateRect(h, nullptr, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE:
            g_botones[h].encima = false;
            InvalidateRect(h, nullptr, FALSE);
            break;
        case BM_SETSTYLE:
            // El diálogo cambia el botón predeterminado con este mensaje: se conserva el dibujo propio.
            wp = (wp & ~static_cast<WPARAM>(BS_TYPEMASK)) | BS_OWNERDRAW;
            break;
        case WM_LBUTTONDBLCLK:
            // Dos clics rápidos cuentan como dos clics (no como "doble clic").
            msg = WM_LBUTTONDOWN;
            break;
        case WM_NCDESTROY:
            g_botones.erase(h);
            RemoveWindowSubclass(h, procBoton, 1);
            break;
        default:
            break;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

}  // namespace

void iniciar() {
    Gdiplus::GdiplusStartupInput entrada;
    Gdiplus::GdiplusStartup(&g_gdiplus, &entrada, nullptr);
}

void terminar() {
    if (g_gdiplus) Gdiplus::GdiplusShutdown(g_gdiplus);
    g_gdiplus = 0;
}

HBRUSH pincel(COLORREF c) {
    static std::map<COLORREF, HBRUSH> cache;
    auto it = cache.find(c);
    if (it != cache.end()) return it->second;
    HBRUSH b = CreateSolidBrush(c);
    cache[c] = b;
    return b;
}

void usarFondo(HWND dlg, COLORREF color) {
    g_datos[dlg].fondo = color;
    g_datos[dlg].fondoPropio = true;
}

COLORREF fondoDe(HWND dlg) {
    auto it = g_datos.find(dlg);
    return it == g_datos.end() ? FONDO : it->second.fondo;
}

void olvidar(HWND dlg) { g_datos.erase(dlg); }

void preparar(HWND dlg) {
    Datos& d = g_datos[dlg];
    if (!d.fondoPropio) d.fondo = FONDO;
    d.marcos.clear();
    d.tablas.clear();
    for (HWND c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        wchar_t clase[32] = {0};
        GetClassNameW(c, clase, 31);
        const LONG estilo = GetWindowLongW(c, GWL_STYLE);
        if (_wcsicmp(clase, L"Button") == 0) {
            const LONG tipo = estilo & BS_TYPEMASK;
            if (tipo == BS_GROUPBOX) {
                // El marco se dibuja como tarjeta; el control queda oculto (pero se sigue moviendo al escalar).
                ShowWindow(c, SW_HIDE);
                d.marcos.push_back(c);
            } else if (tipo == BS_PUSHBUTTON || tipo == BS_DEFPUSHBUTTON) {
                g_botones[c].tipo = tipoDeBoton(c, estilo);
                SetWindowLongW(c, GWL_STYLE, (estilo & ~BS_TYPEMASK) | BS_OWNERDRAW);
                SetWindowSubclass(c, procBoton, 1, 0);
                InvalidateRect(c, nullptr, TRUE);
            }
        } else if (_wcsicmp(clase, WC_LISTVIEWW) == 0) {
            SetWindowTheme(c, L"Explorer", nullptr);
            ListView_SetExtendedListViewStyleEx(c, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER,
                                                LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            SetWindowLongW(c, GWL_STYLE, estilo & ~WS_BORDER);
            SetWindowLongW(c, GWL_EXSTYLE, GetWindowLongW(c, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
            SetWindowPos(c, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            d.tablas.push_back(c);
        }
    }
    InvalidateRect(dlg, nullptr, TRUE);
}

bool pintarFondo(HWND dlg, HDC dc) {
    auto it = g_datos.find(dlg);
    if (it == g_datos.end()) return false;
    RECT r;
    GetClientRect(dlg, &r);
    FillRect(dc, &r, pincel(it->second.fondo));
    for (HWND m : it->second.marcos) {
        RECT rm = rectEn(dlg, m);
        tarjeta(dc, rm);
        // Título de la tarjeta.
        HFONT f = reinterpret_cast<HFONT>(SendMessageW(m, WM_GETFONT, 0, 0));
        LOGFONTW lf{};
        HFONT negrita = f;
        if (f && GetObjectW(f, sizeof lf, &lf)) negrita = fuente(-lf.lfHeight * 72.0 / GetDeviceCaps(dc, LOGPIXELSY), true);
        RECT rt{rm.left + escalaPx(10), rm.top + escalaPx(4), rm.right - escalaPx(10), rm.top + escalaPx(40)};
        texto(dc, U(textoVentana(m)), rt, negrita, CAFE, DT_LEFT | DT_TOP);
    }
    for (HWND t : it->second.tablas) {
        if (!IsWindowVisible(t)) continue;
        RECT rt = rectEn(dlg, t);
        InflateRect(&rt, 1, 1);
        rectRedondeado(dc, rt, escalaPx(4), TARJETA, BORDE);
    }
    return true;
}

INT_PTR colorControl(HWND dlg, UINT msg, HDC dc, HWND control, COLORREF colorTexto) {
    if (msg == WM_CTLCOLORDLG) return reinterpret_cast<INT_PTR>(pincel(fondoDe(dlg)));
    wchar_t clase[32] = {0};
    GetClassNameW(control, clase, 31);
    COLORREF fondo = fondoEn(dlg, control);
    if (_wcsicmp(clase, L"Edit") == 0) fondo = (GetWindowLongW(control, GWL_STYLE) & ES_READONLY) ? RGB(250, 248, 244) : TARJETA;
    SetBkColor(dc, fondo);
    SetTextColor(dc, IsWindowEnabled(control) ? colorTexto : TEXTO_SUAVE);
    return reinterpret_cast<INT_PTR>(pincel(fondo));
}

bool dibujarBoton(const DRAWITEMSTRUCT* d) {
    if (d->CtlType != ODT_BUTTON) return false;
    auto it = g_botones.find(d->hwndItem);
    if (it == g_botones.end()) return false;
    const int tipo = it->second.tipo;
    const bool encima = it->second.encima;
    const bool presionado = (d->itemState & ODS_SELECTED) != 0;
    const bool apagado = (d->itemState & ODS_DISABLED) != 0;
    const bool foco = (d->itemState & ODS_FOCUS) != 0;
    const RECT r = d->rcItem;
    const int w = r.right - r.left, h = r.bottom - r.top;

    // Doble búfer para que no parpadee.
    HDC mem = CreateCompatibleDC(d->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(d->hDC, w, h);
    HGDIOBJ anterior = SelectObject(mem, bmp);
    RECT local{0, 0, w, h};
    FillRect(mem, &local, pincel(fondoEn(GetParent(d->hwndItem), d->hwndItem)));

    COLORREF relleno = TARJETA, borde = BORDE, letra = CAFE;
    if (tipo == BOTON_PRINCIPAL) {
        relleno = presionado ? RGB(55, 20, 18) : (encima ? CAFE_HOVER : CAFE);
        borde = relleno;
        letra = RGB(255, 250, 242);
    } else if (tipo == BOTON_PELIGRO) {
        relleno = presionado ? RGB(250, 228, 225) : (encima ? RGB(253, 240, 238) : TARJETA);
        borde = encima ? ROJO : RGB(238, 200, 195);
        letra = ROJO;
    } else {
        relleno = presionado ? RGB(238, 228, 212) : (encima ? SELECCION : TARJETA);
        borde = encima ? CAFE_CLARO : BORDE;
    }
    if (apagado) {
        relleno = RGB(240, 236, 229);
        borde = RGB(228, 222, 212);
        letra = RGB(165, 155, 145);
    }
    RECT rb{1, 1, w - 1, h - 1};
    rectRedondeado(mem, rb, escalaPx(6), relleno, borde, foco && !apagado && tipo != BOTON_PRINCIPAL ? 2 : 1);

    HFONT f = reinterpret_cast<HFONT>(SendMessageW(d->hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ fa = SelectObject(mem, f);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, letra);
    const std::wstring t = textoVentana(d->hwndItem);
    RECT rt{escalaPx(6), 0, w - escalaPx(6), h};
    if (presionado) OffsetRect(&rt, 0, 1);
    // Si no cabe en una línea, se parte en dos y se centra verticalmente.
    RECT medida = rt;
    DrawTextW(mem, t.c_str(), -1, &medida, DT_CALCRECT | DT_WORDBREAK | DT_CENTER);
    const int altoT = medida.bottom - medida.top;
    rt.top += std::max(0, (h - altoT) / 2);
    DrawTextW(mem, t.c_str(), -1, &rt, DT_WORDBREAK | DT_CENTER | DT_END_ELLIPSIS);
    SelectObject(mem, fa);

    BitBlt(d->hDC, r.left, r.top, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, anterior);
    DeleteObject(bmp);
    DeleteDC(mem);
    return true;
}

// ---------------------------------------------------------------- Dibujo
void rectRedondeado(HDC dc, const RECT& r, int radio, COLORREF relleno, COLORREF borde, int grosor) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    const float m = grosor / 2.0f;
    Gdiplus::GraphicsPath p;
    caminoRedondeado(p, r.left + m, r.top + m, (r.right - r.left) - 2 * m, (r.bottom - r.top) - 2 * m,
                     static_cast<float>(radio));
    Gdiplus::SolidBrush b(colorG(relleno));
    g.FillPath(&b, &p);
    if (borde != relleno || grosor > 1) {
        Gdiplus::Pen pen(colorG(borde), static_cast<Gdiplus::REAL>(grosor));
        g.DrawPath(&pen, &p);
    }
}

void tarjeta(HDC dc, const RECT& r) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    const float radio = static_cast<float>(escalaPx(10));
    // Sombra muy suave debajo.
    for (int i = 3; i >= 1; --i) {
        Gdiplus::GraphicsPath s;
        caminoRedondeado(s, static_cast<float>(r.left), static_cast<float>(r.top + i), static_cast<float>(r.right - r.left),
                         static_cast<float>(r.bottom - r.top), radio);
        Gdiplus::SolidBrush bs(Gdiplus::Color(static_cast<BYTE>(6 * (4 - i)), 74, 30, 27));
        g.FillPath(&bs, &s);
    }
    Gdiplus::GraphicsPath p;
    caminoRedondeado(p, r.left + 0.5f, r.top + 0.5f, (r.right - r.left) - 1.0f, (r.bottom - r.top) - 1.0f, radio);
    Gdiplus::SolidBrush b(colorG(TARJETA));
    g.FillPath(&b, &p);
    Gdiplus::Pen pen(colorG(BORDE), 1.0f);
    g.DrawPath(&pen, &p);
}

void circulo(HDC dc, int cx, int cy, int radio, COLORREF color) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush b(colorG(color));
    g.FillEllipse(&b, cx - radio, cy - radio, radio * 2, radio * 2);
}

void arco(HDC dc, int cx, int cy, int radio, int grosor, double desde, double barrido, COLORREF color) {
    if (barrido <= 0) return;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(colorG(color), static_cast<Gdiplus::REAL>(grosor));
    const float r = radio - grosor / 2.0f;
    // GDI+ mide desde las 3 en punto; aquí 0 = arriba.
    g.DrawArc(&pen, cx - r, cy - r, 2 * r, 2 * r, static_cast<Gdiplus::REAL>(desde - 90), static_cast<Gdiplus::REAL>(barrido));
}

void linea(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int grosor) {
    Gdiplus::Graphics g(dc);
    if (x1 != x2 && y1 != y2) g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(colorG(color), static_cast<Gdiplus::REAL>(grosor));
    g.DrawLine(&pen, x1, y1, x2, y2);
}

void texto(HDC dc, const std::string& t, RECT r, HFONT f, COLORREF color, UINT formato) {
    HGDIOBJ a = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    const std::wstring w = W(t);
    if (!(formato & DT_WORDBREAK)) formato |= DT_SINGLELINE | DT_END_ELLIPSIS;
    DrawTextW(dc, w.c_str(), -1, &r, formato | DT_NOPREFIX);
    SelectObject(dc, a);
}

int altoTexto(HDC dc, HFONT f) {
    HGDIOBJ a = SelectObject(dc, f);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, a);
    return tm.tmHeight;
}

int anchoTexto(HDC dc, HFONT f, const std::string& t) {
    HGDIOBJ a = SelectObject(dc, f);
    SIZE s{0, 0};
    const std::wstring w = W(t);
    GetTextExtentPoint32W(dc, w.c_str(), static_cast<int>(w.size()), &s);
    SelectObject(dc, a);
    return s.cx;
}

namespace {
int CALLBACK hayFuente(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lp) {
    *reinterpret_cast<bool*>(lp) = true;
    return 0;
}
bool existeFuente(const wchar_t* nombre) {
    bool hay = false;
    HDC dc = GetDC(nullptr);
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(lf.lfFaceName, nombre, _TRUNCATE);
    EnumFontFamiliesExW(dc, &lf, hayFuente, reinterpret_cast<LPARAM>(&hay), 0);
    ReleaseDC(nullptr, dc);
    return hay;
}
}  // namespace

HFONT fuenteIconos(int pixeles) {
    static const wchar_t* nombre = [] {
        if (existeFuente(L"Segoe Fluent Icons")) return L"Segoe Fluent Icons";
        if (existeFuente(L"Segoe MDL2 Assets")) return L"Segoe MDL2 Assets";
        return static_cast<const wchar_t*>(nullptr);
    }();
    if (!nombre) return nullptr;
    static std::map<int, HFONT> cache;
    auto it = cache.find(pixeles);
    if (it != cache.end()) return it->second;
    HFONT f = CreateFontW(-pixeles, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, nombre);
    cache[pixeles] = f;
    return f;
}

}  // namespace bu::win::tema
