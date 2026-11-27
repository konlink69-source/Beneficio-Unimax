#pragma once
// Cálculos del panel de inicio: compras de hoy, del mes y de la cosecha, compras por mes
// y los productores que más vendieron en el mes.
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "core/Modelos.h"

namespace bu {

struct ProductorPanel {
    std::string nombre;
    double libras = 0;
    Centavos monto = 0;
};

struct DatosPanel {
    Fecha hoy;
    Fecha inicioCosecha;
    ResumenDia deHoy, delMes, deCosecha;
    // Meses de la cosecha (0 = octubre ... 11 = septiembre).
    std::array<Centavos, 12> montoUva{};
    std::array<Centavos, 12> montoPergamino{};
    int mesActual = 0;  // índice del mes de "hoy"
    std::vector<ProductorPanel> mejoresDelMes;  // hasta 5, de mayor a menor monto
};

// Café en bodega: lo comprado menos lo vendido, en libras, por tipo (índice = tipo de café).
struct Inventario {
    double comprado[NUM_TIPOS_CAFE + 1] = {0, 0, 0, 0};
    double vendido[NUM_TIPOS_CAFE + 1] = {0, 0, 0, 0};
    double existencia(int tipo) const { return comprado[tipo] - vendido[tipo]; }
    double total() const {
        double t = 0;
        for (int i = 1; i <= NUM_TIPOS_CAFE; ++i) t += existencia(i);
        return t;
    }
};

// "filas" = compras desde el inicio de la cosecha hasta hoy.
DatosPanel calcularPanel(const std::vector<FilaCompra>& filas, const Fecha& hoy);
// Períodos de los filtros (Facturas, Ventas, Reportes).
enum Periodo { PERIODO_HOY, PERIODO_SEMANA, PERIODO_MES, PERIODO_MES_ANTERIOR, PERIODO_COSECHA, PERIODO_ANIO,
               PERIODO_ELEGIR, NUM_PERIODOS };
const char* nombrePeriodo(int p);
// Desde y hasta del período (la semana empieza el lunes; la cosecha, el 1 de octubre).
std::pair<Fecha, Fecha> rangoPeriodo(int p, const Fecha& hoy);

// Nombre corto del mes de la cosecha (0 = "Oct").
const char* mesCosecha(int indice);
// Monto corto para los ejes de los gráficos: "L 950", "L 58.5 mil", "L 1.25 M".
std::string montoCorto(Centavos c);
// "3,250 lb" o "1,475.5 lb".
std::string formatoLibras(double libras);

}  // namespace bu
