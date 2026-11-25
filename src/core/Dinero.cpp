#include "core/Dinero.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace bu {

namespace {
std::string agruparMiles(const std::string& entero) {
    std::string r;
    const int n = static_cast<int>(entero.size());
    for (int i = 0; i < n; ++i) {
        r += entero[i];
        const int resto = n - 1 - i;
        if (resto > 0 && resto % 3 == 0) r += ',';
    }
    return r;
}

std::string limpiarNumero(const std::string& s, bool quitarL) {
    std::string t;
    for (char ch : s) {
        if (ch == ' ' || ch == ',' || ch == '\t' || ch == '\r' || ch == '\n') continue;
        if (quitarL && (ch == 'L' || ch == 'l')) continue;
        t += ch;
    }
    return t;
}
}  // namespace

std::string formatoMonto(Centavos c) {
    const bool negativo = c < 0;
    const unsigned long long a =
        negativo ? static_cast<unsigned long long>(-(c + 1)) + 1ULL : static_cast<unsigned long long>(c);
    char dec[8];
    std::snprintf(dec, sizeof dec, "%02llu", a % 100ULL);
    return std::string(negativo ? "-" : "") + agruparMiles(std::to_string(a / 100ULL)) + "." + dec;
}

std::string formatoLempiras(Centavos c) {
    if (c < 0) return "-L " + formatoMonto(-c);
    return "L " + formatoMonto(c);
}

std::string formatoDecimal(double valor, int decimales) {
    char buf[64];
    // El pequeño ajuste evita que 1.255 (guardado como 1.25499...) se redondee hacia abajo.
    std::snprintf(buf, sizeof buf, "%.*f", decimales, std::fabs(valor) * (1 + 1e-12) + 1e-9);
    std::string s(buf);
    const auto punto = s.find('.');
    std::string entero = punto == std::string::npos ? s : s.substr(0, punto);
    std::string fraccion = punto == std::string::npos ? std::string() : s.substr(punto);
    bool esCero = true;
    for (char ch : s)
        if (ch >= '1' && ch <= '9') esCero = false;
    return std::string(valor < 0 && !esCero ? "-" : "") + agruparMiles(entero) + fraccion;
}

std::optional<Centavos> parsearMonto(const std::string& texto) {
    const std::string t = limpiarNumero(texto, true);
    if (t.empty()) return std::nullopt;
    size_t i = 0;
    bool negativo = false;
    if (t[0] == '-') {
        negativo = true;
        i = 1;
    }
    unsigned long long entero = 0;
    bool hayDigitos = false;
    for (; i < t.size() && std::isdigit(static_cast<unsigned char>(t[i])); ++i) {
        entero = entero * 10 + static_cast<unsigned long long>(t[i] - '0');
        hayDigitos = true;
        if (entero > 100000000000ULL) return std::nullopt;  // límite de seguridad
    }
    int centavos = 0;
    int decimalesLeidos = 0;
    bool redondearArriba = false;
    if (i < t.size() && t[i] == '.') {
        ++i;
        for (; i < t.size() && std::isdigit(static_cast<unsigned char>(t[i])); ++i) {
            const int d = t[i] - '0';
            if (decimalesLeidos < 2) {
                centavos = centavos * 10 + d;
            } else if (decimalesLeidos == 2) {
                redondearArriba = d >= 5;
            }
            ++decimalesLeidos;
            hayDigitos = true;
        }
    }
    if (i != t.size() || !hayDigitos) return std::nullopt;
    if (decimalesLeidos == 1) centavos *= 10;
    Centavos r = static_cast<Centavos>(entero) * 100 + centavos + (redondearArriba ? 1 : 0);
    return negativo ? -r : r;
}

std::optional<double> parsearDecimal(const std::string& texto) {
    const std::string t = limpiarNumero(texto, false);
    if (t.empty()) return std::nullopt;
    int puntos = 0;
    int digitos = 0;
    for (size_t i = 0; i < t.size(); ++i) {
        const char ch = t[i];
        if (ch == '-' && i == 0) continue;
        if (ch == '.') {
            if (++puntos > 1) return std::nullopt;
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(ch))) return std::nullopt;
        ++digitos;
    }
    if (digitos == 0) return std::nullopt;
    return std::strtod(t.c_str(), nullptr);
}

Centavos desdeLempiras(double lempiras) { return static_cast<Centavos>(std::llround(lempiras * 100.0)); }

double redondear(double valor, int decimales) {
    const double f = std::pow(10.0, decimales);
    return static_cast<double>(std::llround(valor * f)) / f;
}

Centavos calcularSubtotal(double libras, double precioLibra) {
    const long long centesimasLibra = std::llround(libras * 100.0);
    const long long precio4 = std::llround(precioLibra * 10000.0);
    const long long producto = centesimasLibra * precio4;  // en millonésimas de Lempira
    if (producto >= 0) return (producto + 5000) / 10000;
    return -((-producto + 5000) / 10000);
}

}  // namespace bu
