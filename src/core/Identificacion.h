#pragma once
// Validación de DNI hondureño (13 dígitos) y RTN (14 dígitos).
#include <string>

namespace bu {

struct ResultadoIdentificacion {
    bool valido = false;
    bool esRtn = false;
    std::string formateado;  // DNI: 0801-1990-12345   RTN: 0801-1990-123456
    std::string error;
};

std::string soloDigitos(const std::string& s);

// DNI: 4 dígitos de lugar (departamento 01-18 + municipio), 4 del año y 5 correlativos.
ResultadoIdentificacion validarDni(const std::string& entrada, int anioActual);

// Acepta DNI (13 dígitos) o RTN (14 dígitos, para empresas o personas con RTN).
ResultadoIdentificacion validarIdentificacion(const std::string& entrada, int anioActual);

}  // namespace bu
