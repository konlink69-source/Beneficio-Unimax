#pragma once
// Manejo de dinero (en centavos de Lempira) y de pesos (libras / quintales).
#include <optional>
#include <string>

namespace bu {

// Todo el dinero se maneja en centavos para evitar errores de redondeo.
using Centavos = long long;

// En Honduras el quintal de café es de 100 libras.
constexpr double LIBRAS_POR_QUINTAL = 100.0;

enum class Unidad { Libras = 0, Quintales = 1 };

inline double aLibras(double cantidad, Unidad u) {
    return u == Unidad::Quintales ? cantidad * LIBRAS_POR_QUINTAL : cantidad;
}
inline double aQuintales(double libras) { return libras / LIBRAS_POR_QUINTAL; }

// "1,234.56" (negativo: "-1,234.56")
std::string formatoMonto(Centavos c);
// "L 1,234.56"
std::string formatoLempiras(Centavos c);
// Número con separador de miles y la cantidad de decimales indicada.
std::string formatoDecimal(double valor, int decimales);

// Acepta "1234", "1,234.5", "L 1,234.56". Redondea a 2 decimales.
std::optional<Centavos> parsearMonto(const std::string& texto);
// Acepta "125", "1,250.75". No acepta letras.
std::optional<double> parsearDecimal(const std::string& texto);

inline double aLempiras(Centavos c) { return static_cast<double>(c) / 100.0; }
Centavos desdeLempiras(double lempiras);

double redondear(double valor, int decimales);

// Subtotal = libras x precio por libra, redondeado al centavo.
// Las libras se toman con 2 decimales y el precio con 4 decimales.
Centavos calcularSubtotal(double libras, double precioLibra);

}  // namespace bu
