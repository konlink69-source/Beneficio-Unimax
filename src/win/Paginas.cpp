#include "win/Paginas.h"

#include <algorithm>

#include "win/App.h"

namespace bu::win {

HWND Pagina::crear(HWND padre, int plantilla) {
    // La plantilla se usa con el tamaño de letra elegido para la pantalla (todo crece con la letra).
    const std::vector<BYTE> t = plantillaDialogo(plantilla, tamanoLetra());
    if (t.empty()) return nullptr;
    return CreateDialogIndirectParamW(App::instancia().hInst, reinterpret_cast<LPCDLGTEMPLATEW>(t.data()), padre,
                                      proc, reinterpret_cast<LPARAM>(this));
}

void Pagina::aplicarMarcaAgua(int marca) {
    if (!controlEstirable_) return;
    HWND lista = GetDlgItem(h_, controlEstirable_);
    wchar_t clase[32] = {0};
    GetClassNameW(lista, clase, 31);
    if (std::wstring(clase) != WC_LISTVIEWW) return;
    LVBKIMAGEW bk{};
    HBITMAP imagen = marca == 0 ? nullptr : App::instancia().marcaAguaParaLista(marca);
    if (imagen) {
        bk.ulFlags = LVBKIF_TYPE_WATERMARK;
        bk.hbm = imagen;  // la tabla se queda con la imagen
    } else {
        bk.ulFlags = LVBKIF_SOURCE_NONE;
    }
    SendMessageW(lista, LVM_SETBKIMAGEW, 0, reinterpret_cast<LPARAM>(&bk));
    ListView_SetTextBkColor(lista, CLR_NONE);
    InvalidateRect(lista, nullptr, TRUE);
}

void Pagina::colorTexto(int id, COLORREF color) {
    coloresTexto_[id] = color;
    InvalidateRect(GetDlgItem(h_, id), nullptr, TRUE);
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
            intentar(h, [&] { self->aplicarMarcaAgua(App::instancia().marcaAgua()); });
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
            if (self) self->alRedimensionar(LOWORD(lp), HIWORD(lp));
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
