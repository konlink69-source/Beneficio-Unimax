#include "win/Paginas.h"

#include <algorithm>
#include <cmath>

#include "win/App.h"
#include "win/Tema.h"

namespace bu::win {

HWND Pagina::crear(HWND padre, int plantilla) {
    // La plantilla se usa con el tamaño de letra elegido para la pantalla (todo crece con la letra).
    const std::vector<BYTE> t = plantillaDialogo(plantilla, tamanoLetra());
    if (t.empty()) return nullptr;
    return CreateDialogIndirectParamW(App::instancia().hInst, reinterpret_cast<LPCDLGTEMPLATEW>(t.data()), padre,
                                      proc, reinterpret_cast<LPARAM>(this));
}

void Pagina::aplicarMarcaAgua(int marca) {
    // El logo del beneficio, tenue, de fondo en toda la pestaña (también detrás de las tablas).
    tema::usarLogo(h_, marca != 0);
    if (h_) tema::refrescar(h_);
}

void Pagina::colorTexto(int id, COLORREF color) {
    coloresTexto_[id] = color;
    InvalidateRect(GetDlgItem(h_, id), nullptr, TRUE);
}

namespace {
HFONT letraEscalada(LOGFONTW lf, double escala) {
    // Las letras se crean una vez por tamaño y se reutilizan.
    static std::vector<std::pair<std::wstring, HFONT>> cache;
    lf.lfHeight = static_cast<LONG>(lf.lfHeight * escala + (lf.lfHeight < 0 ? -0.5 : 0.5));
    if (lf.lfHeight == 0) lf.lfHeight = -1;
    const std::wstring clave = std::wstring(lf.lfFaceName) + L"|" + std::to_wstring(lf.lfHeight) + L"|" +
                               std::to_wstring(lf.lfWeight) + L"|" + std::to_wstring(lf.lfItalic);
    for (const auto& c : cache)
        if (c.first == clave) return c.second;
    HFONT f = CreateFontIndirectW(&lf);
    cache.push_back({clave, f});
    return f;
}

}  // namespace

void Pagina::guardarDiseno() {
    RECT rc;
    GetClientRect(h_, &rc);
    tamanoDiseno_ = {rc.right, rc.bottom};
    originales_.clear();
    for (HWND c = GetWindow(h_, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        Original o{};
        o.control = c;
        GetWindowRect(c, &o.rect);
        MapWindowPoints(nullptr, h_, reinterpret_cast<POINT*>(&o.rect), 2);
        wchar_t clase[32] = {0};
        GetClassNameW(c, clase, 31);
        o.combo = _wcsicmp(clase, L"ComboBox") == 0;
        o.tabla = _wcsicmp(clase, WC_LISTVIEWW) == 0;
        o.marco = _wcsicmp(clase, L"Button") == 0 && (GetWindowLongW(c, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX;
        HFONT f = reinterpret_cast<HFONT>(SendMessageW(c, WM_GETFONT, 0, 0));
        o.tieneLetra = f && GetObjectW(f, sizeof o.letra, &o.letra) > 0;
        // Botones en negrita para que se vean mejor.
        if (o.tieneLetra && _wcsicmp(clase, L"Button") == 0) {
            const LONG estilo = GetWindowLongW(c, GWL_STYLE) & BS_TYPEMASK;
            if (estilo == BS_PUSHBUTTON || estilo == BS_DEFPUSHBUTTON) {
                o.letra.lfWeight = FW_BOLD;
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(letraEscalada(o.letra, 1.0)), TRUE);
            }
        }
        originales_.push_back(o);
    }
}

void Pagina::escalar(int ancho, int alto) {
    if (tamanoDiseno_.cx <= 0 || tamanoDiseno_.cy <= 0 || originales_.empty()) return;
    // Cada dirección se ajusta por separado para llenar la ventana; las letras crecen con la menor.
    double ex = static_cast<double>(ancho) / tamanoDiseno_.cx;
    double ey = static_cast<double>(alto) / tamanoDiseno_.cy;
    double e = std::max(0.85, std::min(std::min(ex, ey), 2.2));
    ex = std::max(0.85, std::min(ex, e * 1.9));
    ey = std::max(0.85, std::min(ey, e * 1.6));
    if (std::abs(ex - escalaX_) < 0.005 && std::abs(ey - escalaY_) < 0.005 && std::abs(e - escalaActual_) < 0.005) return;
    const double cambioX = ex / escalaX_;
    HDWP posiciones = BeginDeferWindowPos(static_cast<int>(originales_.size()));
    for (const auto& o : originales_) {
        const int x = static_cast<int>(o.rect.left * ex), y = static_cast<int>(o.rect.top * ey);
        const int w = static_cast<int>((o.rect.right - o.rect.left) * ex);
        // Tablas y marcos crecen con el alto de la ventana; los campos y botones, con la letra.
        const int altoOriginal = o.rect.bottom - o.rect.top;
        const bool crece = o.tabla || o.marco || altoOriginal > 60;
        int h = static_cast<int>(altoOriginal * (crece ? ey : e));
        if (o.combo) h += static_cast<int>(200 * e);  // alto de la lista desplegable
        if (posiciones)
            posiciones = DeferWindowPos(posiciones, o.control, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        if (o.tieneLetra) SendMessageW(o.control, WM_SETFONT, reinterpret_cast<WPARAM>(letraEscalada(o.letra, e)), FALSE);
        // Columnas de las tablas en proporción.
        if (o.tabla) {
            HWND encabezado = ListView_GetHeader(o.control);
            const int columnas = encabezado ? Header_GetItemCount(encabezado) : 0;
            for (int i = 0; i < columnas; ++i)
                ListView_SetColumnWidth(o.control, i, static_cast<int>(ListView_GetColumnWidth(o.control, i) * cambioX));
        }
    }
    if (posiciones) EndDeferWindowPos(posiciones);
    escalaActual_ = e;
    escalaX_ = ex;
    escalaY_ = ey;
    InvalidateRect(h_, nullptr, TRUE);
}

void Pagina::alRedimensionar(int ancho, int alto) {
    if (controlEstirable_) estirar(controlEstirable_, ancho, alto, true, true);
}

void Pagina::estirar(int id, int ancho, int alto, bool horizontal, bool vertical) {
    HWND c = GetDlgItem(h_, id);
    if (!c) return;
    RECT r;
    GetWindowRect(c, &r);
    MapWindowPoints(nullptr, h_, reinterpret_cast<POINT*>(&r), 2);
    const int margen = static_cast<int>(r.left);
    const int izquierda = static_cast<int>(r.left);
    const int arriba = static_cast<int>(r.top);
    const int w = horizontal ? std::max(120, ancho - izquierda - margen) : static_cast<int>(r.right - r.left);
    const int h = vertical ? std::max(80, alto - arriba - margen) : static_cast<int>(r.bottom - r.top);
    SetWindowPos(c, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

INT_PTR CALLBACK Pagina::proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<Pagina*>(GetWindowLongPtrW(h, DWLP_USER));
    switch (msg) {
        case WM_INITDIALOG:
            self = reinterpret_cast<Pagina*>(lp);
            SetWindowLongPtrW(h, DWLP_USER, lp);
            self->h_ = h;
            intentar(h, [&] { self->alIniciar(); });
            self->guardarDiseno();
            tema::usarLogo(h, App::instancia().marcaAgua() != 0);
            tema::preparar(h);
            return FALSE;
        case WM_ERASEBKGND:
            if (tema::pintarFondo(h, reinterpret_cast<HDC>(wp))) {
                SetWindowLongPtrW(h, DWLP_MSGRESULT, TRUE);
                return TRUE;
            }
            return FALSE;
        case WM_PAINT: {
            if (!self) return FALSE;
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT r;
            GetClientRect(h, &r);
            // Doble búfer: el dibujo propio se hace en memoria y se copia de una vez.
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, std::max(1L, r.right), std::max(1L, r.bottom));
            HGDIOBJ anterior = SelectObject(mem, bmp);
            FillRect(mem, &r, tema::pincel(tema::fondoDe(h)));
            bool dibujo = false;
            intentar(h, [&] { dibujo = self->alPintar(mem, r); });
            if (dibujo) BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, anterior);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return TRUE;
        }
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONUP:
            if (self) {
                bool atendido = false;
                intentar(h, [&] {
                    atendido = self->alRaton(msg, static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
                });
                return atendido ? TRUE : FALSE;
            }
            return FALSE;
        case WM_SETCURSOR:
            if (self && reinterpret_cast<HWND>(wp) == h && LOWORD(lp) == HTCLIENT) {
                POINT p;
                GetCursorPos(&p);
                ScreenToClient(h, &p);
                if (self->alRaton(WM_SETCURSOR, p.x, p.y)) {
                    SetWindowLongPtrW(h, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
            }
            return FALSE;
        case WM_DRAWITEM:
            return tema::dibujarBoton(reinterpret_cast<const DRAWITEMSTRUCT*>(lp)) ? TRUE : FALSE;
        case WM_COMMAND: {
            if (!self) return FALSE;
            const int id = LOWORD(wp);
            if (id == IDOK) {
                intentar(h, [&] { self->alAceptar(); });
                return TRUE;
            }
            if (id == IDCANCEL) return TRUE;  // Esc no cierra la pestaña
            bool atendido = false;
            intentar(h, [&] { atendido = self->alComando(id, HIWORD(wp)); });
            return atendido ? TRUE : FALSE;
        }
        case WM_NOTIFY: {
            if (!self) return FALSE;
            LRESULT r = 0;
            bool atendido = false;
            intentar(h, [&] { atendido = self->alNotificar(reinterpret_cast<NMHDR*>(lp), r); });
            if (atendido) {
                SetWindowLongPtrW(h, DWLP_MSGRESULT, r);
                return TRUE;
            }
            return FALSE;
        }
        case WM_SIZE:
            if (self) {
                self->escalar(LOWORD(lp), HIWORD(lp));
                self->alRedimensionar(LOWORD(lp), HIWORD(lp));
                tema::refrescar(h);  // fondo, tarjetas y logo del nuevo tamaño
            }
            return TRUE;
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            if (!self) return FALSE;
            const auto it = self->coloresTexto_.find(GetDlgCtrlID(reinterpret_cast<HWND>(lp)));
            const COLORREF color = it == self->coloresTexto_.end() ? tema::TEXTO : it->second;
            return tema::colorControl(h, msg, reinterpret_cast<HDC>(wp), reinterpret_cast<HWND>(lp), color);
        }
        default:
            return FALSE;
    }
}

}  // namespace bu::win
