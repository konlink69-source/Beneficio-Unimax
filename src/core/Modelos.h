#pragma once
// Estructuras de datos del negocio (beneficio de café).
#include <optional>
#include <string>
#include <vector>

#include "core/Dinero.h"
#include "core/Fecha.h"

namespace bu {

// ---------------------------------------------------------------- Café
enum TipoCafe { CAFE_UVA = 1, CAFE_HUMEDO = 2, CAFE_SECO = 3 };
constexpr int NUM_TIPOS_CAFE = 3;
const char* nombreTipoCafe(int tipo);  // "Uva", "Pergamino húmedo", "Pergamino seco"

// Máximo de líneas en un comprobante de compra (cabe en la plantilla de Excel).
constexpr int MAX_LINEAS_FACTURA = 10;

// ---------------------------------------------------------------- Clientes
enum TipoCliente { CLIENTE_PRODUCTOR = 0, CLIENTE_COMPRADOR = 1, CLIENTE_AMBOS = 2 };
const char* nombreTipoCliente(int tipo);

struct Cliente {
    long id = 0;
    std::string nombre;
    std::string dni;  // DNI (13 dígitos) o RTN (14), con guiones
    std::string telefono;
    std::string direccion;
    int tipo = CLIENTE_PRODUCTOR;
    bool activo = true;
};

// ---------------------------------------------------------------- Precios
struct PreciosDia {
    Fecha fecha;
    // Índice = tipo de café (1..3). 0 = sin precio.
    double precioLibra[NUM_TIPOS_CAFE + 1] = {0, 0, 0, 0};
    bool tiene(int tipo) const { return precioLibra[tipo] > 0; }
    bool vacio() const { return !tiene(1) && !tiene(2) && !tiene(3); }
};

// ---------------------------------------------------------------- Compras
struct LineaFactura {
    int tipoCafe = CAFE_SECO;
    double libras = 0;       // peso en libras (base del pago)
    double galones = 0;      // dato informativo
    double precioLibra = 0;  // Lempiras por libra
    Centavos subtotal = 0;
    double quintales() const { return aQuintales(libras); }
};

struct Factura {  // Comprobante de compra de café
    long id = 0;
    long numero = 0;
    Fecha fecha;
    std::string hora;
    long clienteId = 0;
    std::string clienteNombre;
    std::string clienteDni;
    std::vector<LineaFactura> lineas;
    Centavos total = 0;
    Centavos pagado = 0;  // lo que se pagó en efectivo al momento
    Centavos saldo = 0;   // lo que el beneficio queda debiendo
    std::optional<Fecha> vencimientoSaldo;
    std::string observaciones;
    bool anulada = false;
    double totalLibras = 0;  // usado en listados
};

// Calcula subtotales, total y saldo.
void recalcularFactura(Factura& f);
// Devuelve un mensaje de error o vacío si la factura es válida.
std::string validarFactura(const Factura& f);

// Fila plana para el reporte de compras (una por línea de comprobante).
struct FilaCompra {
    long numero = 0;
    Fecha fecha;
    std::string clienteNombre;
    std::string clienteDni;
    LineaFactura linea;
};

// ---------------------------------------------------------------- Ventas
struct Venta {
    long id = 0;
    long numero = 0;
    Fecha fecha;
    long clienteId = 0;
    std::string clienteNombre;
    std::string clienteDni;
    int tipoCafe = CAFE_SECO;
    double libras = 0;
    double precioLibra = 0;
    Centavos total = 0;
    Centavos recibido = 0;  // efectivo recibido al momento
    Centavos saldo = 0;     // lo que el comprador queda debiendo
    std::optional<Fecha> vencimientoSaldo;
    std::string observaciones;
    bool anulada = false;
};

// ---------------------------------------------------------------- Deudas
enum class TipoDeuda {
    PorPagar = 1,  // El beneficio le debe al cliente/productor
    PorCobrar = 2  // El cliente/productor le debe al beneficio
};
const char* nombreTipoDeuda(TipoDeuda t);       // texto corto para listas
const char* descripcionTipoDeuda(TipoDeuda t);  // texto largo

struct Deuda {
    long id = 0;
    long clienteId = 0;
    std::string clienteNombre;
    std::string clienteDni;
    TipoDeuda tipo = TipoDeuda::PorCobrar;
    std::string origen;  // Compra, Venta, Préstamo, Adelanto, Deuda anterior, Otro
    long facturaId = 0;
    long ventaId = 0;
    Fecha fecha;
    std::optional<Fecha> vencimiento;
    Centavos monto = 0;
    Centavos saldo = 0;
    std::string concepto;
    bool afectoCaja = false;
    bool anulada = false;

    bool pendiente() const { return !anulada && saldo > 0; }
};

// Estado legible: "Vencida hace 3 días", "Vence hoy", "Pagada", ...
std::string estadoDeuda(const Deuda& d, const Fecha& hoy);
// -1 = vencida, 0 = vence hoy, 1 = por vencer (dentro de "dias"), 2 = otra
int urgenciaDeuda(const Deuda& d, const Fecha& hoy, int dias);

struct Abono {
    long id = 0;
    long deudaId = 0;
    Fecha fecha;
    std::string hora;
    Centavos monto = 0;
    std::string nota;
    bool anulado = false;
};

struct ResumenDeudas {
    Centavos porPagar = 0;   // el beneficio debe
    Centavos porCobrar = 0;  // le deben al beneficio
    int cantPorPagar = 0;
    int cantPorCobrar = 0;
};

// ---------------------------------------------------------------- Caja
enum class TipoMovimiento { Ingreso = 1, Egreso = 2 };

struct MovimientoCaja {
    long id = 0;
    Fecha fecha;
    std::string hora;
    TipoMovimiento tipo = TipoMovimiento::Ingreso;
    std::string origen;  // COMPRA, VENTA, ABONO, PRESTAMO, ANULACION, MANUAL
    std::string concepto;
    std::string referencia;
    Centavos monto = 0;
};
std::string nombreOrigenMovimiento(const std::string& origen);

struct CierreCaja {
    Fecha fecha;
    std::string hora;
    Centavos saldoInicial = 0;
    Centavos ingresos = 0;
    Centavos egresos = 0;
    Centavos saldoEsperado = 0;
    Centavos efectivoContado = 0;
    Centavos diferencia = 0;
    std::string notas;
};

struct ResumenCaja {
    Fecha fecha;
    Centavos saldoInicial = 0;  // restante del día anterior
    Centavos ingresos = 0;
    Centavos egresos = 0;
    Centavos esperado = 0;      // saldoInicial + ingresos - egresos
    bool bloqueada = false;     // hay un cierre en esta fecha o posterior
    std::optional<CierreCaja> cierre;
    std::optional<Fecha> cierreAnterior;  // vacío = primer día de uso
};

// ---------------------------------------------------------------- Recordatorio
struct Recordatorio {
    Fecha hoy;
    int diasAnticipacion = 3;
    std::vector<Deuda> vencidas;
    std::vector<Deuda> venceHoy;
    std::vector<Deuda> proximas;
    int pendientesSinFecha = 0;
    ResumenDeudas totales;
    bool faltaPrecioHoy = false;
    std::optional<Fecha> diaSinCerrar;

    bool hayDeudasUrgentes() const { return !vencidas.empty() || !venceHoy.empty() || !proximas.empty(); }
    bool hayAlgo() const { return hayDeudasUrgentes() || faltaPrecioHoy || diaSinCerrar.has_value(); }
    std::string resumenCorto() const;
};

// ---------------------------------------------------------------- Empresa
struct Empresa {
    std::string nombre = "Beneficio Unimax";
    std::string rtn;
    std::string direccion;
    std::string telefono;
    std::string lineaDatos() const;  // "RTN ... · dirección · Tel. ..."
};

}  // namespace bu
