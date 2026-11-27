#pragma once
// Reglas de la sincronización con la nube (Google Drive) y de las actualizaciones automáticas.
// Aquí solo está la lógica (sin red ni ventanas) para poder probarla.
//
// En Drive, dentro de la carpeta "Beneficio Unimax":
//   BeneficioUnimax.accdb   la base de datos más reciente (clientes, facturas, caja, usuarios y contraseñas)
//   estado.json             versión de los datos, quién los subió y qué equipo los tiene abiertos
//   Respaldos\              copias con fecha (al cerrar caja, al salir y cada hora)
//   Documentos\, Plantillas\  copia de los comprobantes PDF, reportes y plantillas (con las anteriores)
#include <cstdint>
#include <string>
#include <vector>

#include "core/Json.h"

namespace bu {

// Segundos desde 1970-01-01 UTC.
using Momento = long long;

// "2026-11-27T21:30:05.123Z" (RFC 3339, como lo devuelve Google) -> segundos. 0 si no se entiende.
Momento momentoDesdeRfc3339(const std::string& s);
std::string momentoRfc3339(Momento m);
// Hora local legible: "27/11/2026 15:30" (desfase = segundos que suma la zona horaria a UTC).
std::string momentoTexto(Momento m, long desfase);

// ------------------------------------------------ Quién tiene abierto el programa
// Un equipo que trabaja con los datos renueva el "latido" cada LATIDO_SEGUNDOS. Si pasa
// BLOQUEO_VENCE_SEGUNDOS sin latido (se apagó la computadora, se fue la luz), el bloqueo vence.
constexpr Momento LATIDO_SEGUNDOS = 5 * 60;
constexpr Momento BLOQUEO_VENCE_SEGUNDOS = 20 * 60;

struct Bloqueo {
    std::string dispositivo;  // identificador único del equipo
    std::string equipo;       // nombre de la computadora
    std::string usuario;
    Momento desde = 0;
    Momento latido = 0;
    bool vacio() const { return dispositivo.empty(); }
};

enum class EstadoBloqueo { Libre, Propio, Ajeno, Vencido };
EstadoBloqueo evaluarBloqueo(const Bloqueo& b, const std::string& miDispositivo, Momento ahora);

// ------------------------------------------------ estado.json (en la nube)
struct EstadoNube {
    long long version = 0;     // sube en 1 cada vez que se suben datos nuevos
    std::string dispositivo;   // quién subió la última versión
    std::string equipo;
    std::string usuario;
    Momento subido = 0;
    int versionPrograma = 0;   // versión del programa que la subió
    Bloqueo bloqueo;
};
Json aJson(const EstadoNube& e);
EstadoNube estadoNubeDesdeJson(const Json& j);

// ------------------------------------------------ nube.json (en esta computadora)
struct EstadoLocal {
    std::string dispositivo;   // se crea una vez por computadora
    long long versionBase = 0; // versión de la nube de la que parten los datos locales
    bool pendiente = false;    // hay cambios locales que todavía no se subieron
    Momento ultimaSubida = 0;
    Momento ultimoRespaldo = 0;
    std::vector<std::string> archivosSubidos;  // "Documentos/..." con tamaño y fecha: "ruta|tamaño|fecha"
};
Json aJson(const EstadoLocal& e);
EstadoLocal estadoLocalDesdeJson(const Json& j);

// ------------------------------------------------ Qué hacer al abrir el programa
enum class AccionAlAbrir {
    Nada,           // los datos locales y los de la nube son los mismos
    PrimeraSubida,  // la nube está vacía: se suben los datos de esta computadora
    Subir,          // hay cambios locales sin subir y nadie más cambió nada
    Descargar,      // otra computadora subió datos más nuevos
    Conflicto,      // cambios aquí y en otra computadora: el usuario elige
};
AccionAlAbrir decidirAlAbrir(const EstadoLocal& local, bool hayDatosEnNube, const EstadoNube& nube,
                             bool hayDatosLocales);

// ------------------------------------------------ Respaldos que se conservan en la nube
// Se guardan todos los de las últimas 48 horas, el último de cada día de los últimos 30 días
// y el último de cada mes de antes. Siempre quedan al menos los 10 más recientes.
struct ArchivoRespaldo {
    std::string id;
    Momento momento = 0;
};
std::vector<std::string> respaldosABorrar(std::vector<ArchivoRespaldo> respaldos, Momento ahora, long desfase);
// "Respaldo 2026-11-27 15.30 (cierre de caja).accdb" en hora local.
std::string nombreRespaldo(Momento m, long desfase, const std::string& motivo);

// ------------------------------------------------ Actualizaciones
// Archivo version.txt publicado junto al programa:
//   version=57
//   sha256=<64 caracteres hexadecimales del .exe>
//   tamano=2532352
//   ---
//   Lo que cambió (texto libre, varias líneas)
struct InfoVersion {
    int numero = 0;
    std::string sha256;
    long long tamano = 0;
    std::string notas;
    bool valida() const { return numero > 0 && sha256.size() == 64; }
};
InfoVersion parsearInfoVersion(const std::string& texto);

// ------------------------------------------------ Utilidades de red
std::string base64Url(const uint8_t* datos, size_t n);
std::string base64(const uint8_t* datos, size_t n);
std::string codificarUrl(const std::string& s);  // %XX para todo lo que no sea letra, número o -._~
std::string decodificarUrl(const std::string& s);
// Valor de un parámetro en "a=1&b=2" (ya decodificado). Vacío si no está.
std::string parametroConsulta(const std::string& consulta, const std::string& nombre);
// PKCE (OAuth): verificador a partir de bytes aleatorios y su "challenge" S256.
std::string verificadorPkce(const std::vector<uint8_t>& aleatorios);
std::string desafioPkce(const std::string& verificador);

}  // namespace bu
