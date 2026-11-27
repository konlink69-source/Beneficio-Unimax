#pragma once
// Conexión directa con Google Drive (sin instalar "Google Drive para escritorio").
//
// Inicio de sesión: OAuth 2.0 para aplicaciones de escritorio (PKCE + dirección local 127.0.0.1).
// Se abre el navegador, el usuario elige su cuenta de Google y autoriza; el programa guarda un
// "token de actualización" cifrado con la cuenta de Windows (DPAPI) en esta computadora.
//
// Permiso pedido: "drive.file" = el programa solo ve los archivos que él mismo creó en Drive
// (no puede leer el resto de su Drive).
#include "win/Http.h"

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/Sincronizacion.h"

namespace bu::win {

class Avance;

struct ArchivoDrive {
    std::string id;
    std::string nombre;
    long long tamano = 0;
    Momento modificado = 0;
    Momento creado = 0;
    bool carpeta = false;
};

// Datos de la aplicación registrada en Google Cloud (ver README: "Conectar Google Drive").
struct ClienteGoogle {
    std::string id;
    std::string secreto;
    bool valido() const { return !id.empty(); }
};

class GoogleDrive {
public:
    static GoogleDrive& instancia();

    // Cliente: el incluido al compilar o el que se escribió en Configuración (este manda).
    ClienteGoogle cliente() const;
    void guardarCliente(const ClienteGoogle& c);
    bool clienteIncluido() const;

    bool conectado();               // hay un token guardado en esta computadora
    std::string cuenta();           // correo de la cuenta conectada
    // Abre el navegador para autorizar. Bloquea hasta terminar (úsese dentro de ejecutarConProgreso).
    void conectar(Avance& avance);
    void desconectar();

    // ---- Archivos (lanzan ErrorRed o std::runtime_error)
    std::optional<ArchivoDrive> buscar(const std::string& nombre, const std::string& carpetaId, bool esCarpeta = false);
    std::vector<ArchivoDrive> listar(const std::string& carpetaId);
    // Busca la carpeta y si no existe la crea. carpetaPadre "root" = Mi unidad.
    std::string carpeta(const std::string& nombre, const std::string& carpetaPadre);
    // Sube datos como archivo nuevo (idExistente vacío) o reemplaza el contenido de uno existente.
    ArchivoDrive subir(const std::string& datos, const std::string& nombre, const std::string& carpetaId,
                       const std::string& idExistente, const std::string& tipo, const Progreso& progreso = {});
    void descargar(const std::string& id, const std::wstring& destino, const Progreso& progreso = {});
    std::string leer(const std::string& id);
    ArchivoDrive copiar(const std::string& id, const std::string& nombre, const std::string& carpetaId);
    void borrar(const std::string& id);

private:
    GoogleDrive() = default;
    std::string token();  // token de acceso vigente (lo renueva si hace falta)
    RespuestaHttp llamar(const std::string& metodo, const std::string& url, const std::string& cuerpo = "",
                         const std::string& tipo = "", const std::wstring& archivo = L"", const Progreso& p = {});
    void cargar();
    void guardar();

    std::recursive_mutex m_;
    bool cargado_ = false;
    std::string refresco_;
    std::string correo_;
    std::string acceso_;
    unsigned long long venceAcceso_ = 0;  // GetTickCount64
};

}  // namespace bu::win
