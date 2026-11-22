#include "win/App.h"

#include <commdlg.h>
#include <knownfolders.h>
#include <shlobj.h>

#include "db/Esquema.h"
#include "win/Dialogos.h"
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

bool App::iniciar(const std::wstring& carpetaDatos, std::string& error) {
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
    copiarPlantillas(rutas_.carpetaExe + L"\\plantillas", rutas_.plantillas);
    extraerPlantillasIncluidas(rutas_.plantillas);

    if (!existeArchivo(rutas_.baseDatos)) {
        const std::string e = db::crearArchivoAccess(rutas_.baseDatos);
        if (!e.empty()) {
            error = "No se pudo crear la base de datos de Access:\n" + U(rutas_.baseDatos) + "\n\nDetalle: " + e +
                    "\n\nSolución: instale el \"Microsoft Access Database Engine 2016 Redistributable\" de 64 bits "
                    "(o compile el programa con la misma cantidad de bits que su Office).";
            return false;
        }
    }
    try {
        abrirBaseDatos();
    } catch (const std::exception& e) {
        error = std::string("No se pudo abrir la base de datos:\n") + U(rutas_.baseDatos) + "\n\nDetalle: " + e.what() +
                "\n\nSi el detalle menciona el controlador (driver), instale el \"Microsoft Access Database Engine "
                "2016 Redistributable\" de 64 bits.";
        return false;
    }
    return true;
}

void App::abrirBaseDatos() {
    conexion_.abrir(db::cadenaConexionAccess(U(rutas_.baseDatos)));
    db::crearEsquema(conexion_, db::Dialecto::Access);
    repo_ = std::make_unique<Repositorio>(conexion_, db::Dialecto::Access);
}

std::wstring App::rutaPlantilla(const std::string& archivo) const {
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
    const BOOL ok = CopyFileW(rutas_.baseDatos.c_str(), destino.c_str(), FALSE);
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
        repo().setConfig("UltimoRecordatorio", hoy.iso());
        if (forzado || r.hayAlgo()) dialogoRecordatorio(padre, r);
    });
}

void App::irAPagina(int indice) {
    if (ventana) PostMessageW(ventana, WM_APP_IR_PAGINA, static_cast<WPARAM>(indice), 0);
}

}  // namespace bu::win
