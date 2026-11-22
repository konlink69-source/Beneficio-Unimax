// Beneficio Unimax: contabilidad de compra de café (uva, pergamino húmedo y seco).
//
// Argumentos:
//   /datos "carpeta"    usa otra carpeta de datos (por defecto Documentos\BeneficioUnimax)
//   /recordatorio       solo muestra el aviso de deudas (lo usa la tarea programada de Windows)
//   /probar             junto con /recordatorio: muestra el aviso aunque no haya pendientes
//   /verrecordatorio    abre el programa mostrando el recordatorio del día
//   /noinstalar         no pregunta si se desea instalar (por ejemplo, al probarlo desde Visual Studio)
#include "win/App.h"

#include <shellapi.h>

#include <array>
#include <memory>

#include "win/Avisos.h"
#include "win/Instalacion.h"
#include "win/Paginas.h"
#include "win/resource.h"

using namespace bu;
using namespace bu::win;

namespace {

struct Ventana {
    HWND pestanas = nullptr;
    std::array<std::unique_ptr<Pagina>, NUM_PAGINAS> paginas;
    int actual = -1;
    HBITMAP logo = nullptr;
    int logoAncho = 0;
    int logoAlto = 0;
};

Ventana g;

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

int altoEncabezado() { return escala(64); }

std::string fechaLarga(const Fecha& f) {
    static const char* const DIAS[] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
    const long n = f.numeroDia();  // el 1970-01-01 fue jueves
    return std::string(DIAS[((n % 7) + 7 + 4) % 7]) + " " + f.texto();
}

// Encabezado con el logo, en los colores de la fachada: crema con una viga café abajo.
void pintarEncabezado(HWND h, HDC dc) {
    RECT r;
    GetClientRect(h, &r);
    const int alto = altoEncabezado();
    const int viga = escala(5);
    RECT banda{0, 0, r.right, alto - viga};
    HBRUSH crema = CreateSolidBrush(RGB(241, 234, 219));
    FillRect(dc, &banda, crema);
    DeleteObject(crema);
    RECT rv{0, alto - viga, r.right, alto};
    HBRUSH cafe = CreateSolidBrush(COLOR_CAFE);
    FillRect(dc, &rv, cafe);
    DeleteObject(cafe);

    int x = escala(10);
    if (g.logo && g.logoAlto > 0) {
        const int la = alto - viga - escala(8);
        const int an = MulDiv(g.logoAncho, la, g.logoAlto);
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ anterior = SelectObject(mem, g.logo);
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchBlt(dc, x, escala(4), an, la, mem, 0, 0, g.logoAncho, g.logoAlto, SRCCOPY);
        SelectObject(mem, anterior);
        DeleteDC(mem);
        x += an;
    }
    SetBkMode(dc, TRANSPARENT);
    const int medio = (alto - viga) / 2 + escala(3);
    RECT rt{x + escala(16), escala(4), r.right - escala(16), medio};
    HGDIOBJ fuenteAnterior = SelectObject(dc, fuenteGrande());
    SetTextColor(dc, COLOR_CAFE);
    DrawTextW(dc, L"Contabilidad de compra de café", -1, &rt, DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT rf{rt.left, medio, rt.right, alto - viga};
    SelectObject(dc, fuenteNormal());
    SetTextColor(dc, COLOR_GRIS);
    DrawTextW(dc, W("Hoy es " + fechaLarga(Fecha::hoy())).c_str(), -1, &rf, DT_RIGHT | DT_TOP | DT_SINGLELINE);
    SelectObject(dc, fuenteAnterior);
}

HWND paginaActiva() { return g.actual >= 0 ? g.paginas[static_cast<size_t>(g.actual)]->hwnd() : nullptr; }

void ubicarPaginas(HWND h) {
    RECT r;
    GetClientRect(h, &r);
    const int margen = 4;
    const int arriba = altoEncabezado() + margen;
    const int ancho = r.right - 2 * margen;
    const int alto = r.bottom - arriba - margen;
    SetWindowPos(g.pestanas, nullptr, margen, arriba, ancho, alto, SWP_NOZORDER);
    RECT area{0, 0, ancho, alto};
    TabCtrl_AdjustRect(g.pestanas, FALSE, &area);
    for (auto& p : g.paginas)
        SetWindowPos(p->hwnd(), HWND_TOP, area.left + margen, area.top + arriba, area.right - area.left,
                     area.bottom - area.top, SWP_NOACTIVATE);
    RECT encabezado{0, 0, r.right, altoEncabezado()};
    InvalidateRect(h, &encabezado, FALSE);
}

void mostrarPagina(int indice) {
    if (indice < 0 || indice >= NUM_PAGINAS) return;
    TabCtrl_SetCurSel(g.pestanas, indice);
    for (int i = 0; i < NUM_PAGINAS; ++i)
        ShowWindow(g.paginas[static_cast<size_t>(i)]->hwnd(), i == indice ? SW_SHOW : SW_HIDE);
    g.actual = indice;
    Pagina* p = g.paginas[static_cast<size_t>(indice)].get();
    SetWindowPos(p->hwnd(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    intentar(p->hwnd(), [&] { p->alMostrar(); });
}

bool crearContenido(HWND h) {
    g.pestanas = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP, 0, 0, 100,
                                 100, h, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PESTANAS)),
                                 App::instancia().hInst, nullptr);
    SendMessageW(g.pestanas, WM_SETFONT, reinterpret_cast<WPARAM>(fuenteNegrita()), TRUE);
    const std::pair<int, std::unique_ptr<Pagina> (*)()> definiciones[NUM_PAGINAS] = {
        {IDD_PAG_FACTURA, crearPaginaFactura},     {IDD_PAG_HISTORIAL, crearPaginaHistorial},
        {IDD_PAG_CLIENTES, crearPaginaClientes},   {IDD_PAG_PRECIOS, crearPaginaPrecios},
        {IDD_PAG_DEUDAS, crearPaginaDeudas},       {IDD_PAG_CAJA, crearPaginaCaja},
        {IDD_PAG_VENTAS, crearPaginaVentas},       {IDD_PAG_REPORTES, crearPaginaReportes},
        {IDD_PAG_CONFIG, crearPaginaConfiguracion}};
    for (int i = 0; i < NUM_PAGINAS; ++i) {
        auto pagina = definiciones[i].second();
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(pagina->titulo());
        SendMessageW(g.pestanas, TCM_INSERTITEMW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&item));
        if (!pagina->crear(h, definiciones[i].first)) return false;
        g.paginas[static_cast<size_t>(i)] = std::move(pagina);
    }
    return true;
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
    mostrarInfo(h, "Beneficio Unimax 1.0\nContabilidad de compra y venta de café (uva, pergamino húmedo y seco).\n\n"
                   "Base de datos (Access): " + U(r.baseDatos) + "\nPlantillas de Excel: " + U(r.plantillas) +
                   "\nDocumentos generados: " + U(r.documentos) + "\n\n1 quintal = 100 libras. Moneda: Lempiras (L).");
}

LRESULT CALLBACK procVentana(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            App::instancia().ventana = h;
            g.logo = static_cast<HBITMAP>(LoadImageW(App::instancia().hInst, MAKEINTRESOURCEW(IDB_LOGO), IMAGE_BITMAP,
                                                     0, 0, LR_CREATEDIBSECTION));
            if (g.logo) {
                BITMAP bm{};
                GetObjectW(g.logo, sizeof bm, &bm);
                g.logoAncho = bm.bmWidth;
                g.logoAlto = bm.bmHeight;
            }
            if (!crearContenido(h)) {
                MessageBoxW(h, L"No se pudieron crear las ventanas del programa.", L"Beneficio Unimax", MB_ICONERROR);
                return -1;
            }
            ubicarPaginas(h);
            mostrarPagina(PAG_FACTURA);
            return 0;
        case WM_SIZE:
            if (g.pestanas) ubicarPaginas(h);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            pintarEncabezado(h, dc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
            mm->ptMinTrackSize.x = 900;
            mm->ptMinTrackSize.y = 600;
            return 0;
        }
        case WM_NOTIFY: {
            const auto* nm = reinterpret_cast<NMHDR*>(lp);
            if (nm->hwndFrom == g.pestanas && nm->code == TCN_SELCHANGE) mostrarPagina(TabCtrl_GetCurSel(g.pestanas));
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
                case IDM_RECORDATORIO:
                    App::instancia().mostrarRecordatorio(h, true);
                    return 0;
                case IDM_ACERCA:
                    acercaDe(h);
                    return 0;
                case IDM_SALIR:
                    DestroyWindow(h);
                    return 0;
                default:
                    break;
            }
            break;
        case WM_APP_INICIO:
            if (lp != 0) intentar(h, [&] { despuesDeInstalar(h); });
            App::instancia().mostrarRecordatorio(h, wp != 0);
            if (g.actual >= 0) mostrarPagina(g.actual);  // refresca por si cambió algo
            return 0;
        case WM_APP_RECORDATORIO:
            App::instancia().mostrarRecordatorio(h, true);
            return 0;
        case WM_APP_IR_PAGINA:
            mostrarPagina(static_cast<int>(wp));
            return 0;
        case WM_DESTROY:
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
    std::wstring carpetaDatos;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (_wcsicmp(a.c_str(), L"/recordatorio") == 0) modoAviso = true;
        else if (_wcsicmp(a.c_str(), L"/probar") == 0) probar = true;
        else if (_wcsicmp(a.c_str(), L"/verrecordatorio") == 0) verRecordatorio = true;
        else if (_wcsicmp(a.c_str(), L"/instalado") == 0) recienInstalado = true;
        else if (_wcsicmp(a.c_str(), L"/noinstalar") == 0) noInstalar = true;
        else if (_wcsicmp(a.c_str(), L"/datos") == 0 && i + 1 < argc) carpetaDatos = argv[++i];
    }
    LocalFree(argv);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof icc;
    icc.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    App& app = App::instancia();
    app.hInst = hInst;

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
        if (!noInstalar && !recienInstalado && !ejecutandoDesdeInstalacion(exe) && !noPreguntarInstalacion()) {
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
    if (!app.iniciar(carpetaDatos, error)) {
        if (!modoAviso) MessageBoxW(nullptr, W(error).c_str(), L"Beneficio Unimax", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
    if (modoAviso) {
        const int r = ejecutarModoAviso(hInst, probar);
        CoUninitialize();
        return r;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = procVentana;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MENU);
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
    PostMessageW(h, WM_APP_INICIO, verRecordatorio ? 1 : 0, recienInstalado ? 1 : 0);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        HWND activa = paginaActiva();
        if (activa && IsDialogMessageW(activa, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
