#include "win/Http.h"

#include <winhttp.h>

#include <algorithm>

namespace bu::win {

namespace {

struct Manija {
    HINTERNET h = nullptr;
    explicit Manija(HINTERNET x) : h(x) {}
    ~Manija() {
        if (h) WinHttpCloseHandle(h);
    }
    Manija(const Manija&) = delete;
    Manija& operator=(const Manija&) = delete;
};

[[noreturn]] void fallar(const char* paso) {
    const DWORD codigo = GetLastError();
    std::string motivo;
    switch (codigo) {
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
        case ERROR_WINHTTP_CANNOT_CONNECT:
        case ERROR_WINHTTP_CONNECTION_ERROR:
            motivo = "No hay conexión a Internet o el servidor no responde.";
            break;
        case ERROR_WINHTTP_TIMEOUT:
            motivo = "La conexión a Internet tardó demasiado.";
            break;
        case ERROR_WINHTTP_SECURE_FAILURE:
            motivo = "No se pudo establecer una conexión segura (revise la fecha y hora de la computadora).";
            break;
        default:
            motivo = "Error de conexión.";
    }
    throw ErrorRed(motivo + " (" + paso + ", código " + std::to_string(codigo) + ")");
}

std::wstring encabezadoTexto(HINTERNET req, DWORD consulta) {
    DWORD tamano = 0;
    WinHttpQueryHeaders(req, consulta, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &tamano, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || tamano == 0) return L"";
    std::wstring r(tamano / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(req, consulta, WINHTTP_HEADER_NAME_BY_INDEX, &r[0], &tamano, WINHTTP_NO_HEADER_INDEX))
        return L"";
    r.resize(tamano / sizeof(wchar_t));
    return r;
}

}  // namespace

RespuestaHttp peticionHttp(const std::string& metodo, const std::string& url, const Encabezados& encabezados,
                           const std::string& cuerpo, const std::wstring& archivoSalida, const Progreso& progreso) {
    const std::wstring wurl = W(url);
    URL_COMPONENTS partes{};
    partes.dwStructSize = sizeof partes;
    partes.dwSchemeLength = static_cast<DWORD>(-1);
    partes.dwHostNameLength = static_cast<DWORD>(-1);
    partes.dwUrlPathLength = static_cast<DWORD>(-1);
    partes.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &partes)) throw ErrorRed("Dirección inválida: " + url);
    const std::wstring host(partes.lpszHostName, partes.dwHostNameLength);
    const std::wstring ruta = std::wstring(partes.lpszUrlPath, partes.dwUrlPathLength) +
                              std::wstring(partes.lpszExtraInfo ? partes.lpszExtraInfo : L"", partes.dwExtraInfoLength);
    const bool seguro = partes.nScheme == INTERNET_SCHEME_HTTPS;

    // WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY (Windows 8.1+) usa el proxy del sistema; si no existe, el predeterminado.
    HINTERNET s = WinHttpOpen(L"BeneficioUnimax/1.0", 4 /*WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY*/,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s)
        s = WinHttpOpen(L"BeneficioUnimax/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                        WINHTTP_NO_PROXY_BYPASS, 0);
    Manija sesion(s);
    if (!sesion.h) fallar("abrir");
    WinHttpSetTimeouts(sesion.h, 15000, 15000, 120000, 120000);
    DWORD tls = 0x00000800 /*TLS1_2*/ | 0x00002000 /*TLS1_3*/;
    if (!WinHttpSetOption(sesion.h, WINHTTP_OPTION_SECURE_PROTOCOLS, &tls, sizeof tls)) {
        tls = 0x00000800;
        WinHttpSetOption(sesion.h, WINHTTP_OPTION_SECURE_PROTOCOLS, &tls, sizeof tls);
    }
    Manija con(WinHttpConnect(sesion.h, host.c_str(), partes.nPort, 0));
    if (!con.h) fallar("conectar");
    Manija req(WinHttpOpenRequest(con.h, W(metodo).c_str(), ruta.c_str(), nullptr, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES, seguro ? WINHTTP_FLAG_SECURE : 0));
    if (!req.h) fallar("preparar");

    std::wstring cab;
    for (const auto& e : encabezados) cab += W(e.first) + L": " + W(e.second) + L"\r\n";
    if (!WinHttpSendRequest(req.h, cab.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : cab.c_str(),
                            cab.empty() ? 0 : static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0,
                            static_cast<DWORD>(cuerpo.size()), 0))
        fallar("enviar");
    // El cuerpo se envía por partes para poder mostrar el progreso.
    const size_t trozo = 256 * 1024;
    for (size_t enviado = 0; enviado < cuerpo.size();) {
        const DWORD n = static_cast<DWORD>(std::min(trozo, cuerpo.size() - enviado));
        DWORD escritos = 0;
        if (!WinHttpWriteData(req.h, cuerpo.data() + enviado, n, &escritos)) fallar("enviar datos");
        enviado += escritos;
        if (progreso) progreso(static_cast<long long>(enviado), static_cast<long long>(cuerpo.size()));
    }
    if (!WinHttpReceiveResponse(req.h, nullptr)) fallar("recibir");

    RespuestaHttp r;
    DWORD estado = 0, tamano = sizeof estado;
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &estado, &tamano, WINHTTP_NO_HEADER_INDEX);
    r.estado = static_cast<int>(estado);
    r.ubicacion = U(encabezadoTexto(req.h, WINHTTP_QUERY_LOCATION));
    long long total = -1;
    const std::wstring largo = encabezadoTexto(req.h, WINHTTP_QUERY_CONTENT_LENGTH);
    if (!largo.empty()) total = _wtoi64(largo.c_str());

    HANDLE archivo = INVALID_HANDLE_VALUE;
    const bool aArchivo = !archivoSalida.empty() && r.ok();
    if (aArchivo) {
        archivo = CreateFileW(archivoSalida.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
        if (archivo == INVALID_HANDLE_VALUE)
            throw std::runtime_error("No se pudo crear el archivo " + U(archivoSalida));
    }
    long long recibidos = 0;
    std::string buf;
    bool correcto = true;
    while (true) {
        DWORD disponible = 0;
        if (!WinHttpQueryDataAvailable(req.h, &disponible)) {
            correcto = false;
            break;
        }
        if (disponible == 0) break;
        buf.resize(disponible);
        DWORD leidos = 0;
        if (!WinHttpReadData(req.h, &buf[0], disponible, &leidos)) {
            correcto = false;
            break;
        }
        if (leidos == 0) break;
        recibidos += leidos;
        if (aArchivo) {
            DWORD escritos = 0;
            if (!WriteFile(archivo, buf.data(), leidos, &escritos, nullptr) || escritos != leidos) {
                correcto = false;
                break;
            }
        } else {
            r.cuerpo.append(buf.data(), leidos);
        }
        if (progreso) progreso(recibidos, total);
    }
    if (archivo != INVALID_HANDLE_VALUE) {
        CloseHandle(archivo);
        if (!correcto || (total >= 0 && recibidos != total)) DeleteFileW(archivoSalida.c_str());
    }
    if (!correcto) fallar("leer respuesta");
    if (total >= 0 && recibidos != total) throw ErrorRed("La descarga se interrumpió. Intente de nuevo.");
    return r;
}

bool leerArchivo(const std::wstring& ruta, std::string& datos) {
    HANDLE f = CreateFileW(ruta.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER t{};
    GetFileSizeEx(f, &t);
    datos.assign(static_cast<size_t>(t.QuadPart), '\0');
    size_t hecho = 0;
    bool ok = true;
    while (hecho < datos.size()) {
        DWORD leidos = 0;
        const DWORD pedir = static_cast<DWORD>(std::min<size_t>(datos.size() - hecho, 1 << 20));
        if (!ReadFile(f, &datos[hecho], pedir, &leidos, nullptr) || leidos == 0) {
            ok = false;
            break;
        }
        hecho += leidos;
    }
    CloseHandle(f);
    return ok;
}

bool escribirArchivo(const std::wstring& ruta, const std::string& datos) {
    const std::wstring temporal = ruta + L".nuevo";
    HANDLE f = CreateFileW(temporal.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD escritos = 0;
    const BOOL ok = datos.empty() || WriteFile(f, datos.data(), static_cast<DWORD>(datos.size()), &escritos, nullptr);
    CloseHandle(f);
    if (!ok || escritos != datos.size()) {
        DeleteFileW(temporal.c_str());
        return false;
    }
    return MoveFileExW(temporal.c_str(), ruta.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

}  // namespace bu::win
