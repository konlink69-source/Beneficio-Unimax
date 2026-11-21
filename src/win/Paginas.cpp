#include "win/Paginas.h"

#include <algorithm>

#include "win/App.h"

namespace bu::win {

HWND Pagina::crear(HWND padre, int plantilla) {
    return CreateDialogParamW(App::instancia().hInst, MAKEINTRESOURCEW(plantilla), padre, proc,
                              reinterpret_cast<LPARAM>(this));
}

void Pagina::colorTexto(int id, COLORREF color) {
    coloresTexto_[id] = color;
    InvalidateRect(GetDlgItem(h_, id), nullptr, TRUE);
}

void Pagina::redimensionar(int ancho, int alto) {
    if (!controlEstirable_) return;
    HWND c = GetDlgItem(h_, controlEstirable_);
    if (!c) return;
    RECT r;
    GetWindowRect(c, &r);
    MapWindowPoints(nullptr, h_, reinterpret_cast<POINT*>(&r), 2);
    const int margen = static_cast<int>(r.left);
    const int izquierda = static_cast<int>(r.left);
    const int arriba = static_cast<int>(r.top);
    const int w = std::max(200, ancho - izquierda - margen);
    const int h = std::max(80, alto - arriba - margen);
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
            return FALSE;
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
            if (self) self->redimensionar(LOWORD(lp), HIWORD(lp));
            return TRUE;
        case WM_CTLCOLORSTATIC: {
            if (!self) return FALSE;
            const auto it = self->coloresTexto_.find(GetDlgCtrlID(reinterpret_cast<HWND>(lp)));
            if (it == self->coloresTexto_.end()) return FALSE;
            HDC dc = reinterpret_cast<HDC>(wp);
            SetTextColor(dc, it->second);
            SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        default:
            return FALSE;
    }
}

}  // namespace bu::win
