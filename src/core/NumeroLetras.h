#pragma once
// Convierte cantidades a letras para los comprobantes:
// 4575.50 -> "CUATRO MIL QUINIENTOS SETENTA Y CINCO LEMPIRAS CON 50/100"
#include <string>

#include "core/Dinero.h"

namespace bu {

// apocope = true usa "UN" / "VEINTIÚN" (para "UN LEMPIRA", "VEINTIÚN MIL").
std::string numeroEnLetras(long long n, bool apocope = false);
std::string montoEnLetras(Centavos c);

}  // namespace bu
