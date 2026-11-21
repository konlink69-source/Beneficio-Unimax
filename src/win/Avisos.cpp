#include "win/Avisos.h"

#include <shellapi.h>

#include <cwchar>

#include "win/App.h"
#include "win/resource.h"

namespace bu::win {

namespace {

const wchar_t* const NOMBRE_TAREA = L"BeneficioUnimax - Recordatorio de deudas";
constexpr UINT WM_ICONO = WM_APP + 20;
constexpr UINT_PTR TEMPORIZADOR = 1;

// Ejecuta schtasks.exe sin mostrar ventana y devuelve su código de salida.
int ejecutarSchtasks(const std::wstring& argumentos) {
    std::wstring cmd = L"schtasks.exe " + argumentos;
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, 20000);
    DWORD codigo = 1;
    GetExitCodeProcess(pi.hProcess, &codigo);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(codigo);
}

std::wstring argumentoDatos() {
    const Rutas& r = App::instancia().rutas();
    return r.datosPersonalizados ? L" /datos \"" + r.datos + L"\"" : std::wstring();
}

void abrirDesdeAviso() { lanzarPrograma(L"/verrecordatorio" + argumentoDatos()); }

LRESULT CALLBACK procAviso(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ICONO:
            switch (LOWORD(lp)) {
                case NIN_BALLOONUSERCLICK:
                case NIN_SELECT:
                case NIN_KEYSELECT:
                case WM_LBUTTONDBLCLK:
                    abrirDesdeAviso();
                    DestroyWindow(h);
                    break;
                default:
                    break;
            }
            return 0;
        case WM_TIMER:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY: {
            NOTIFYICONDATAW nid{};
            nid.cbSize = sizeof nid;
            nid.hWnd = h;
            nid.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &nid);
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
    }
    (void)wp;
    return DefWindowProcW(h, msg, wp, lp);
}

void copiar(wchar_t* destino, size_t capacidad, const std::wstring& origen) {
    const size_t n = origen.size() < capacidad - 1 ? origen.size() : capacidad - 1;
    std::wmemcpy(destino, origen.c_str(), n);
    destino[n] = L'\0';
}

}  // namespace

bool horaValida(const std::string& h) {
    if (h.size() != 5 || h[2] != ':') return false;
    for (int i : {0, 1, 3, 4})
        if (h[static_cast<size_t>(i)] < '0' || h[static_cast<size_t>(i)] > '9') return false;
    const int hh = (h[0] - '0') * 10 + (h[1] - '0');
    const int mm = (h[3] - '0') * 10 + (h[4] - '0');
    return hh < 24 && mm < 60;
}

bool avisoWindowsActivo() { return ejecutarSchtasks(L"/Query /TN \"" + std::wstring(NOMBRE_TAREA) + L"\"") == 0; }

bool activarAvisoWindows(const std::string& hora, std::string& error) {
    if (!horaValida(hora)) {
        error = "La hora debe tener el formato HH:MM de 24 horas (por ejemplo 07:30 o 16:00).";
        return false;
    }
    const std::wstring exe = App::instancia().rutas().exe;
    std::wstring datos;
    if (App::instancia().rutas().datosPersonalizados)
        datos = L" /datos \\\"" + App::instancia().rutas().datos + L"\\\"";
    const std::wstring args = L"/Create /F /SC DAILY /ST " + W(hora) + L" /TN \"" + NOMBRE_TAREA + L"\" /TR \"\\\"" +
                              exe + L"\\\" /recordatorio" + datos + L"\"";
    const int codigo = ejecutarSchtasks(args);
    if (codigo != 0) {
        error = "Windows no permitió crear la tarea programada (código " + std::to_string(codigo) +
                "). Intente de nuevo o créela manualmente en el Programador de tareas con el programa:\n" + U(exe) +
                " /recordatorio";
        return false;
    }
    return true;
}

bool desactivarAvisoWindows(std::string& error) {
    if (!avisoWindowsActivo()) return true;
    const int codigo = ejecutarSchtasks(L"/Delete /F /TN \"" + std::wstring(NOMBRE_TAREA) + L"\"");
    if (codigo != 0) {
        error = "No se pudo eliminar la tarea programada (código " + std::to_string(codigo) + ").";
        return false;
    }
    return true;
}

void lanzarPrograma(const std::wstring& argumentos) {
    ShellExecuteW(nullptr, L"open", App::instancia().rutas().exe.c_str(), argumentos.c_str(), nullptr, SW_SHOWNORMAL);
}

int ejecutarModoAviso(HINSTANCE hInst, bool forzar) {
    Recordatorio r;
    try {
        r = repo().recordatorio(Fecha::hoy());
    } catch (...) {
        return 1;
    }
    if (!r.hayAlgo() && !forzar) return 0;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = procAviso;
    wc.hInstance = hInst;
    wc.lpszClassName = L"BeneficioUnimaxAviso";
    RegisterClassExW(&wc);
    HWND h = CreateWindowExW(0, wc.lpszClassName, L"Beneficio Unimax", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr,
                             hInst, nullptr);
    if (!h) return 1;

    std::string texto = r.hayAlgo() ? r.resumenCorto() : std::string("No hay deudas vencidas ni por vencer hoy.");
    texto += "\nTotal por pagar: " + formatoLempiras(r.totales.porPagar) +
             "  ·  por cobrar: " + formatoLempiras(r.totales.porCobrar) + "\nHaga clic aquí para ver el detalle.";

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof nid;
    nid.hWnd = h;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_INFO;
    nid.uCallbackMessage = WM_ICONO;
    nid.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    if (!nid.hIcon) nid.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32516));  // IDI_INFORMATION
    copiar(nid.szTip, ARRAYSIZE(nid.szTip), L"Beneficio Unimax: recordatorio de deudas");
    copiar(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), L"Beneficio Unimax: recordatorio del día");
    copiar(nid.szInfo, ARRAYSIZE(nid.szInfo), W(texto));
    nid.dwInfoFlags = r.hayDeudasUrgentes() ? NIIF_WARNING : NIIF_INFO;
    Shell_NotifyIconW(NIM_ADD, &nid);
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);

    // El icono queda en la bandeja 30 minutos por si el usuario quiere abrir el detalle.
    SetTimer(h, TEMPORIZADOR, 30 * 60 * 1000, nullptr);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

}  // namespace bu::win
