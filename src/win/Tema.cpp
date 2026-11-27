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
enum TipoCampo { CAMPO_TEXTO = 1, CAMPO_BUSQUEDA = 2, CAMPO_FECHA = 3 };

struct Datos {
    COLORREF fondo = FONDO;
    bool fondoPropio = false;
    bool conLogo = false;
    std::vector<HWND> marcos;  // GROUPBOX ocultos: se dibujan como tarjetas
    std::vector<HWND> tablas;  // tablas con tarjeta alrededor
    HBITMAP cache = nullptr;   // fondo ya dibujado (tarjetas + logo) del tamaño de la ventana
    HBRUSH patron = nullptr;   // pincel con ese fondo (para etiquetas y casillas "transparentes")
    SIZE tam{0, 0};
};
std::map<HWND, Datos> g_datos;

struct EstadoBoton {
    int tipo = BOTON_NORMAL;
    bool encima = false;
};
std::map<HWND, EstadoBoton> g_botones;
std::map<HWND, int> g_campos;  // campos con borde redondeado: tipo

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

// Copia en "dc" (en 0,0) el pedazo del fondo de "dlg" que queda detrás de "control".
bool copiarFondo(HDC dc, HWND dlg, HWND control, int ancho, int alto) {
    auto it = g_datos.find(dlg);
    if (it == g_datos.end() || !it->second.cache) return false;
    const RECT r = rectEn(dlg, control);
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ a = SelectObject(mem, it->second.cache);
    BitBlt(dc, 0, 0, ancho, alto, mem, r.left, r.top, SRCCOPY);
    SelectObject(mem, a);
    DeleteDC(mem);
    return true;
}

// ---- Logo de fondo: café con transparencia según lo oscuro de cada punto.
Gdiplus::Bitmap* logoAlfa() {
    static Gdiplus::Bitmap* logo = [] () -> Gdiplus::Bitmap* {
        HBITMAP original = static_cast<HBITMAP>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(311), IMAGE_BITMAP, 0, 0,
                                                           LR_CREATEDIBSECTION));
        if (!original) return nullptr;
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
        ReleaseDC(nullptr, pantalla);
        DeleteObject(original);
        auto* b = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);
        Gdiplus::Rect todo(0, 0, w, h);
        Gdiplus::BitmapData datos{};
        if (b->LockBits(&todo, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &datos) != Gdiplus::Ok) return b;
        // Fondo crema = transparente; café = opaco.
        const double fondo = 0.299 * 241 + 0.587 * 234 + 0.114 * 219, tinta = 0.299 * 74 + 0.587 * 30 + 0.114 * 27;
        for (int y = 0; y < h; ++y) {
            auto* fila = static_cast<BYTE*>(datos.Scan0) + y * datos.Stride;
            for (int x = 0; x < w; ++x) {
                const BYTE* o = &px[(static_cast<size_t>(y) * w + x) * 4];
                const double lum = 0.299 * o[2] + 0.587 * o[1] + 0.114 * o[0];
                const double t = std::max(0.0, std::min(1.0, (fondo - lum) / (fondo - tinta)));
                fila[x * 4 + 0] = 27;   // B
                fila[x * 4 + 1] = 30;   // G
                fila[x * 4 + 2] = 74;   // R
                fila[x * 4 + 3] = static_cast<BYTE>(t * 255);
            }
        }
        b->UnlockBits(&datos);
        return b;
    }();
    return logo;
}

void dibujarLogo(HDC dc, const RECT& area, float opacidad) {
    Gdiplus::Bitmap* logo = logoAlfa();
    if (!logo) return;
    const int aw = area.right - area.left, ah = area.bottom - area.top;
    if (aw < 50 || ah < 50) return;
    const double proporcion = static_cast<double>(logo->GetWidth()) / logo->GetHeight();
    int w = aw * 70 / 100;
    int h = static_cast<int>(w / proporcion);
    if (h > ah * 45 / 100) {
        h = ah * 45 / 100;
        w = static_cast<int>(h * proporcion);
    }
    const int x = area.left + (aw - w) / 2, y = area.top + (ah - h) / 2;
    Gdiplus::Graphics g(dc);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    Gdiplus::ColorMatrix m = {{{1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, opacidad, 0}, {0, 0, 0, 0, 1}}};
    Gdiplus::ImageAttributes atributos;
    atributos.SetColorMatrix(&m);
    g.DrawImage(logo, Gdiplus::Rect(x, y, w, h), 0, 0, static_cast<INT>(logo->GetWidth()),
                static_cast<INT>(logo->GetHeight()), Gdiplus::UnitPixel, &atributos);
}

constexpr float OPACIDAD_LOGO = 0.085f;

// Dibuja el fondo completo de una ventana: color, tarjetas, marcos de tablas y el logo encima.
void dibujarFondo(HWND dlg, Datos& d, HDC dc, const RECT& r) {
    FillRect(dc, &r, pincel(d.fondo));
    for (HWND m : d.marcos) {
        const RECT rm = rectEn(dlg, m);
        tarjeta(dc, rm);
        HFONT f = reinterpret_cast<HFONT>(SendMessageW(m, WM_GETFONT, 0, 0));
        LOGFONTW lf{};
        HFONT negrita = f;
        if (f && GetObjectW(f, sizeof lf, &lf)) negrita = fuente(-lf.lfHeight * 72.0 / GetDeviceCaps(dc, LOGPIXELSY), true);
        RECT rt{rm.left + escalaPx(12), rm.top + escalaPx(5), rm.right - escalaPx(12), rm.top + escalaPx(40)};
        texto(dc, U(textoVentana(m)), rt, negrita, CAFE, DT_LEFT | DT_TOP);
    }
    for (HWND t : d.tablas) {
        if (!IsWindowVisible(t)) continue;
        RECT rt = rectEn(dlg, t);
        InflateRect(&rt, 1, 1);
        rectRedondeado(dc, rt, escalaPx(6), TARJETA, BORDE);
    }
    // El logo va encima de las tarjetas, muy tenue, para que se vea siempre.
    if (d.conLogo) dibujarLogo(dc, r, OPACIDAD_LOGO);
}

void liberarCache(Datos& d) {
    if (d.patron) DeleteObject(d.patron);
    if (d.cache) DeleteObject(d.cache);
    d.patron = nullptr;
    d.cache = nullptr;
    d.tam = {0, 0};
}

void construirCache(HWND dlg, Datos& d) {
    RECT r;
    GetClientRect(dlg, &r);
    if (r.right <= 0 || r.bottom <= 0) return;
    liberarCache(d);
    HDC pantalla = GetDC(dlg);
    d.cache = CreateCompatibleBitmap(pantalla, r.right, r.bottom);
    HDC mem = CreateCompatibleDC(pantalla);
    HGDIOBJ a = SelectObject(mem, d.cache);
    dibujarFondo(dlg, d, mem, r);
    SelectObject(mem, a);
    DeleteDC(mem);
    ReleaseDC(dlg, pantalla);
    d.patron = CreatePatternBrush(d.cache);
    d.tam = {r.right, r.bottom};
}

// Cada tabla muestra de fondo el mismo pedazo del fondo de la ventana (con el logo).
void fondoTablas(HWND dlg, Datos& d) {
    if (!d.cache) return;
    for (HWND t : d.tablas) {
        RECT rc;
        GetClientRect(t, &rc);
        if (rc.right <= 0 || rc.bottom <= 0) continue;
        POINT o{0, 0};
        MapWindowPoints(t, dlg, &o, 1);
        HDC pantalla = GetDC(t);
        HBITMAP bmp = CreateCompatibleBitmap(pantalla, rc.right, rc.bottom);
        HDC mem = CreateCompatibleDC(pantalla), src = CreateCompatibleDC(pantalla);
        HGDIOBJ a = SelectObject(mem, bmp), b = SelectObject(src, d.cache);
        BitBlt(mem, 0, 0, rc.right, rc.bottom, src, o.x, o.y, SRCCOPY);
        SelectObject(mem, a);
        SelectObject(src, b);
        DeleteDC(mem);
        DeleteDC(src);
        ReleaseDC(t, pantalla);
        LVBKIMAGEW bk{};
        bk.ulFlags = LVBKIF_TYPE_WATERMARK;
        bk.hbm = bmp;  // la tabla se queda con la imagen
        SendMessageW(t, LVM_SETBKIMAGEW, 0, reinterpret_cast<LPARAM>(&bk));
        ListView_SetTextBkColor(t, CLR_NONE);
        ListView_SetBkColor(t, TARJETA);
        InvalidateRect(t, nullptr, TRUE);
    }
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

// ---- Campos de texto y fechas: borde redondeado dibujado en el área "no cliente".
COLORREF rellenoCampo(HWND h) {
    const int tipo = g_campos.count(h) ? g_campos[h] : CAMPO_TEXTO;
    if (!IsWindowEnabled(h)) return RGB(243, 239, 232);
    if (tipo == CAMPO_FECHA) return TARJETA;
    if (GetWindowLongW(h, GWL_STYLE) & ES_READONLY) return RGB(246, 242, 236);
    return GetFocus() == h ? TARJETA : CAMPO;
}

int altoLetra(HWND h) {
    HFONT f = reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0));
    HDC dc = GetDC(h);
    HGDIOBJ a = SelectObject(dc, f ? f : GetStockObject(DEFAULT_GUI_FONT));
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, a);
    ReleaseDC(h, dc);
    return tm.tmHeight;
}

void pintarMarcoCampo(HWND h) {
    RECT rv;
    GetWindowRect(h, &rv);
    const int w = rv.right - rv.left, alto = rv.bottom - rv.top;
    if (w <= 0 || alto <= 0) return;
    RECT rc;
    GetClientRect(h, &rc);
    MapWindowPoints(h, nullptr, reinterpret_cast<POINT*>(&rc), 2);
    OffsetRect(&rc, -rv.left, -rv.top);
    HDC dc = GetWindowDC(h);
    ExcludeClipRect(dc, rc.left, rc.top, rc.right, rc.bottom);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, w, alto);
    HGDIOBJ a = SelectObject(mem, bmp);
    HWND padre = GetParent(h);
    RECT todo{0, 0, w, alto};
    if (!copiarFondo(mem, padre, h, w, alto)) FillRect(mem, &todo, pincel(fondoEn(padre, h)));
    const bool foco = GetFocus() == h || IsChild(h, GetFocus());
    const COLORREF relleno = rellenoCampo(h);
    rectRedondeado(mem, todo, std::min(escalaPx(8), alto / 2), relleno, foco ? CAFE_CLARO : BORDE, foco ? 2 : 1);
    if (g_campos[h] == CAMPO_BUSQUEDA) {
        if (HFONT ic = fuenteIconos(std::max(10, alto * 45 / 100))) {
            RECT ri{escalaPx(4), 0, escalaPx(4) + escalaPx(24), alto};
            HGDIOBJ fa = SelectObject(mem, ic);
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, TEXTO_SUAVE);
            DrawTextW(mem, L"\xE721", 1, &ri, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(mem, fa);
        }
    }
    // El relleno del área de texto debe ser igual al del campo.
    FillRect(mem, &rc, pincel(relleno));
    BitBlt(dc, 0, 0, w, alto, mem, 0, 0, SRCCOPY);
    SelectObject(mem, a);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(h, dc);
}

LRESULT CALLBACK procCampo(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) {
                auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
                RECT& r = p->rgrc[0];
                const int tipo = g_campos[h];
                const int alto = r.bottom - r.top;
                const bool multilinea = (GetWindowLongW(h, GWL_STYLE) & ES_MULTILINE) && tipo != CAMPO_FECHA;
                int arriba;
                if (tipo == CAMPO_FECHA) arriba = std::max(1, std::min(escalaPx(3), (alto - altoLetra(h) - 2) / 2));
                else if (multilinea) arriba = escalaPx(6);
                else arriba = std::max(1, (alto - altoLetra(h)) / 2);
                const int izq = tipo == CAMPO_FECHA ? escalaPx(6) : (escalaPx(10) + (tipo == CAMPO_BUSQUEDA ? escalaPx(22) : 0));
                const int der = tipo == CAMPO_FECHA ? escalaPx(4) : escalaPx(8);
                r.left += izq;
                r.right -= der;
                r.top += arriba;
                r.bottom -= arriba;
                if (r.right < r.left) r.right = r.left;
                if (r.bottom < r.top) r.bottom = r.top;
                return 0;
            }
            break;
        case WM_NCPAINT:
            pintarMarcoCampo(h);
            return 0;
        case WM_NCHITTEST: {
            // El borde también responde como parte del campo (para hacer clic y escribir).
            const LRESULT r = DefSubclassProc(h, msg, wp, lp);
            return r == HTNOWHERE || r == HTBORDER ? HTCLIENT : r;
        }
        case WM_NCLBUTTONDOWN:
            SetFocus(h);
            break;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_ENABLE:
        case EM_SETREADONLY: {
            const LRESULT r = DefSubclassProc(h, msg, wp, lp);
            RedrawWindow(h, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE);
            return r;
        }
        case WM_NCDESTROY:
            g_campos.erase(h);
            RemoveWindowSubclass(h, procCampo, 2);
            break;
        default:
            break;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

void prepararCampo(HWND c, int tipo) {
    if (g_campos.count(c)) {
        g_campos[c] = std::max(g_campos[c], tipo);
        return;
    }
    g_campos[c] = tipo;
    SetWindowLongW(c, GWL_STYLE, GetWindowLongW(c, GWL_STYLE) & ~WS_BORDER);
    SetWindowLongW(c, GWL_EXSTYLE, GetWindowLongW(c, GWL_EXSTYLE) & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    SetWindowSubclass(c, procCampo, 2, 0);
    SetWindowPos(c, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// ---- Listas desplegables (ComboBox de solo lista): campo redondeado con flecha.
LRESULT CALLBACK procCombo(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT r;
            GetClientRect(h, &r);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, std::max(1L, r.right), std::max(1L, r.bottom));
            HGDIOBJ a = SelectObject(mem, bmp);
            HWND padre = GetParent(h);
            if (!copiarFondo(mem, padre, h, r.right, r.bottom)) FillRect(mem, &r, pincel(fondoEn(padre, h)));
            const bool foco = GetFocus() == h || SendMessageW(h, CB_GETDROPPEDSTATE, 0, 0);
            const bool activo = IsWindowEnabled(h) != 0;
            rectRedondeado(mem, r, std::min(escalaPx(8), static_cast<int>(r.bottom) / 2),
                           activo ? (foco ? TARJETA : CAMPO) : RGB(243, 239, 232), foco ? CAFE_CLARO : BORDE, foco ? 2 : 1);
            std::wstring t;
            const LRESULT i = SendMessageW(h, CB_GETCURSEL, 0, 0);
            if (i != CB_ERR) {
                const LRESULT n = SendMessageW(h, CB_GETLBTEXTLEN, static_cast<WPARAM>(i), 0);
                if (n > 0) {
                    t.assign(static_cast<size_t>(n) + 1, L'\0');
                    SendMessageW(h, CB_GETLBTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&t[0]));
                    t.resize(static_cast<size_t>(n));
                }
            }
            HFONT f = reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0));
            HGDIOBJ fa = SelectObject(mem, f);
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, activo ? TEXTO : TEXTO_SUAVE);
            const int flecha = escalaPx(26);
            RECT rt{escalaPx(10), 0, r.right - flecha, r.bottom};
            DrawTextW(mem, t.c_str(), -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            RECT rf{r.right - flecha, 0, r.right - escalaPx(6), r.bottom};
            if (HFONT ic = fuenteIconos(std::max(9, static_cast<int>(r.bottom) * 38 / 100))) {
                SelectObject(mem, ic);
                SetTextColor(mem, CAFE_CLARO);
                DrawTextW(mem, L"\xE70D", 1, &rf, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                DrawTextW(mem, L"\x25BE", 1, &rf, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            SelectObject(mem, fa);
            BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, a);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_ENABLE:
        case CB_SETCURSEL: {
            const LRESULT r = DefSubclassProc(h, msg, wp, lp);
            InvalidateRect(h, nullptr, FALSE);
            return r;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(h, procCombo, 3);
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

void usarLogo(HWND dlg, bool si) { g_datos[dlg].conLogo = si; }

COLORREF fondoDe(HWND dlg) {
    auto it = g_datos.find(dlg);
    return it == g_datos.end() ? FONDO : it->second.fondo;
}

void olvidar(HWND dlg) {
    auto it = g_datos.find(dlg);
    if (it == g_datos.end()) return;
    liberarCache(it->second);
    g_datos.erase(it);
}

void campoBusqueda(HWND edit) { prepararCampo(edit, CAMPO_BUSQUEDA); }

void logoMarca(HDC dc, const RECT& area) { dibujarLogo(dc, area, OPACIDAD_LOGO); }

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
            } else if ((tipo == BS_PUSHBUTTON || tipo == BS_DEFPUSHBUTTON) && !g_botones.count(c)) {
                g_botones[c].tipo = tipoDeBoton(c, estilo);
                SetWindowLongW(c, GWL_STYLE, (estilo & ~BS_TYPEMASK) | BS_OWNERDRAW);
                SetWindowSubclass(c, procBoton, 1, 0);
                InvalidateRect(c, nullptr, TRUE);
            }
        } else if (_wcsicmp(clase, WC_LISTVIEWW) == 0) {
            SetWindowTheme(c, L"Explorer", nullptr);
            ListView_SetExtendedListViewStyleEx(c, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES,
                                                LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            SetWindowLongW(c, GWL_STYLE, estilo & ~WS_BORDER);
            SetWindowLongW(c, GWL_EXSTYLE, GetWindowLongW(c, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
            SetWindowPos(c, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            d.tablas.push_back(c);
        } else if (_wcsicmp(clase, L"Edit") == 0) {
            prepararCampo(c, CAMPO_TEXTO);
        } else if (_wcsicmp(clase, L"SysDateTimePick32") == 0) {
            prepararCampo(c, CAMPO_FECHA);
        } else if (_wcsicmp(clase, L"ComboBox") == 0 && (estilo & 3) == CBS_DROPDOWNLIST) {
            SetWindowSubclass(c, procCombo, 3, 0);
            InvalidateRect(c, nullptr, TRUE);
        }
    }
    refrescar(dlg);
}

void refrescar(HWND dlg) {
    auto it = g_datos.find(dlg);
    if (it == g_datos.end()) return;
    construirCache(dlg, it->second);
    fondoTablas(dlg, it->second);
    RedrawWindow(dlg, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

bool pintarFondo(HWND dlg, HDC dc) {
    auto it = g_datos.find(dlg);
    if (it == g_datos.end()) return false;
    Datos& d = it->second;
    RECT r;
    GetClientRect(dlg, &r);
    if (!d.cache || d.tam.cx != r.right || d.tam.cy != r.bottom) construirCache(dlg, d);
    if (!d.cache) {
        FillRect(dc, &r, pincel(d.fondo));
        return true;
    }
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ a = SelectObject(mem, d.cache);
    BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, a);
    DeleteDC(mem);
    return true;
}

INT_PTR colorControl(HWND dlg, UINT msg, HDC dc, HWND control, COLORREF colorTexto) {
    if (msg == WM_CTLCOLORDLG) return reinterpret_cast<INT_PTR>(pincel(fondoDe(dlg)));
    wchar_t clase[32] = {0};
    GetClassNameW(control, clase, 31);
    if (_wcsicmp(clase, L"Edit") == 0) {
        const COLORREF relleno = g_campos.count(control) ? rellenoCampo(control) : TARJETA;
        SetBkColor(dc, relleno);
        SetTextColor(dc, IsWindowEnabled(control) ? (msg == WM_CTLCOLOREDIT ? TEXTO : colorTexto) : TEXTO_SUAVE);
        return reinterpret_cast<INT_PTR>(pincel(relleno));
    }
    if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX) return 0;
    SetTextColor(dc, IsWindowEnabled(control) ? colorTexto : TEXTO_SUAVE);
    // Etiquetas y casillas: el fondo es el mismo dibujo de la ventana (con el logo), así parecen transparentes.
    auto it = g_datos.find(dlg);
    if (it != g_datos.end() && it->second.patron) {
        const RECT r = rectEn(dlg, control);
        SetBkMode(dc, TRANSPARENT);
        SetBrushOrgEx(dc, -r.left, -r.top, nullptr);
        return reinterpret_cast<INT_PTR>(it->second.patron);
    }
    const COLORREF fondo = fondoEn(dlg, control);
    SetBkColor(dc, fondo);
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
    HWND padre = GetParent(d->hwndItem);
    if (!copiarFondo(mem, padre, d->hwndItem, w, h)) FillRect(mem, &local, pincel(fondoEn(padre, d->hwndItem)));

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
    rectRedondeado(mem, rb, std::min(escalaPx(8), h / 2), relleno, borde, foco && !apagado && tipo != BOTON_PRINCIPAL ? 2 : 1);

    HFONT f = reinterpret_cast<HFONT>(SendMessageW(d->hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ fa = SelectObject(mem, f);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, letra);
    const std::wstring t = textoVentana(d->hwndItem);
    RECT rt{escalaPx(8), 0, w - escalaPx(8), h};
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
