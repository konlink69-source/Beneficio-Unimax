// winsock2.h debe incluirse antes que windows.h.
#include <winsock2.h>
#include <ws2tcpip.h>

#include "win/GoogleDrive.h"

#include <knownfolders.h>

#include <algorithm>
#include <shellapi.h>
#include <shlobj.h>
#include <wincrypt.h>

#include "win/Tareas.h"

#ifndef BU_GOOGLE_CLIENT_ID
#define BU_GOOGLE_CLIENT_ID ""
#endif
#ifndef BU_GOOGLE_CLIENT_SECRET
#define BU_GOOGLE_CLIENT_SECRET ""
#endif

namespace bu::win {

namespace {

const char* const API = "https://www.googleapis.com/drive/v3";
const char* const API_SUBIDA = "https://www.googleapis.com/upload/drive/v3";
const char* const CAMPOS = "id,name,size,modifiedTime,createdTime,mimeType";
const char* const TIPO_CARPETA = "application/vnd.google-apps.folder";

std::wstring carpetaLocal() {
    PWSTR p = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &p))) r = p;
    if (p) CoTaskMemFree(p);
    r += L"\\BeneficioUnimax";
    crearCarpetas(r);
    return r;
}

std::string cifrar(const std::string& texto) {
    DATA_BLOB entrada{static_cast<DWORD>(texto.size()), reinterpret_cast<BYTE*>(const_cast<char*>(texto.data()))};
    DATA_BLOB salida{};
    if (!CryptProtectData(&entrada, L"BeneficioUnimax", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &salida))
        return "";
    std::string r(reinterpret_cast<char*>(salida.pbData), salida.cbData);
    LocalFree(salida.pbData);
    return r;
}

std::string descifrar(const std::string& datos) {
    DATA_BLOB entrada{static_cast<DWORD>(datos.size()), reinterpret_cast<BYTE*>(const_cast<char*>(datos.data()))};
    DATA_BLOB salida{};
    if (!CryptUnprotectData(&entrada, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &salida)) return "";
    std::string r(reinterpret_cast<char*>(salida.pbData), salida.cbData);
    LocalFree(salida.pbData);
    return r;
}

ArchivoDrive archivoDesdeJson(const Json& j) {
    ArchivoDrive a;
    a.id = j["id"].texto();
    a.nombre = j["name"].texto();
    a.tamano = j["size"].entero();
    a.modificado = momentoDesdeRfc3339(j["modifiedTime"].texto());
    a.creado = momentoDesdeRfc3339(j["createdTime"].texto());
    a.carpeta = j["mimeType"].texto() == TIPO_CARPETA;
    return a;
}

std::string comillas(const std::string& s) {
    std::string r = "'";
    for (const char c : s) {
        if (c == '\'' || c == '\\') r += '\\';
        r += c;
    }
    return r + "'";
}

std::string mensajeGoogle(const RespuestaHttp& r) {
    try {
        const Json j = Json::parsear(r.cuerpo);
        if (j["error"].esObjeto()) return j["error"]["message"].texto();
        if (!j["error_description"].texto().empty()) return j["error_description"].texto();
        return j["error"].texto();
    } catch (...) {
        return r.cuerpo.substr(0, 200);
    }
}

[[noreturn]] void fallaGoogle(const std::string& que, const RespuestaHttp& r) {
    throw std::runtime_error("Google Drive: no se pudo " + que + " (HTTP " + std::to_string(r.estado) + "). " +
                             mensajeGoogle(r));
}

// ---------------------------------------------------------------- Servidor local para recibir la autorización
class ServidorLocal {
public:
    ServidorLocal() {
        WSADATA w;
        iniciado_ = WSAStartup(MAKEWORD(2, 2), &w) == 0;
        s_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s_ == INVALID_SOCKET) throw std::runtime_error("No se pudo preparar la conexión local para Google.");
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = 0;  // puerto libre cualquiera
        if (bind(s_, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || listen(s_, 4) != 0)
            throw std::runtime_error("No se pudo preparar la conexión local para Google.");
        int largo = sizeof a;
        getsockname(s_, reinterpret_cast<sockaddr*>(&a), &largo);
        puerto_ = ntohs(a.sin_port);
    }
    ~ServidorLocal() {
        if (s_ != INVALID_SOCKET) closesocket(s_);
        if (iniciado_) WSACleanup();
    }
    int puerto() const { return puerto_; }

    // Espera la visita del navegador a http://127.0.0.1:<puerto>/?code=... y devuelve la consulta.
    std::string esperar(Avance& avance, int segundos) {
        for (int i = 0; i < segundos * 2; ++i) {
            if (avance.cancelado()) return "";
            fd_set fs;
            FD_ZERO(&fs);
            FD_SET(s_, &fs);
            timeval t{0, 500000};
            if (select(0, &fs, nullptr, nullptr, &t) <= 0) continue;
            SOCKET c = accept(s_, nullptr, nullptr);
            if (c == INVALID_SOCKET) continue;
            DWORD espera = 5000;
            setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&espera), sizeof espera);
            std::string pedido;
            char buf[2048];
            while (pedido.find("\r\n") == std::string::npos && pedido.size() < 16384) {
                const int n = recv(c, buf, sizeof buf, 0);
                if (n <= 0) break;
                pedido.append(buf, static_cast<size_t>(n));
            }
            // "GET /?state=..&code=..&scope=.. HTTP/1.1"
            std::string consulta;
            const size_t ini = pedido.find(' '), fin = pedido.find(' ', ini == std::string::npos ? 0 : ini + 1);
            if (ini != std::string::npos && fin != std::string::npos) {
                const std::string ruta = pedido.substr(ini + 1, fin - ini - 1);
                const size_t q = ruta.find('?');
                if (q != std::string::npos) consulta = ruta.substr(q + 1);
            }
            const bool util = !parametroConsulta(consulta, "code").empty() || !parametroConsulta(consulta, "error").empty();
            const std::string html =
                util ? "<html><head><meta charset='utf-8'><title>Beneficio Unimax</title></head>"
                       "<body style='font-family:Segoe UI,Arial;text-align:center;margin-top:80px;color:#4a1e1b'>"
                       "<h2>Listo</h2><p>Ya puede cerrar esta pestaña y volver a Beneficio Unimax.</p></body></html>"
                     : "<html><body></body></html>";
            const std::string resp = std::string(util ? "HTTP/1.1 200 OK" : "HTTP/1.1 404 Not Found") +
                                     "\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: " +
                                     std::to_string(html.size()) + "\r\n\r\n" + html;
            send(c, resp.data(), static_cast<int>(resp.size()), 0);
            shutdown(c, SD_SEND);
            closesocket(c);
            if (util) return consulta;
        }
        return "";
    }

private:
    bool iniciado_ = false;
    SOCKET s_ = INVALID_SOCKET;
    int puerto_ = 0;
};

}  // namespace

GoogleDrive& GoogleDrive::instancia() {
    static GoogleDrive g;
    return g;
}

ClienteGoogle GoogleDrive::cliente() const {
    std::string datos;
    if (leerArchivo(carpetaLocal() + L"\\google_cliente.json", datos)) {
        try {
            const Json j = Json::parsear(datos);
            ClienteGoogle c{j["id"].texto(), j["secreto"].texto()};
            if (c.valido()) return c;
        } catch (...) {
        }
    }
    return ClienteGoogle{BU_GOOGLE_CLIENT_ID, BU_GOOGLE_CLIENT_SECRET};
}

bool GoogleDrive::clienteIncluido() const { return std::string(BU_GOOGLE_CLIENT_ID).size() > 0; }

void GoogleDrive::guardarCliente(const ClienteGoogle& c) {
    const std::wstring ruta = carpetaLocal() + L"\\google_cliente.json";
    if (!c.valido()) {
        DeleteFileW(ruta.c_str());
        return;
    }
    Json j = Json::objeto();
    j["id"] = c.id;
    j["secreto"] = c.secreto;
    escribirArchivo(ruta, j.serializar());
}

void GoogleDrive::cargar() {
    std::lock_guard<std::recursive_mutex> l(m_);
    if (cargado_) return;
    cargado_ = true;
    std::string datos;
    if (!leerArchivo(carpetaLocal() + L"\\google.dat", datos)) return;
    try {
        const Json j = Json::parsear(descifrar(datos));
        refresco_ = j["refresh_token"].texto();
        correo_ = j["correo"].texto();
    } catch (...) {
    }
}

void GoogleDrive::guardar() {
    Json j = Json::objeto();
    j["refresh_token"] = refresco_;
    j["correo"] = correo_;
    const std::string c = cifrar(j.serializar());
    if (!c.empty()) escribirArchivo(carpetaLocal() + L"\\google.dat", c);
}

bool GoogleDrive::conectado() {
    cargar();
    std::lock_guard<std::recursive_mutex> l(m_);
    return !refresco_.empty();
}

std::string GoogleDrive::cuenta() {
    cargar();
    std::lock_guard<std::recursive_mutex> l(m_);
    return correo_;
}

void GoogleDrive::desconectar() {
    std::lock_guard<std::recursive_mutex> l(m_);
    if (!refresco_.empty()) {
        try {
            peticionHttp("POST", "https://oauth2.googleapis.com/revoke",
                         {{"Content-Type", "application/x-www-form-urlencoded"}}, "token=" + codificarUrl(refresco_));
        } catch (...) {
        }
    }
    refresco_.clear();
    correo_.clear();
    acceso_.clear();
    venceAcceso_ = 0;
    DeleteFileW((carpetaLocal() + L"\\google.dat").c_str());
}

void GoogleDrive::conectar(Avance& avance) {
    const ClienteGoogle c = cliente();
    if (!c.valido())
        throw std::runtime_error(
            "Falta el \"ID de cliente de Google\". Escríbalo en Configuración → Nube (vea el manual, sección "
            "\"Conectar Google Drive\").");
    ServidorLocal servidor;
    const std::string redireccion = "http://127.0.0.1:" + std::to_string(servidor.puerto());
    const std::string verificador = verificadorPkce(bytesAleatorios(32));
    const std::vector<uint8_t> e = bytesAleatorios(16);
    const std::string estado = base64Url(e.data(), e.size());
    const std::string url = "https://accounts.google.com/o/oauth2/v2/auth?client_id=" + codificarUrl(c.id) +
                            "&redirect_uri=" + codificarUrl(redireccion) + "&response_type=code" +
                            "&scope=" + codificarUrl("https://www.googleapis.com/auth/drive.file") +
                            "&code_challenge=" + desafioPkce(verificador) + "&code_challenge_method=S256" +
                            "&access_type=offline&prompt=consent&state=" + estado;
    avance.texto("Se abrió el navegador. Elija su cuenta de Google y presione \"Permitir\".\n\n"
                 "(Si no se abrió, revise la barra de tareas.)");
    ShellExecuteW(nullptr, L"open", W(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    const std::string consulta = servidor.esperar(avance, 300);
    if (avance.cancelado()) return;
    if (consulta.empty()) throw std::runtime_error("No se recibió la autorización de Google (pasaron 5 minutos).");
    if (!parametroConsulta(consulta, "error").empty())
        throw std::runtime_error("Google no autorizó la conexión (" + parametroConsulta(consulta, "error") + ").");
    if (parametroConsulta(consulta, "state") != estado) throw std::runtime_error("Respuesta de Google no válida.");

    avance.texto("Conectando con Google Drive...");
    std::string cuerpo = "code=" + codificarUrl(parametroConsulta(consulta, "code")) +
                         "&client_id=" + codificarUrl(c.id) + "&redirect_uri=" + codificarUrl(redireccion) +
                         "&grant_type=authorization_code&code_verifier=" + verificador;
    if (!c.secreto.empty()) cuerpo += "&client_secret=" + codificarUrl(c.secreto);
    const RespuestaHttp r = peticionHttp("POST", "https://oauth2.googleapis.com/token",
                                         {{"Content-Type", "application/x-www-form-urlencoded"}}, cuerpo);
    if (!r.ok()) fallaGoogle("completar la conexión", r);
    const Json j = Json::parsear(r.cuerpo);
    std::lock_guard<std::recursive_mutex> l(m_);
    cargado_ = true;
    refresco_ = j["refresh_token"].texto();
    acceso_ = j["access_token"].texto();
    venceAcceso_ = GetTickCount64() + static_cast<unsigned long long>(j["expires_in"].entero(3600) - 120) * 1000ULL;
    if (refresco_.empty()) throw std::runtime_error("Google no entregó el permiso permanente. Intente de nuevo.");
    // Correo de la cuenta, para mostrarlo.
    try {
        const RespuestaHttp a = llamar("GET", std::string(API) + "/about?fields=user(emailAddress,displayName)");
        if (a.ok()) correo_ = Json::parsear(a.cuerpo)["user"]["emailAddress"].texto();
    } catch (...) {
    }
    guardar();
}

std::string GoogleDrive::token() {
    cargar();
    std::lock_guard<std::recursive_mutex> l(m_);
    if (!acceso_.empty() && GetTickCount64() < venceAcceso_) return acceso_;
    if (refresco_.empty()) throw std::runtime_error("Esta computadora no está conectada con Google Drive.");
    const ClienteGoogle c = cliente();
    std::string cuerpo = "client_id=" + codificarUrl(c.id) + "&refresh_token=" + codificarUrl(refresco_) +
                         "&grant_type=refresh_token";
    if (!c.secreto.empty()) cuerpo += "&client_secret=" + codificarUrl(c.secreto);
    const RespuestaHttp r = peticionHttp("POST", "https://oauth2.googleapis.com/token",
                                         {{"Content-Type", "application/x-www-form-urlencoded"}}, cuerpo);
    if (r.estado == 400 || r.estado == 401) {
        // Permiso revocado o vencido: hay que conectar de nuevo.
        refresco_.clear();
        guardar();
        throw std::runtime_error("Google retiró el permiso de esta computadora. Conecte de nuevo en Configuración → "
                                 "Nube → Conectar con Google Drive.");
    }
    if (!r.ok()) fallaGoogle("renovar el permiso", r);
    const Json j = Json::parsear(r.cuerpo);
    acceso_ = j["access_token"].texto();
    venceAcceso_ = GetTickCount64() + static_cast<unsigned long long>(j["expires_in"].entero(3600) - 120) * 1000ULL;
    return acceso_;
}

RespuestaHttp GoogleDrive::llamar(const std::string& metodo, const std::string& url, const std::string& cuerpo,
                                  const std::string& tipo, const std::wstring& archivo, const Progreso& p) {
    for (int intento = 0;; ++intento) {
        Encabezados e{{"Authorization", "Bearer " + token()}};
        if (!tipo.empty()) e.push_back({"Content-Type", tipo});
        RespuestaHttp r = peticionHttp(metodo, url, e, cuerpo, archivo, p);
        if (r.estado == 401 && intento == 0) {
            std::lock_guard<std::recursive_mutex> l(m_);
            acceso_.clear();
            continue;
        }
        // Límite de peticiones o error temporal de Google: se reintenta con espera.
        if ((r.estado == 429 || r.estado >= 500) && intento < 3) {
            Sleep(static_cast<DWORD>(1000 << intento));
            continue;
        }
        return r;
    }
}

std::optional<ArchivoDrive> GoogleDrive::buscar(const std::string& nombre, const std::string& carpetaId, bool esCarpeta) {
    std::string q = "name = " + comillas(nombre) + " and " + comillas(carpetaId) + " in parents and trashed = false";
    q += esCarpeta ? std::string(" and mimeType = '") + TIPO_CARPETA + "'"
                   : std::string(" and mimeType != '") + TIPO_CARPETA + "'";
    const RespuestaHttp r = llamar("GET", std::string(API) + "/files?spaces=drive&pageSize=10&orderBy=modifiedTime%20desc&q=" +
                                              codificarUrl(q) + "&fields=" + codificarUrl(std::string("files(") + CAMPOS + ")"));
    if (!r.ok()) fallaGoogle("buscar " + nombre, r);
    const Json j = Json::parsear(r.cuerpo);
    if (j["files"].tamano() == 0) return std::nullopt;
    return archivoDesdeJson(j["files"][0]);
}

std::vector<ArchivoDrive> GoogleDrive::listar(const std::string& carpetaId) {
    std::vector<ArchivoDrive> r;
    std::string pagina;
    const std::string q = comillas(carpetaId) + " in parents and trashed = false";
    do {
        std::string url = std::string(API) + "/files?spaces=drive&pageSize=1000&q=" + codificarUrl(q) +
                          "&fields=" + codificarUrl(std::string("nextPageToken,files(") + CAMPOS + ")");
        if (!pagina.empty()) url += "&pageToken=" + codificarUrl(pagina);
        const RespuestaHttp resp = llamar("GET", url);
        if (!resp.ok()) fallaGoogle("leer la carpeta", resp);
        const Json j = Json::parsear(resp.cuerpo);
        for (const auto& f : j["files"].elementos()) r.push_back(archivoDesdeJson(f));
        pagina = j["nextPageToken"].texto();
    } while (!pagina.empty());
    return r;
}

std::string GoogleDrive::carpeta(const std::string& nombre, const std::string& carpetaPadre) {
    if (const auto a = buscar(nombre, carpetaPadre, true)) return a->id;
    Json meta = Json::objeto();
    meta["name"] = nombre;
    meta["mimeType"] = TIPO_CARPETA;
    Json padres = Json::lista();
    padres.agregar(carpetaPadre);
    meta["parents"] = padres;
    const RespuestaHttp r =
        llamar("POST", std::string(API) + "/files?fields=id", meta.serializar(), "application/json; charset=UTF-8");
    if (!r.ok()) fallaGoogle("crear la carpeta " + nombre, r);
    return Json::parsear(r.cuerpo)["id"].texto();
}

ArchivoDrive GoogleDrive::subir(const std::string& datos, const std::string& nombre, const std::string& carpetaId,
                                const std::string& idExistente, const std::string& tipo, const Progreso& progreso) {
    // Subida "reanudable": primero los datos del archivo, luego el contenido.
    Json meta = Json::objeto();
    meta["name"] = nombre;
    std::string url;
    std::string metodo;
    if (idExistente.empty()) {
        Json padres = Json::lista();
        padres.agregar(carpetaId);
        meta["parents"] = padres;
        url = std::string(API_SUBIDA) + "/files?uploadType=resumable&fields=" + codificarUrl(CAMPOS);
        metodo = "POST";
    } else {
        url = std::string(API_SUBIDA) + "/files/" + idExistente + "?uploadType=resumable&fields=" + codificarUrl(CAMPOS);
        metodo = "PATCH";
    }
    RespuestaHttp inicio;
    for (int intento = 0;; ++intento) {
        Encabezados e{{"Authorization", "Bearer " + token()},
                      {"Content-Type", "application/json; charset=UTF-8"},
                      {"X-Upload-Content-Type", tipo},
                      {"X-Upload-Content-Length", std::to_string(datos.size())}};
        inicio = peticionHttp(metodo, url, e, meta.serializar());
        if (inicio.estado == 401 && intento == 0) {
            std::lock_guard<std::recursive_mutex> l(m_);
            acceso_.clear();
            continue;
        }
        break;
    }
    if (!inicio.ok() || inicio.ubicacion.empty()) fallaGoogle("empezar a subir " + nombre, inicio);
    const RespuestaHttp r = peticionHttp("PUT", inicio.ubicacion, {{"Content-Type", tipo}}, datos, L"", progreso);
    if (!r.ok()) fallaGoogle("subir " + nombre, r);
    return archivoDesdeJson(Json::parsear(r.cuerpo));
}

void GoogleDrive::descargar(const std::string& id, const std::wstring& destino, const Progreso& progreso) {
    const RespuestaHttp r = llamar("GET", std::string(API) + "/files/" + id + "?alt=media", "", "", destino, progreso);
    if (!r.ok()) fallaGoogle("descargar el archivo", r);
}

std::string GoogleDrive::leer(const std::string& id) {
    const RespuestaHttp r = llamar("GET", std::string(API) + "/files/" + id + "?alt=media");
    if (!r.ok()) fallaGoogle("leer el archivo", r);
    return r.cuerpo;
}

ArchivoDrive GoogleDrive::copiar(const std::string& id, const std::string& nombre, const std::string& carpetaId) {
    Json meta = Json::objeto();
    meta["name"] = nombre;
    Json padres = Json::lista();
    padres.agregar(carpetaId);
    meta["parents"] = padres;
    const RespuestaHttp r = llamar("POST", std::string(API) + "/files/" + id + "/copy?fields=" + codificarUrl(CAMPOS),
                                   meta.serializar(), "application/json; charset=UTF-8");
    if (!r.ok()) fallaGoogle("copiar " + nombre, r);
    return archivoDesdeJson(Json::parsear(r.cuerpo));
}

void GoogleDrive::borrar(const std::string& id) {
    const RespuestaHttp r = llamar("DELETE", std::string(API) + "/files/" + id);
    if (!r.ok() && r.estado != 404) fallaGoogle("borrar un respaldo viejo", r);
}

}  // namespace bu::win
