#include "win/Actualizacion.h"

#include <knownfolders.h>

#include <algorithm>
#include <shellapi.h>
#include <shlobj.h>

#include <thread>

#include "core/Seguridad.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Http.h"
#include "win/Instalacion.h"
#include "win/Nube.h"
#include "win/Tareas.h"
#include "win/Version.h"
#include "win/resource.h"

#ifndef BU_REPOSITORIO
#define BU_REPOSITORIO "konlink69-source/Beneficio-Unimax"
#endif

namespace bu::win {

namespace {

std::string urlDescarga(const std::string& archivo) {
    return std::string("https://github.com/") + BU_REPOSITORIO + "/releases/latest/download/" + archivo;
}

std::wstring carpetaLocal() {
    PWSTR p = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &p))) r = p;
    if (p) CoTaskMemFree(p);
    r += L"\\BeneficioUnimax";
    crearCarpetas(r);
    return r;
}

std::wstring archivoNovedades() { return carpetaLocal() + L"\\novedades.txt"; }

class DialogoActualizacion : public DialogoModal {
public:
    explicit DialogoActualizacion(const InfoVersion& v) : v_(v) {}

protected:
    void alIniciar() override {
        ponerTexto(h_, IDC_AC_INFO,
                   "Hay una versión nueva de Beneficio Unimax: versión " + std::to_string(v_.numero) + " (usted tiene la " +
                       std::to_string(VERSION_PROGRAMA) + ").\n\nLa actualización tarda menos de un minuto.");
        std::string notas = v_.notas.empty() ? "Mejoras y correcciones." : v_.notas;
        // El cuadro de texto necesita \r\n.
        std::string conRetorno;
        for (const char c : notas) {
            if (c == '\n') conRetorno += "\r\n";
            else conRetorno += c;
        }
        ponerTexto(h_, IDC_AC_NOTAS, conRetorno);
    }

private:
    InfoVersion v_;
};

}  // namespace

void buscarActualizacion(HWND ventana, bool manual) {
    std::thread([ventana, manual] {
        auto* info = new InfoVersion();
        try {
            const RespuestaHttp r = peticionHttp("GET", urlDescarga("version.txt"));
            if (r.ok()) {
                *info = parsearInfoVersion(r.cuerpo);
                if (!info->valida()) {
                    info->numero = -1;
                    info->notas = "El archivo de versión publicado no es válido.";
                }
            } else if (r.estado == 404) {
                info->numero = -1;
                info->notas = "No se pudo entrar a la página de descargas en GitHub (" + std::string(BU_REPOSITORIO) +
                              "). Si el repositorio es privado, el programa no puede descargar las actualizaciones: "
                              "vea el manual, sección 9.";
            } else {
                info->numero = -1;
                info->notas = "No se encontró la información de la versión (HTTP " + std::to_string(r.estado) + ").";
            }
        } catch (const std::exception& e) {
            info->numero = -1;
            info->notas = e.what();
        }
        const bool hayNueva = info->numero > VERSION_PROGRAMA && VERSION_PROGRAMA > 0;
        if ((hayNueva || manual) && IsWindow(ventana))
            PostMessageW(ventana, WM_APP_ACTUALIZACION, manual ? 1 : 0, reinterpret_cast<LPARAM>(info));
        else
            delete info;
    }).detach();
}

bool ofrecerActualizacion(HWND ventana, const InfoVersion& v, bool hayTrabajoSinGuardar) {
    DialogoActualizacion d(v);
    if (d.mostrar(ventana, IDD_ACTUALIZACION) != IDOK) return false;
    if (hayTrabajoSinGuardar &&
        !confirmar(ventana, "Hay un comprobante a medio llenar que no se ha guardado. Si actualiza ahora se perderá.\n\n"
                            "¿Actualizar de todas formas? (Elija No para guardarlo primero y actualizar después desde "
                            "Ayuda → Buscar actualizaciones.)"))
        return false;

    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    const std::wstring nuevo = std::wstring(dir) + L"BeneficioUnimax_v" + std::to_wstring(v.numero) + L".exe";
    std::string error;
    const bool ok = ejecutarConProgreso(
        ventana, "Descargando la actualización",
        [&](Avance& a) {
            a.texto("Descargando la versión " + std::to_string(v.numero) + "...");
            const RespuestaHttp r = peticionHttp("GET", urlDescarga("BeneficioUnimax.exe"), {}, "", nuevo,
                                                 [&](long long h, long long t) {
                                                     if (t > 0) a.porcentaje(static_cast<int>(h * 100 / t));
                                                 });
            if (!r.ok()) throw std::runtime_error("No se pudo descargar (HTTP " + std::to_string(r.estado) + ").");
            a.porcentaje(-1);
            a.texto("Comprobando el archivo descargado...");
            std::string datos;
            if (!leerArchivo(nuevo, datos)) throw std::runtime_error("No se pudo leer el archivo descargado.");
            const Sha256 h = sha256(datos.data(), datos.size());
            if ((v.tamano > 0 && static_cast<long long>(datos.size()) != v.tamano) || aHex(h.data(), h.size()) != v.sha256 ||
                datos.size() < 2 || datos[0] != 'M' || datos[1] != 'Z') {
                DeleteFileW(nuevo.c_str());
                throw std::runtime_error("El archivo descargado está incompleto o dañado. Intente de nuevo más tarde.");
            }
        },
        error, true);
    if (!ok) {
        if (!error.empty()) mostrarError(ventana, "No se pudo descargar la actualización:\n" + error);
        DeleteFileW(nuevo.c_str());
        return false;
    }

    // Respaldo y liberar los datos antes de cerrar.
    App::instancia().respaldoAutomatico();
    Nube::instancia().alSalir(ventana, "antes de actualizar");

    std::string notas = "Beneficio Unimax se actualizó a la versión " + std::to_string(v.numero) + ".\n\n";
    notas += v.notas.empty() ? "Mejoras y correcciones." : "Qué cambió:\n" + v.notas;
    notas += "\n\nSus datos, facturas y plantillas siguen igual.";
    escribirArchivo(archivoNovedades(), notas);

    const Rutas& rutas = App::instancia().rutas();
    std::wstring argumentos = L"/actualizar " + std::to_wstring(GetCurrentProcessId()) + L" \"" + rutas.exe + L"\"";
    if (rutas.datosPersonalizados) argumentos += L" /datos \"" + rutas.datos + L"\"";
    const HINSTANCE r = ShellExecuteW(nullptr, L"open", nuevo.c_str(), argumentos.c_str(), dir, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(r) <= 32) {
        mostrarError(ventana, "No se pudo abrir el instalador de la actualización (código " +
                                  std::to_string(reinterpret_cast<INT_PTR>(r)) + ").");
        return false;
    }
    return true;
}

int completarActualizacion(DWORD pid, const std::wstring& destino, const std::wstring& carpetaDatos) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    // 1. Esperar a que se cierre la versión anterior.
    if (HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid)) {
        WaitForSingleObject(p, 60000);
        CloseHandle(p);
    }
    // 2. Reemplazar el programa (se reintenta por si Windows todavía lo tiene abierto).
    const std::wstring anterior = destino + L".anterior";
    bool ok = false;
    DWORD codigo = 0;
    for (int i = 0; i < 40 && !ok; ++i) {
        MoveFileExW(destino.c_str(), anterior.c_str(), MOVEFILE_REPLACE_EXISTING);
        ok = CopyFileW(exe, destino.c_str(), FALSE) != 0;
        if (!ok) {
            codigo = GetLastError();
            MoveFileExW(anterior.c_str(), destino.c_str(), MOVEFILE_REPLACE_EXISTING);
            Sleep(500);
        }
    }
    if (!ok) {
        MessageBoxW(nullptr,
                    (L"No se pudo instalar la actualización (código " + std::to_wstring(codigo) +
                     L").\n\nCierre Beneficio Unimax si sigue abierto y vuelva a intentarlo desde Ayuda → Buscar "
                     L"actualizaciones. El programa anterior sigue funcionando.")
                        .c_str(),
                    L"Beneficio Unimax", MB_ICONERROR);
        ShellExecuteW(nullptr, L"open", destino.c_str(), nullptr, carpetaDe(destino).c_str(), SW_SHOWNORMAL);
        return 1;
    }
    DeleteFileW(anterior.c_str());
    DeleteFileW((destino + L":Zone.Identifier").c_str());
    // Íconos del escritorio y del menú Inicio (si es la copia instalada).
    if (ejecutandoDesdeInstalacion(destino)) {
        std::string mensaje;
        instalar(destino, mensaje);
    }
    // 3. Abrir la versión nueva.
    std::wstring argumentos = L"/actualizado";
    if (!carpetaDatos.empty()) argumentos += L" /datos \"" + carpetaDatos + L"\"";
    ShellExecuteW(nullptr, L"open", destino.c_str(), argumentos.c_str(), carpetaDe(destino).c_str(), SW_SHOWNORMAL);
    // El archivo descargado se borra la próxima vez (ver mostrarNovedades).
    return 0;
}

void mostrarNovedades(HWND ventana) {
    // Se borran los instaladores descargados que quedaron en la carpeta temporal.
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((std::wstring(dir) + L"BeneficioUnimax_v*.exe").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            DeleteFileW((std::wstring(dir) + fd.cFileName).c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::string notas;
    if (!leerArchivo(archivoNovedades(), notas) || notas.empty()) return;
    DeleteFileW(archivoNovedades().c_str());
    mostrarInfo(ventana, notas);
}

}  // namespace bu::win
