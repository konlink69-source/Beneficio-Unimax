#pragma once
// Peticiones HTTPS con WinHTTP (incluido en Windows; usa el proxy configurado en el sistema).
#include "win/Util.h"

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace bu::win {

// Sin Internet, servidor caído, tiempo agotado...
class ErrorRed : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct RespuestaHttp {
    int estado = 0;          // 200, 404...
    std::string cuerpo;      // vacío si se guardó en un archivo
    std::string ubicacion;   // encabezado "Location"
    bool ok() const { return estado >= 200 && estado < 300; }
};

using Encabezados = std::vector<std::pair<std::string, std::string>>;
// Progreso: (bytes hechos, total o -1 si no se sabe).
using Progreso = std::function<void(long long, long long)>;

// Lanza ErrorRed si no se pudo hablar con el servidor. Los errores HTTP (4xx, 5xx) vienen en "estado".
// Si "archivoSalida" no está vacío, el cuerpo de la respuesta se guarda allí (solo si el estado es 2xx).
RespuestaHttp peticionHttp(const std::string& metodo, const std::string& url, const Encabezados& encabezados = {},
                           const std::string& cuerpo = "", const std::wstring& archivoSalida = L"",
                           const Progreso& progreso = {});

// Lee y escribe archivos completos (para subir y bajar).
bool leerArchivo(const std::wstring& ruta, std::string& datos);
bool escribirArchivo(const std::wstring& ruta, const std::string& datos);

}  // namespace bu::win
