#pragma once
// Fecha simple (sin hora) usada en todo el sistema.
#include <optional>
#include <string>

namespace bu {

struct Fecha {
    int anio = 0;
    int mes = 0;
    int dia = 0;

    Fecha() = default;
    Fecha(int a, int m, int d) : anio(a), mes(m), dia(d) {}

    bool valida() const;
    bool vacia() const { return anio == 0; }

    static Fecha hoy();

    // Número de días desde 1970-01-01 (puede ser negativo).
    long numeroDia() const;
    static Fecha desdeNumeroDia(long n);

    Fecha sumarDias(int n) const { return desdeNumeroDia(numeroDia() + n); }
    // Días desde esta fecha hasta "otra" (positivo si "otra" es posterior).
    int diasHasta(const Fecha& otra) const { return static_cast<int>(otra.numeroDia() - numeroDia()); }

    Fecha primerDiaDelMes() const { return Fecha(anio, mes, 1); }

    std::string iso() const;    // AAAA-MM-DD
    std::string texto() const;  // DD/MM/AAAA

    // Acepta "AAAA-MM-DD" seguido opcionalmente de la hora (formato ODBC).
    static std::optional<Fecha> desdeIso(const std::string& s);
    // Acepta "DD/MM/AAAA".
    static std::optional<Fecha> desdeTexto(const std::string& s);

    // Número de serie de fecha de Excel/OLE (días desde 1899-12-30).
    double fechaOle() const { return static_cast<double>(numeroDia() + 25569); }

    bool operator==(const Fecha& o) const { return anio == o.anio && mes == o.mes && dia == o.dia; }
    bool operator!=(const Fecha& o) const { return !(*this == o); }
    bool operator<(const Fecha& o) const { return numeroDia() < o.numeroDia(); }
    bool operator>(const Fecha& o) const { return o < *this; }
    bool operator<=(const Fecha& o) const { return !(o < *this); }
    bool operator>=(const Fecha& o) const { return !(*this < o); }
};

int diasDelMes(int anio, int mes);

// Hora local actual en formato HH:MM:SS.
std::string horaActual();

}  // namespace bu
