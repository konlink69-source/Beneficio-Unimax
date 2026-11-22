#include "win/Instalacion.h"

#include <knownfolders.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace bu::win {

namespace {

const wchar_t* const CLAVE_REGISTRO = L"Software\\BeneficioUnimax";
const wchar_t* const VALOR_NO_PREGUNTAR = L"NoPreguntarInstalacion";

std::wstring carpetaConocida(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &p))) r = p;
    if (p) CoTaskMemFree(p);
    return r;
}

std::wstring minusculas(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], static_cast<DWORD>(s.size()));
    return s;
}

bool crearAccesoDirecto(const std::wstring& archivoLnk, const std::wstring& destino, const std::wstring& carpeta) {
    IShellLinkW* enlace = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                reinterpret_cast<void**>(&enlace))))
        return false;
    enlace->SetPath(destino.c_str());
    enlace->SetWorkingDirectory(carpeta.c_str());
    enlace->SetDescription(L"Beneficio Unimax - Contabilidad de compra de café");
    enlace->SetIconLocation(destino.c_str(), 0);
    bool ok = false;
    IPersistFile* archivo = nullptr;
    if (SUCCEEDED(enlace->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&archivo)))) {
        ok = SUCCEEDED(archivo->Save(archivoLnk.c_str(), TRUE));
        archivo->Release();
    }
    enlace->Release();
    return ok;
}

}  // namespace

std::wstring carpetaInstalacion() {
    std::wstring base = carpetaConocida(FOLDERID_UserProgramFiles);  // %LOCALAPPDATA%\Programs
    if (base.empty()) base = carpetaConocida(FOLDERID_LocalAppData) + L"\\Programs";
    return base + L"\\BeneficioUnimax";
}

std::wstring exeInstalado() { return carpetaInstalacion() + L"\\BeneficioUnimax.exe"; }

bool ejecutandoDesdeInstalacion(const std::wstring& exe) {
    return minusculas(carpetaDe(exe)) == minusculas(carpetaInstalacion());
}

bool noPreguntarInstalacion() {
    DWORD valor = 0;
    DWORD tamano = sizeof valor;
    return RegGetValueW(HKEY_CURRENT_USER, CLAVE_REGISTRO, VALOR_NO_PREGUNTAR, RRF_RT_REG_DWORD, nullptr, &valor,
                        &tamano) == ERROR_SUCCESS &&
           valor != 0;
}

void guardarNoPreguntarInstalacion() {
    const DWORD valor = 1;
    RegSetKeyValueW(HKEY_CURRENT_USER, CLAVE_REGISTRO, VALOR_NO_PREGUNTAR, REG_DWORD, &valor, sizeof valor);
}

bool instalar(const std::wstring& exeActual, std::string& mensaje) {
    const std::wstring carpeta = carpetaInstalacion();
    const std::wstring destino = exeInstalado();
    crearCarpetas(carpeta);
    if (minusculas(exeActual) != minusculas(destino)) {
        if (!CopyFileW(exeActual.c_str(), destino.c_str(), FALSE)) {
            mensaje = "No se pudo copiar el programa a:\n" + U(destino) + "\n(código " +
                      std::to_string(GetLastError()) + "). Cierre Beneficio Unimax si está abierto e intente de nuevo.";
            return false;
        }
        // El usuario ya aceptó abrir el archivo descargado: se quita la marca "descargado de Internet"
        // para que Windows no vuelva a preguntar cada vez que se abra desde el ícono.
        DeleteFileW((destino + L":Zone.Identifier").c_str());
    }
    const std::wstring escritorio = carpetaConocida(FOLDERID_Desktop);
    const std::wstring menuInicio = carpetaConocida(FOLDERID_Programs);
    const bool iconoEscritorio =
        !escritorio.empty() && crearAccesoDirecto(escritorio + L"\\Beneficio Unimax.lnk", destino, carpeta);
    if (!menuInicio.empty()) crearAccesoDirecto(menuInicio + L"\\Beneficio Unimax.lnk", destino, carpeta);
    if (!iconoEscritorio)
        mensaje = "El programa quedó instalado en:\n" + U(destino) +
                  "\npero no se pudo crear el ícono en el escritorio. Búsquelo en el menú Inicio como \"Beneficio Unimax\".";
    return true;
}

}  // namespace bu::win
