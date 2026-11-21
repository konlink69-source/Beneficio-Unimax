#include "core/Identificacion.h"

#include <cctype>

namespace bu {

std::string soloDigitos(const std::string& s) {
    std::string r;
    for (char ch : s)
        if (std::isdigit(static_cast<unsigned char>(ch))) r += ch;
    return r;
}

namespace {
bool soloCaracteresPermitidos(const std::string& s) {
    for (char ch : s) {
        if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == ' ' || ch == '.') continue;
        return false;
    }
    return true;
}
}  // namespace

ResultadoIdentificacion validarDni(const std::string& entrada, int anioActual) {
    ResultadoIdentificacion r;
    if (!soloCaracteresPermitidos(entrada)) {
        r.error = "El DNI solo debe tener números (puede escribir guiones).";
        return r;
    }
    const std::string d = soloDigitos(entrada);
    if (d.size() != 13) {
        r.error = "El DNI debe tener 13 dígitos (ejemplo: 0801-1990-12345). Usted escribió " +
                  std::to_string(d.size()) + ".";
        return r;
    }
    const int depto = std::stoi(d.substr(0, 2));
    const int municipio = std::stoi(d.substr(2, 2));
    const int anio = std::stoi(d.substr(4, 4));
    if (depto < 1 || depto > 18) {
        r.error = "Los dos primeros dígitos del DNI (departamento) deben estar entre 01 y 18.";
        return r;
    }
    if (municipio < 1) {
        r.error = "Los dígitos 3 y 4 del DNI (municipio) no pueden ser 00.";
        return r;
    }
    if (anio < 1900 || anio > anioActual) {
        r.error = "El año del DNI (dígitos 5 al 8) no es válido: " + d.substr(4, 4) + ".";
        return r;
    }
    r.valido = true;
    r.formateado = d.substr(0, 4) + "-" + d.substr(4, 4) + "-" + d.substr(8, 5);
    return r;
}

ResultadoIdentificacion validarIdentificacion(const std::string& entrada, int anioActual) {
    const std::string d = soloDigitos(entrada);
    if (d.size() == 14 && soloCaracteresPermitidos(entrada)) {
        ResultadoIdentificacion r;
        r.valido = true;
        r.esRtn = true;
        r.formateado = d.substr(0, 4) + "-" + d.substr(4, 4) + "-" + d.substr(8, 6);
        return r;
    }
    ResultadoIdentificacion r = validarDni(entrada, anioActual);
    if (!r.valido && d.size() != 13)
        r.error = "Escriba un DNI de 13 dígitos (0801-1990-12345) o un RTN de 14 dígitos. Usted escribió " +
                  std::to_string(d.size()) + " dígitos.";
    return r;
}

}  // namespace bu
