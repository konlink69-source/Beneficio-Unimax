#include "win/Tareas.h"

#include <mutex>
#include <thread>

#include "win/App.h"
#include "win/Tema.h"
#include "win/resource.h"

namespace bu::win {

namespace {

constexpr UINT WM_TAREA_AVANCE = WM_APP + 50;
constexpr UINT WM_TAREA_FIN = WM_APP + 51;

class AvanceVentana : public Avance {
public:
    HWND h = nullptr;
    std::atomic<bool> cancelar{false};
    std::mutex m;
    std::string textoActual;
    int porcentajeActual = -1;
    std::function<void(Avance&)> trabajo;
    std::string titulo;
    std::string error;
    bool ok = false;
    bool cancelable = true;

    void texto(const std::string& t) override {
        {
            std::lock_guard<std::mutex> l(m);
            textoActual = t;
        }
        if (h) PostMessageW(h, WM_TAREA_AVANCE, 0, 0);
    }
    void porcentaje(int p) override {
        {
            std::lock_guard<std::mutex> l(m);
            porcentajeActual = p;
        }
        if (h) PostMessageW(h, WM_TAREA_AVANCE, 0, 0);
    }
    bool cancelado() const override { return cancelar.load(); }
};

INT_PTR CALLBACK procProgreso(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* a = reinterpret_cast<AvanceVentana*>(GetWindowLongPtrW(h, DWLP_USER));
    switch (msg) {
        case WM_INITDIALOG: {
            a = reinterpret_cast<AvanceVentana*>(lp);
            SetWindowLongPtrW(h, DWLP_USER, lp);
            a->h = h;
            SetWindowTextW(h, W(a->titulo).c_str());
            SetDlgItemTextW(h, IDC_P_TEXTO, W(a->textoActual.empty() ? "Un momento..." : a->textoActual).c_str());
            HWND barra = GetDlgItem(h, IDC_P_BARRA);
            SetWindowLongPtrW(barra, GWL_STYLE, GetWindowLongPtrW(barra, GWL_STYLE) | PBS_MARQUEE);
            SendMessageW(barra, PBM_SETMARQUEE, TRUE, 30);
            ShowWindow(GetDlgItem(h, IDCANCEL), a->cancelable ? SW_SHOW : SW_HIDE);
            tema::usarFondo(h, tema::TARJETA);
            tema::preparar(h);
            std::thread([a] {
                try {
                    a->trabajo(*a);
                    a->ok = !a->cancelar.load();
                } catch (const std::exception& e) {
                    a->error = e.what();
                } catch (...) {
                    a->error = "Error desconocido.";
                }
                PostMessageW(a->h, WM_TAREA_FIN, 0, 0);
            }).detach();
            return TRUE;
        }
        case WM_TAREA_AVANCE: {
            std::string t;
            int p;
            {
                std::lock_guard<std::mutex> l(a->m);
                t = a->textoActual;
                p = a->porcentajeActual;
            }
            SetDlgItemTextW(h, IDC_P_TEXTO, W(t).c_str());
            HWND barra = GetDlgItem(h, IDC_P_BARRA);
            const bool marquesina = (GetWindowLongPtrW(barra, GWL_STYLE) & PBS_MARQUEE) != 0;
            if (p < 0 && !marquesina) {
                SetWindowLongPtrW(barra, GWL_STYLE, GetWindowLongPtrW(barra, GWL_STYLE) | PBS_MARQUEE);
                SendMessageW(barra, PBM_SETMARQUEE, TRUE, 30);
            } else if (p >= 0) {
                if (marquesina) {
                    SendMessageW(barra, PBM_SETMARQUEE, FALSE, 0);
                    SetWindowLongPtrW(barra, GWL_STYLE, GetWindowLongPtrW(barra, GWL_STYLE) & ~PBS_MARQUEE);
                    SendMessageW(barra, PBM_SETRANGE32, 0, 100);
                }
                SendMessageW(barra, PBM_SETPOS, static_cast<WPARAM>(p), 0);
            }
            return TRUE;
        }
        case WM_TAREA_FIN:
            tema::olvidar(h);
            EndDialog(h, IDOK);
            return TRUE;
        case WM_ERASEBKGND:
            if (tema::pintarFondo(h, reinterpret_cast<HDC>(wp))) {
                SetWindowLongPtrW(h, DWLP_MSGRESULT, TRUE);
                return TRUE;
            }
            return FALSE;
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
            return tema::colorControl(h, msg, reinterpret_cast<HDC>(wp), reinterpret_cast<HWND>(lp), tema::TEXTO);
        case WM_DRAWITEM:
            return tema::dibujarBoton(reinterpret_cast<const DRAWITEMSTRUCT*>(lp)) ? TRUE : FALSE;
        case WM_CLOSE:
            return TRUE;  // no se cierra con la X: se usa Cancelar
        default:
            return FALSE;
    }
}

INT_PTR CALLBACK procProgresoConCancelar(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_COMMAND && LOWORD(wp) == IDCANCEL) {
        auto* a = reinterpret_cast<AvanceVentana*>(GetWindowLongPtrW(h, DWLP_USER));
        if (a && a->cancelable && !a->cancelar.load()) {
            a->cancelar = true;
            SetDlgItemTextW(h, IDC_P_TEXTO, L"Cancelando...");
            EnableWindow(GetDlgItem(h, IDCANCEL), FALSE);
        }
        return TRUE;
    }
    return procProgreso(h, msg, wp, lp);
}

}  // namespace

bool ejecutarConProgreso(HWND padre, const std::string& titulo, const std::function<void(Avance&)>& trabajo,
                         std::string& error, bool cancelable) {
    AvanceVentana a;
    a.trabajo = trabajo;
    a.titulo = titulo;
    a.cancelable = cancelable;
    const std::vector<BYTE> t = plantillaDialogo(IDD_PROGRESO, tamanoLetra() > 0 ? tamanoLetra() : 9);
    if (t.empty()) {
        // Sin ventana: se ejecuta aquí mismo.
        try {
            trabajo(a);
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    DialogBoxIndirectParamW(App::instancia().hInst, reinterpret_cast<LPCDLGTEMPLATEW>(t.data()), padre,
                            procProgresoConCancelar, reinterpret_cast<LPARAM>(&a));
    error = a.error;
    return a.ok;
}

}  // namespace bu::win
