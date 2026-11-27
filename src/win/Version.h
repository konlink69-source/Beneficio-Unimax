#pragma once
// Número de versión del programa. Lo pone GitHub Actions al compilar (número de la compilación);
// en una compilación local queda en 0 y el programa no busca actualizaciones.
#ifndef BU_VERSION
#define BU_VERSION 0
#endif

namespace bu::win {
constexpr int VERSION_PROGRAMA = BU_VERSION;
}  // namespace bu::win
