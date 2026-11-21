#pragma once
// Contenido de los documentos de Excel. Cada plantilla (carpeta "plantillas")
// tiene celdas con nombre (por ejemplo CLIENTE, TOTAL) y celdas de inicio de
// tablas (por ejemplo DETALLE). Aquí solo se arma QUÉ va en cada nombre;
// el módulo de Windows (win/Excel.cpp) se encarga de escribirlo en Excel.
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/Modelos.h"

namespace bu {

using Valor = std::variant<std::monostate, std::string, double, Fecha>;

// Formatos de número de Excel usados en las tablas.
extern const char* const FMT_TEXTO;
extern const char* const FMT_LEMPIRAS;
extern const char* const FMT_PESO;
extern const char* const FMT_PRECIO;
extern const char* const FMT_FECHA;
extern const char* const FMT_ENTERO;

struct TablaDocumento {
    std::string nombre;                  // nombre de la celda de inicio en la plantilla
    std::vector<std::string> formatos;   // un formato por columna
    std::vector<std::vector<Valor>> filas;
};

struct Documento {
    std::string plantilla;      // archivo, por ejemplo "Factura.xlsx"
    std::string subcarpeta;     // carpeta de salida dentro de Documentos
    std::string nombreArchivo;  // sin extensión
    std::vector<std::pair<std::string, Valor>> celdas;
    std::vector<TablaDocumento> tablas;
};

Documento docComprobanteCompra(const Empresa& e, const Factura& f, const Cliente& c);
Documento docCierreCaja(const Empresa& e, const ResumenCaja& r, const std::vector<MovimientoCaja>& movimientos);
Documento docEstadoCuenta(const Empresa& e, const Cliente& c, const std::vector<Deuda>& deudas,
                          const std::vector<std::vector<Abono>>& abonosPorDeuda, const Fecha& hoy);
Documento docDeudasPendientes(const Empresa& e, const std::vector<Deuda>& deudas, const Fecha& hoy);
Documento docReporteCompras(const Empresa& e, const Fecha& desde, const Fecha& hasta,
                            const std::vector<Factura>& facturas, const std::vector<FilaCompra>& filas);
Documento docReporteVentas(const Empresa& e, const Fecha& desde, const Fecha& hasta, const std::vector<Venta>& ventas);
Documento docReporteCaja(const Empresa& e, const Fecha& desde, const Fecha& hasta, const std::vector<CierreCaja>& cierres);

// Todos los nombres de celda que usa cada plantilla (para verificar las plantillas).
std::vector<std::pair<std::string, std::vector<std::string>>> nombresPorPlantilla();

}  // namespace bu
