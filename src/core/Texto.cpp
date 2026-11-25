#include "core/Texto.h"

#include <cctype>

#include "core/Identificacion.h"

namespace bu {

std::string recortar(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string normalizarBusqueda(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            r += static_cast<char>(std::tolower(c));
            continue;
        }
        if (c == 0xC3 && i + 1 < s.size()) {
            const unsigned char d = static_cast<unsigned char>(s[i + 1]);
            char base = 0;
            switch (d) {
                case 0xA1: case 0x81: case 0xA0: case 0x80: base = 'a'; break;  // á Á à À
                case 0xA9: case 0x89: case 0xA8: case 0x88: base = 'e'; break;  // é É è È
                case 0xAD: case 0x8D: case 0xAC: case 0x8C: base = 'i'; break;  // í Í ì Ì
                case 0xB3: case 0x93: case 0xB2: case 0x92: base = 'o'; break;  // ó Ó ò Ò
                case 0xBA: case 0x9A: case 0xBC: case 0x9C: base = 'u'; break;  // ú Ú ü Ü
                case 0xB1: case 0x91: base = 'n'; break;                        // ñ Ñ
                default: break;
            }
            if (base) {
                r += base;
                ++i;
                continue;
            }
        }
        r += static_cast<char>(c);
    }
    return r;
}

bool coincideBusqueda(const std::string& texto, const std::string& filtro) {
    const std::string f = normalizarBusqueda(recortar(filtro));
    if (f.empty()) return true;
    if (normalizarBusqueda(texto).find(f) != std::string::npos) return true;
    // Filtro numérico: comparar solo dígitos (DNI con o sin guiones).
    bool numerico = true;
    for (char ch : f)
        if (!std::isdigit(static_cast<unsigned char>(ch)) && ch != '-' && ch != ' ') numerico = false;
    if (numerico) {
        const std::string df = soloDigitos(f);
        if (!df.empty() && soloDigitos(texto).find(df) != std::string::npos) return true;
    }
    return false;
}

bool tieneLetras(const std::string& texto) {
    for (char ch : texto) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalpha(c) || c >= 0x80) return true;
    }
    return false;
}

std::string nombreCarpetaCliente(const std::string& nombre, const std::string& dni) {
    std::string r;
    for (char ch : recortar(nombre)) {
        // Caracteres no permitidos en nombres de carpeta de Windows.
        if (std::string("\\/:*?\"<>|").find(ch) != std::string::npos || static_cast<unsigned char>(ch) < 32) {
            r += ' ';
            continue;
        }
        r += ch;
    }
    // Sin espacios dobles ni punto o espacio al final.
    std::string limpio;
    for (char ch : r)
        if (!(ch == ' ' && !limpio.empty() && limpio.back() == ' ')) limpio += ch;
    while (!limpio.empty() && (limpio.back() == ' ' || limpio.back() == '.')) limpio.pop_back();
    if (limpio.size() > 80) {
        limpio.resize(80);
        // No dejar una letra con tilde cortada a la mitad (UTF-8).
        while (!limpio.empty() && (static_cast<unsigned char>(limpio.back()) & 0xC0) == 0x80) limpio.pop_back();
        if (!limpio.empty() && static_cast<unsigned char>(limpio.back()) >= 0xC0) limpio.pop_back();
    }
    while (!limpio.empty() && (limpio.back() == ' ' || limpio.back() == '.')) limpio.pop_back();
    if (limpio.empty()) limpio = "Cliente";
    return dni.empty() ? limpio : limpio + " (" + dni + ")";
}

}  // namespace bu
