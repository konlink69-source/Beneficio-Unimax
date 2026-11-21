#include "core/Fecha.h"

#include <cstdio>
#include <ctime>

namespace bu {

namespace {
bool bisiesto(int a) { return (a % 4 == 0 && a % 100 != 0) || a % 400 == 0; }

std::tm ahoraLocal() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}
}  // namespace

int diasDelMes(int anio, int mes) {
    static const int dias[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (mes < 1 || mes > 12) return 0;
    return (mes == 2 && bisiesto(anio)) ? 29 : dias[mes - 1];
}

bool Fecha::valida() const {
    return anio >= 1900 && anio <= 9999 && mes >= 1 && mes <= 12 && dia >= 1 &&
           dia <= diasDelMes(anio, mes);
}

Fecha Fecha::hoy() {
    std::tm tm = ahoraLocal();
    return Fecha(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

// Algoritmo "days_from_civil" de Howard Hinnant.
long Fecha::numeroDia() const {
    long y = anio;
    const long m = mes;
    const long d = dia;
    y -= m <= 2 ? 1 : 0;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400;
    const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

Fecha Fecha::desdeNumeroDia(long z) {
    z += 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const long doe = z - era * 146097;
    const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long y = yoe + era * 400;
    const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long mp = (5 * doy + 2) / 153;
    const long d = doy - (153 * mp + 2) / 5 + 1;
    const long m = mp < 10 ? mp + 3 : mp - 9;
    return Fecha(static_cast<int>(y + (m <= 2 ? 1 : 0)), static_cast<int>(m), static_cast<int>(d));
}

std::string Fecha::iso() const {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", anio, mes, dia);
    return buf;
}

std::string Fecha::texto() const {
    if (vacia()) return std::string();
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d/%02d/%04d", dia, mes, anio);
    return buf;
}

std::optional<Fecha> Fecha::desdeIso(const std::string& s) {
    int a = 0, m = 0, d = 0;
    if (s.size() < 10 || std::sscanf(s.c_str(), "%4d-%2d-%2d", &a, &m, &d) != 3) return std::nullopt;
    Fecha f(a, m, d);
    if (!f.valida()) return std::nullopt;
    return f;
}

std::optional<Fecha> Fecha::desdeTexto(const std::string& s) {
    int a = 0, m = 0, d = 0;
    if (std::sscanf(s.c_str(), "%2d/%2d/%4d", &d, &m, &a) != 3) return std::nullopt;
    Fecha f(a, m, d);
    if (!f.valida()) return std::nullopt;
    return f;
}

std::string horaActual() {
    std::tm tm = ahoraLocal();
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

}  // namespace bu
