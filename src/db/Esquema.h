#pragma once
// Creación de las tablas de la base de datos.
#include <string>

#include "db/Odbc.h"

namespace bu::db {

enum class Dialecto { Access, SQLite };

constexpr int VERSION_ESQUEMA = 3;

// Crea las tablas que falten y los datos iniciales. Se puede llamar en cada arranque.
void crearEsquema(Conexion& c, Dialecto d);

// Cadena de conexión ODBC para un archivo de Access (.accdb).
std::string cadenaConexionAccess(const std::string& rutaArchivo);

#ifdef _WIN32
// Crea un archivo .accdb vacío usando el controlador ODBC de Access.
// Devuelve un mensaje de error o vacío si todo salió bien.
std::string crearArchivoAccess(const std::wstring& ruta);
#endif

}  // namespace bu::db
