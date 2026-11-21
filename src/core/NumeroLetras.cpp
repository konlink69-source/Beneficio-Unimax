#include "core/NumeroLetras.h"

#include <cstdio>

namespace bu {

namespace {
const char* const UNIDADES[] = {"",           "UNO",        "DOS",          "TRES",        "CUATRO",
                                "CINCO",      "SEIS",       "SIETE",        "OCHO",        "NUEVE",
                                "DIEZ",       "ONCE",       "DOCE",         "TRECE",       "CATORCE",
                                "QUINCE",     "DIECISÉIS",  "DIECISIETE",   "DIECIOCHO",   "DIECINUEVE",
                                "VEINTE",     "VEINTIUNO",  "VEINTIDÓS",    "VEINTITRÉS",  "VEINTICUATRO",
                                "VEINTICINCO", "VEINTISÉIS", "VEINTISIETE", "VEINTIOCHO",  "VEINTINUEVE"};
const char* const DECENAS[] = {"", "", "", "TREINTA", "CUARENTA", "CINCUENTA", "SESENTA", "SETENTA", "OCHENTA", "NOVENTA"};
const char* const CENTENAS[] = {"",           "CIENTO",      "DOSCIENTOS", "TRESCIENTOS", "CUATROCIENTOS",
                                "QUINIENTOS", "SEISCIENTOS", "SETECIENTOS", "OCHOCIENTOS", "NOVECIENTOS"};

// n entre 1 y 999
std::string menorQueMil(int n, bool apocope) {
    std::string r;
    const int c = n / 100;
    const int resto = n % 100;
    if (c > 0) r = (n == 100) ? "CIEN" : CENTENAS[c];
    if (resto > 0) {
        if (!r.empty()) r += " ";
        if (resto < 30) {
            if (apocope && resto == 1)
                r += "UN";
            else if (apocope && resto == 21)
                r += "VEINTIÚN";
            else
                r += UNIDADES[resto];
        } else {
            r += DECENAS[resto / 10];
            const int u = resto % 10;
            if (u > 0) {
                r += " Y ";
                r += (apocope && u == 1) ? "UN" : UNIDADES[u];
            }
        }
    }
    return r;
}
}  // namespace

std::string numeroEnLetras(long long n, bool apocope) {
    if (n == 0) return "CERO";
    if (n < 0) return "MENOS " + numeroEnLetras(-n, apocope);
    const long long millones = n / 1000000;
    const int miles = static_cast<int>((n / 1000) % 1000);
    const int resto = static_cast<int>(n % 1000);
    std::string r;
    if (millones > 0) r = (millones == 1) ? "UN MILLÓN" : numeroEnLetras(millones, true) + " MILLONES";
    if (miles > 0) {
        if (!r.empty()) r += " ";
        r += (miles == 1) ? "MIL" : menorQueMil(miles, true) + " MIL";
    }
    if (resto > 0) {
        if (!r.empty()) r += " ";
        r += menorQueMil(resto, apocope);
    }
    return r;
}

std::string montoEnLetras(Centavos c) {
    if (c < 0) return "MENOS " + montoEnLetras(-c);
    const long long entero = c / 100;
    const int centavos = static_cast<int>(c % 100);
    std::string r = numeroEnLetras(entero, true);
    if (entero >= 1000000 && entero % 1000000 == 0) r += " DE";
    r += (entero == 1) ? " LEMPIRA" : " LEMPIRAS";
    char buf[16];
    std::snprintf(buf, sizeof buf, " CON %02d/100", centavos);
    return r + buf;
}

}  // namespace bu
