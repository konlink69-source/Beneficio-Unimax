#include "win/App.h"

#include <commdlg.h>

#include <cstdlib>
#include <knownfolders.h>
#include <shlobj.h>

#include "db/Esquema.h"
#include "win/Dialogos.h"
#include "win/Nube.h"
#include "win/resource.h"

namespace bu::win {

const wchar_t* const CLASE_VENTANA = L"BeneficioUnimaxVentanaPrincipal";

App& App::instancia() {
    static App app;
    return app;
}

namespace {
std::wstring carpetaDocumentos() {
    PWSTR p = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p))) r = p;
    if (p) CoTaskMemFree(p);
    if (r.empty()) {
        wchar_t buf[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, buf);
        r = buf;
    }
    return r;
}

// Copia a la carpeta de datos las plantillas que todavía no estén allí,
// para que el usuario pueda personalizarlas sin tocar las originales.
void copiarPlantillas(const std::wstring& origen, const std::wstring& destino) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((origen + L"\\*.xlsx").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::wstring a = destino + L"\\" + fd.cFileName;
        if (!existeArchivo(a)) {
            CopyFileW((origen + L"\\" + fd.cFileName).c_str(), a.c_str(), TRUE);
            SetFileAttributesW(a.c_str(), FILE_ATTRIBUTE_NORMAL);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
// Versión de las plantillas incluidas en el programa. Al subirla, las plantillas que el usuario
// tenga se guardan en "Plantillas\Anteriores" y se ponen las nuevas.
//   1 = primeras plantillas    2 = con logo (fachada)    3 = logo de texto con granos de café
constexpr int VERSION_PLANTILLAS = 3;

void actualizarPlantillasSiHaceFalta(const std::wstring& carpeta) {
    const std::wstring marca = carpeta + L"\\version.txt";
    int version = 0;
    HANDLE f = CreateFileW(marca.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        char buf[16] = {0};
        DWORD leidos = 0;
        ReadFile(f, buf, sizeof buf - 1, &leidos, nullptr);
        CloseHandle(f);
        version = std::atoi(buf);
    }
    if (version >= VERSION_PLANTILLAS) return;

    // Las plantillas que había se conservan en "Anteriores\Versión N (fecha)" y se pueden seguir
    // usando desde Configuración → Plantillas de Excel.
    const std::wstring anteriores = carpeta + L"\\Anteriores\\Versión " + std::to_wstring(version) + L" (" +
                                    W(Fecha::hoy().iso()) + L")";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((carpeta + L"\\*.xlsx").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        crearCarpetas(anteriores);
        do {
            MoveFileExW((carpeta + L"\\" + fd.cFileName).c_str(), (anteriores + L"\\" + fd.cFileName).c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    f = CreateFileW(marca.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        const std::string texto = std::to_string(VERSION_PLANTILLAS);
        DWORD escritos = 0;
        WriteFile(f, texto.data(), static_cast<DWORD>(texto.size()), &escritos, nullptr);
        CloseHandle(f);
    }
}

// Escribe las plantillas incluidas dentro del .exe que todavía no estén en la carpeta.
void extraerPlantillasIncluidas(const std::wstring& destino) {
    struct Incluida {
        int id;
        const wchar_t* archivo;
    };
    static const Incluida plantillas[] = {
        {IDR_PLT_FACTURA, L"Factura.xlsx"},         {IDR_PLT_CIERRE_CAJA, L"CierreCaja.xlsx"},
        {IDR_PLT_ESTADO_CUENTA, L"EstadoCuenta.xlsx"}, {IDR_PLT_DEUDAS, L"ReporteDeudas.xlsx"},
        {IDR_PLT_COMPRAS, L"ReporteCompras.xlsx"},  {IDR_PLT_VENTAS, L"ReporteVentas.xlsx"},
        {IDR_PLT_CAJA, L"ReporteCaja.xlsx"},
        {IDR_MARCA_PNG1, L"marca_agua_1.png"},    {IDR_MARCA_PNG2, L"marca_agua_2.png"},
    };
    HMODULE modulo = GetModuleHandleW(nullptr);
    for (const auto& p : plantillas) {
        const std::wstring ruta = destino + L"\\" + p.archivo;
        if (existeArchivo(ruta)) continue;
        HRSRC recurso = FindResourceW(modulo, MAKEINTRESOURCEW(p.id), MAKEINTRESOURCEW(10));  // RT_RCDATA
        if (!recurso) continue;
        HGLOBAL cargado = LoadResource(modulo, recurso);
        const void* datos = cargado ? LockResource(cargado) : nullptr;
        const DWORD tamano = SizeofResource(modulo, recurso);
        if (!datos || !tamano) continue;
        HANDLE f = CreateFileW(ruta.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) continue;
        DWORD escritos = 0;
        const BOOL ok = WriteFile(f, datos, tamano, &escritos, nullptr);
        CloseHandle(f);
        if (!ok || escritos != tamano) DeleteFileW(ruta.c_str());
    }
}
}  // namespace

bool App::iniciar(const std::wstring& carpetaDatos, std::string& error, bool sincronizar) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    rutas_.exe = exe;
    rutas_.carpetaExe = carpetaDe(rutas_.exe);
    rutas_.datosPersonalizados = !carpetaDatos.empty();
    rutas_.datos = carpetaDatos.empty() ? carpetaDocumentos() + L"\\BeneficioUnimax" : carpetaDatos;
    rutas_.baseDatos = rutas_.datos + L"\\BeneficioUnimax.accdb";
    rutas_.plantillas = rutas_.datos + L"\\Plantillas";
    rutas_.documentos = rutas_.datos + L"\\Documentos";
    rutas_.respaldos = rutas_.datos + L"\\Respaldos";
    crearCarpetas(rutas_.datos);
    crearCarpetas(rutas_.plantillas);
    crearCarpetas(rutas_.documentos);
    crearCarpetas(rutas_.respaldos);
    actualizarPlantillasSiHaceFalta(rutas_.plantillas);
    copiarPlantillas(rutas_.carpetaExe + L"\\plantillas", rutas_.plantillas);
    extraerPlantillasIncluidas(rutas_.plantillas);

    baseAbierta_ = rutas_.baseDatos;
    // Google Drive: trae los datos más nuevos o abre la copia de consulta.
    if (sincronizar) Nube::instancia().alIniciar(baseAbierta_);

    if (!existeArchivo(baseAbierta_)) {
        const std::string e = db::crearArchivoAccess(baseAbierta_);
        if (!e.empty()) {
            error = "No se pudo crear la base de datos de Access:\n" + U(baseAbierta_) + "\n\nDetalle: " + e +
                    "\n\nSolución: instale el \"Microsoft Access Database Engine 2016 Redistributable\" de 64 bits "
                    "(o compile el programa con la misma cantidad de bits que su Office).";
            return false;
        }
    }
    try {
        abrirBaseDatos();
    } catch (const std::exception& e) {
        error = std::string("No se pudo abrir la base de datos:\n") + U(baseAbierta_) + "\n\nDetalle: " + e.what() +
                "\n\nSi el detalle menciona el controlador (driver), instale el \"Microsoft Access Database Engine "
                "2016 Redistributable\" de 64 bits.";
        return false;
    }
    return true;
}

void App::abrirBaseDatos() {
    conexion_.soloLectura = false;
    conexion_.alModificar = nullptr;
    conexion_.abrir(db::cadenaConexionAccess(U(baseAbierta_)));
    db::crearEsquema(conexion_, db::Dialecto::Access);
    repo_ = std::make_unique<Repositorio>(conexion_, db::Dialecto::Access);
    repo_->asegurarUsuarioMaestro(bytesAleatorios(16));
    if (usuario_.id) repo_->usarUsuario(usuario_.usuario);
    // Cada cambio se anota para subirlo a Google Drive.
    conexion_.alModificar = [] { Nube::instancia().marcarCambio(); };
    if (usuario_.id) aplicarSoloConsulta();
}

void App::aplicarSoloConsulta() {
    if (!Nube::instancia().soloConsulta()) return;
    conexion_.soloLectura = true;
    conexion_.mensajeSoloLectura = "Este equipo está en modo SOLO CONSULTA: " + Nube::instancia().motivoSoloConsulta() +
                                   "\n\nPuede ver los datos, pero para registrar o cambiar algo cierre el programa en "
                                   "la otra computadora y vuelva a abrirlo aquí.";
}

void App::establecerUsuario(const Usuario& u) {
    usuario_ = u;
    if (repo_) repo_->usarUsuario(u.usuario);
    // En SOLO CONSULTA se bloquean los cambios después de ingresar.
    aplicarSoloConsulta();
}

bool App::reemplazarBase(const std::wstring& nuevo, const std::wstring& copiaAnterior, std::string& error) {
    conexion_.cerrar();
    bool ok = MoveFileExW(baseAbierta_.c_str(), copiaAnterior.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) != 0;
    if (!ok) error = "No se pudo apartar la base de datos actual (código " + std::to_string(GetLastError()) + ").";
    if (ok && !MoveFileExW(nuevo.c_str(), baseAbierta_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        error = "No se pudo poner la base de datos nueva (código " + std::to_string(GetLastError()) + ").";
        MoveFileExW(copiaAnterior.c_str(), baseAbierta_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
        ok = false;
    }
    try {
        abrirBaseDatos();
    } catch (const std::exception& e) {
        error += std::string(" No se pudo abrir la base de datos: ") + e.what();
        ok = false;
    }
    return ok;
}

bool App::instantanea(const std::wstring& destino) {
    // Solo se cierra y se vuelve a abrir la conexión: el repositorio sigue siendo el mismo objeto
    // (puede haber funciones esperando en una ventana que lo usan).
    const bool soloLectura = conexion_.soloLectura;
    const std::string mensaje = conexion_.mensajeSoloLectura;
    auto aviso = conexion_.alModificar;
    conexion_.cerrar();
    const BOOL ok = CopyFileW(baseAbierta_.c_str(), destino.c_str(), FALSE);
    try {
        conexion_.abrir(db::cadenaConexionAccess(U(baseAbierta_)));
        conexion_.soloLectura = soloLectura;
        conexion_.mensajeSoloLectura = mensaje;
        conexion_.alModificar = aviso;
    } catch (const std::exception& e) {
        mostrarError(ventana, std::string("No se pudo volver a abrir la base de datos: ") + e.what());
    }
    return ok != 0;
}

int App::marcaAgua() {
    try {
        const long m = repo().configEntero("MarcaAgua", MARCA_DIAGONAL);
        return (m == MARCA_DIAGONAL || m == MARCA_SELLO) ? static_cast<int>(m) : MARCA_NINGUNA;
    } catch (...) {
        return MARCA_NINGUNA;
    }
}

HBITMAP App::marcaAguaParaLista(int marca) {
    const ImagenDib img = imagenRecurso(marca == MARCA_SELLO ? IDB_MARCA2 : IDB_MARCA1);
    if (!img.info) return nullptr;
    HDC pantalla = GetDC(nullptr);
    const int lado = MulDiv(230, GetDeviceCaps(pantalla, LOGPIXELSY), 96);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = lado;
    bi.bmiHeader.biHeight = lado;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(pantalla, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC mem = CreateCompatibleDC(pantalla);
    HGDIOBJ anterior = SelectObject(mem, bmp);
    RECT r{0, 0, lado, lado};
    FillRect(mem, &r, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    SetStretchBltMode(mem, HALFTONE);
    SetBrushOrgEx(mem, 0, 0, nullptr);
    StretchDIBits(mem, 0, 0, lado, lado, 0, 0, img.ancho, img.alto, img.bits, img.info, DIB_RGB_COLORS, SRCCOPY);
    SelectObject(mem, anterior);
    DeleteDC(mem);
    ReleaseDC(nullptr, pantalla);
    return bmp;
}

HBITMAP App::logoTenue(int ancho, int alto) {
    const ImagenDib img = imagenRecurso(IDB_LOGO_DOC);
    if (!img.info || ancho <= 0 || alto <= 0) return nullptr;
    const double factor = std::min(static_cast<double>(ancho) / img.ancho, static_cast<double>(alto) / img.alto);
    const int w = std::max(1, static_cast<int>(img.ancho * factor)), h = std::max(1, static_cast<int>(img.alto * factor));
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HDC pantalla = GetDC(nullptr);
    HBITMAP bmp = CreateDIBSection(pantalla, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC mem = CreateCompatibleDC(pantalla);
    HGDIOBJ anterior = SelectObject(mem, bmp);
    SetStretchBltMode(mem, HALFTONE);
    SetBrushOrgEx(mem, 0, 0, nullptr);
    StretchDIBits(mem, 0, 0, w, h, 0, 0, img.ancho, img.alto, img.bits, img.info, DIB_RGB_COLORS, SRCCOPY);
    GdiFlush();
    // Aclarar: cada color queda al 12 % de su intensidad sobre blanco.
    auto* p = static_cast<BYTE*>(bits);
    for (int i = 0; i < w * h * 4; ++i)
        if (i % 4 != 3) p[i] = static_cast<BYTE>(255 - (255 - p[i]) * 12 / 100);
    SelectObject(mem, anterior);
    DeleteDC(mem);
    ReleaseDC(nullptr, pantalla);
    return bmp;
}

std::wstring App::rutaMarcaAguaPng(int marca) const {
    return rutas_.plantillas + (marca == MARCA_SELLO ? L"\\marca_agua_2.png" : L"\\marca_agua_1.png");
}

Centavos App::minimoCaja() {
    try {
        return parsearMonto(repo().config("MinimoCaja", "5,000.00")).value_or(500000);
    } catch (...) {
        return 500000;
    }
}

bool App::actualizarCaja() {
    try {
        saldoCaja_ = repo().resumenCaja(Fecha::hoy()).esperado;
        cajaBaja_ = saldoCaja_ < minimoCaja();
    } catch (...) {
        cajaBaja_ = false;
    }
    if (ventana) InvalidateRect(ventana, nullptr, FALSE);
    return cajaBaja_;
}

void App::avisarCajaBaja(HWND padre) {
    if (actualizarCaja())
        mostrarAviso(padre, "¡Atención! En caja quedan " + formatoLempiras(saldoCaja_) + ", menos del mínimo de " +
                                formatoLempiras(minimoCaja()) + ".\n\nConsidere reponer efectivo antes de seguir pagando.");
}

void App::refrescarMarcaAgua() {
    if (ventana) PostMessageW(ventana, WM_APP_MARCA_AGUA, 0, 0);
}

std::vector<std::pair<std::string, std::string>> App::versionesPlantillas() const {
    std::vector<std::pair<std::string, std::string>> r{{"", "Actuales (versión " + std::to_string(VERSION_PLANTILLAS) + ")"}};
    const std::wstring anteriores = rutas_.plantillas + L"\\Anteriores";
    WIN32_FIND_DATAW fd;
    bool sueltas = false;
    HANDLE h = FindFirstFileW((anteriores + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                r.push_back({"Anteriores\\" + U(n), U(n)});
            else if (n.size() > 5 && n.substr(n.size() - 5) == L".xlsx")
                sueltas = true;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (sueltas) r.push_back({"Anteriores", "Anteriores (sin fecha)"});
    return r;
}

std::string App::configEquipo(const std::string& clave, const std::string& defecto) const {
    wchar_t buf[512] = {0};
    DWORD tamano = sizeof buf;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\BeneficioUnimax", W(clave).c_str(), RRF_RT_REG_SZ, nullptr, buf,
                     &tamano) == ERROR_SUCCESS)
        return U(buf);
    return defecto;
}

void App::setConfigEquipo(const std::string& clave, const std::string& valor) {
    const std::wstring v = W(valor);
    RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\BeneficioUnimax", W(clave).c_str(), REG_SZ, v.c_str(),
                    static_cast<DWORD>((v.size() + 1) * sizeof(wchar_t)));
}

std::wstring App::rutaPlantilla(const std::string& archivo) const {
    // Versión de plantillas elegida en Configuración (si el archivo existe en esa versión).
    std::string version;
    try {
        if (repo_) version = repo_->config("PlantillasEnUso");
    } catch (...) {
    }
    if (!version.empty()) {
        const std::wstring anterior = rutas_.plantillas + L"\\" + W(version) + L"\\" + W(archivo);
        if (existeArchivo(anterior)) return anterior;
    }
    const std::wstring personal = rutas_.plantillas + L"\\" + W(archivo);
    if (existeArchivo(personal)) return personal;
    return rutas_.carpetaExe + L"\\plantillas\\" + W(archivo);
}

void App::respaldar(HWND padre) {
    std::wstring nombre = L"BeneficioUnimax_respaldo_" + W(Fecha::hoy().iso()) + L".accdb";
    std::wstring archivo(MAX_PATH, L'\0');
    nombre.copy(&archivo[0], nombre.size());
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = padre;
    ofn.lpstrFilter = L"Base de datos de Access (*.accdb)\0*.accdb\0";
    ofn.lpstrFile = &archivo[0];
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = rutas_.respaldos.c_str();
    ofn.lpstrDefExt = L"accdb";
    ofn.lpstrTitle = L"Guardar respaldo de la base de datos";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;
    const std::wstring destino = archivo.c_str();

    // Se cierra la conexión para copiar el archivo completo y consistente.
    conexion_.cerrar();
    const BOOL ok = CopyFileW(baseAbierta_.c_str(), destino.c_str(), FALSE);
    const DWORD codigo = GetLastError();
    try {
        abrirBaseDatos();
    } catch (const std::exception& e) {
        mostrarError(padre, std::string("No se pudo volver a abrir la base de datos: ") + e.what());
    }
    if (ok)
        mostrarInfo(padre, "Respaldo guardado en:\n" + U(destino) +
                               "\n\nGuarde una copia en una memoria USB o en la nube regularmente.");
    else
        mostrarError(padre, "No se pudo copiar la base de datos (código " + std::to_string(codigo) + ").");
}

void App::mostrarRecordatorio(HWND padre, bool forzado) {
    intentar(padre, [&] {
        const Fecha hoy = Fecha::hoy();
        const bool alAbrir = repo().config("RecordatorioAlAbrir", "1") == "1";
        if (!forzado && (!alAbrir || repo().config("UltimoRecordatorio") == hoy.iso())) return;
        const Recordatorio r = repo().recordatorio(hoy);
        try {
            repo().setConfig("UltimoRecordatorio", hoy.iso());
        } catch (const std::exception&) {
            // SOLO CONSULTA: no se anota, no importa.
        }
        if (forzado || r.hayAlgo()) dialogoRecordatorio(padre, r);
    });
}

void App::irAPagina(int indice) {
    if (ventana) PostMessageW(ventana, WM_APP_IR_PAGINA, static_cast<WPARAM>(indice), 0);
}

}  // namespace bu::win
