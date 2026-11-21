#pragma once
// Utilidades de texto (UTF-8).
#include <string>

namespace bu {

std::string recortar(const std::string& s);
// Minúsculas y sin tildes, para búsquedas: "José Peña" -> "jose pena".
std::string normalizarBusqueda(const std::string& s);
// ¿El texto contiene el filtro? Ignora mayúsculas, tildes y, si el filtro es
// numérico, también los guiones (para buscar DNI con o sin guiones).
bool coincideBusqueda(const std::string& texto, const std::string& filtro);

}  // namespace bu
