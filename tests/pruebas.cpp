// Pruebas automáticas del núcleo (cálculos) y del repositorio (base de datos).
// En Linux usan SQLite por ODBC; en Windows usan un archivo de Access temporal,
// así que también sirven para comprobar que el controlador de Access funciona.
//
//   pruebas                      -> ejecuta todo
//   pruebas --nombres            -> imprime los nombres de celda que usa cada plantilla
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "core/Documentos.h"
#include "core/Identificacion.h"
#include "core/NumeroLetras.h"
#include "core/Texto.h"
#include "db/Repositorio.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace bu;

static int g_total = 0;
static int g_fallos = 0;

#define CHECK(cond)                                                                \
    do {                                                                           \
        ++g_total;                                                                 \
        if (!(cond)) {                                                             \
            ++g_fallos;                                                            \
            std::cout << "FALLA " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
        }                                                                          \
    } while (0)

#define CHECK_EQ(a, b)                                                                                    \
    do {                                                                                                  \
        ++g_total;                                                                                        \
        const auto _a = (a);                                                                              \
        const auto _b = (b);                                                                              \
        if (!(_a == _b)) {                                                                                \
            ++g_fallos;                                                                                   \
            std::cout << "FALLA " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b "\n   obtenido: [" \
                      << _a << "]\n   esperado: [" << _b << "]\n";                                         \
        }                                                                                                 \
    } while (0)

#define CHECK_ERROR_NEGOCIO(expr)                                                                       \
    do {                                                                                                \
        ++g_total;                                                                                      \
        bool _lanzo = false;                                                                            \
        try {                                                                                           \
            expr;                                                                                       \
        } catch (const ErrorNegocio&) {                                                                 \
            _lanzo = true;                                                                              \
        }                                                                                               \
        if (!_lanzo) {                                                                                  \
            ++g_fallos;                                                                                 \
            std::cout << "FALLA " << __FILE__ << ":" << __LINE__ << ": se esperaba ErrorNegocio en " #expr "\n"; \
        }                                                                                               \
    } while (0)

// ====================================================================== Núcleo
static void pruebasFecha() {
    CHECK_EQ(Fecha(1970, 1, 1).numeroDia(), 0L);
    CHECK_EQ(Fecha::desdeNumeroDia(Fecha(2026, 11, 21).numeroDia()).iso(), std::string("2026-11-21"));
    CHECK(Fecha(2024, 2, 29).valida());
    CHECK(!Fecha(2023, 2, 29).valida());
    CHECK(!Fecha(2026, 13, 1).valida());
    CHECK_EQ(Fecha(2026, 12, 31).sumarDias(1).iso(), std::string("2027-01-01"));
    CHECK_EQ(Fecha(2024, 3, 1).sumarDias(-1).iso(), std::string("2024-02-29"));
    CHECK_EQ(Fecha(2026, 11, 21).diasHasta(Fecha(2026, 12, 1)), 10);
    CHECK_EQ(Fecha(2026, 11, 21).fechaOle(), 46347.0);
    CHECK_EQ(Fecha(2026, 11, 5).texto(), std::string("05/11/2026"));
    CHECK(Fecha::desdeIso("2026-11-21 00:00:00.000").value() == Fecha(2026, 11, 21));
    CHECK(Fecha::desdeTexto("21/11/2026").value() == Fecha(2026, 11, 21));
    CHECK(!Fecha::desdeIso("hola").has_value());
    CHECK(Fecha(2026, 1, 2) < Fecha(2026, 2, 1));
}

static void pruebasDinero() {
    CHECK_EQ(formatoMonto(123456), std::string("1,234.56"));
    CHECK_EQ(formatoMonto(-5), std::string("-0.05"));
    CHECK_EQ(formatoMonto(0), std::string("0.00"));
    CHECK_EQ(formatoLempiras(100000000), std::string("L 1,000,000.00"));
    CHECK_EQ(formatoLempiras(-150), std::string("-L 1.50"));
    CHECK_EQ(parsearMonto("L 1,234.5").value(), 123450LL);
    CHECK_EQ(parsearMonto("1234.567").value(), 123457LL);
    CHECK_EQ(parsearMonto("-20").value(), -2000LL);
    CHECK_EQ(parsearMonto(".5").value(), 50LL);
    CHECK(!parsearMonto("abc").has_value());
    CHECK(!parsearMonto("").has_value());
    CHECK(!parsearMonto("12.3.4").has_value());
    CHECK_EQ(parsearDecimal("1,250.75").value(), 1250.75);
    CHECK(!parsearDecimal("12a").has_value());
    CHECK_EQ(formatoDecimal(1234.5, 2), std::string("1,234.50"));
    CHECK_EQ(formatoDecimal(-0.001, 2), std::string("0.00"));
    CHECK_EQ(aLibras(2.5, Unidad::Quintales), 250.0);
    CHECK_EQ(aQuintales(125.0), 1.25);
    // 125.5 lb x L 45.75 = 5741.625 -> 5741.63
    CHECK_EQ(calcularSubtotal(125.5, 45.75), 574163LL);
    CHECK_EQ(calcularSubtotal(100, 8.5), 85000LL);
    // Precio por quintal 4567.50 -> 45.675 por libra; 1 qq = 4567.50
    CHECK_EQ(calcularSubtotal(100, 45.675), 456750LL);
    CHECK_EQ(calcularSubtotal(0.1, 0.1), 1LL);
}

static void pruebasIdentificacion() {
    auto r = validarDni("0801199012345", 2026);
    CHECK(r.valido);
    CHECK_EQ(r.formateado, std::string("0801-1990-12345"));
    CHECK(validarDni("0801-1990-12345", 2026).valido);
    CHECK(!validarDni("0801-1990-1234", 2026).valido);
    CHECK(!validarDni("1901-1990-12345", 2026).valido);
    CHECK(!validarDni("0800-1990-12345", 2026).valido);
    CHECK(!validarDni("0801-2099-12345", 2026).valido);
    CHECK(!validarDni("08A1-1990-12345", 2026).valido);
    auto rtn = validarIdentificacion("08019002267079", 2026);
    CHECK(rtn.valido);
    CHECK(rtn.esRtn);
    CHECK_EQ(rtn.formateado, std::string("0801-9002-267079"));
    CHECK(!validarIdentificacion("123", 2026).valido);
}

static void pruebasNumeroLetras() {
    CHECK_EQ(numeroEnLetras(0), std::string("CERO"));
    CHECK_EQ(numeroEnLetras(21), std::string("VEINTIUNO"));
    CHECK_EQ(numeroEnLetras(100), std::string("CIEN"));
    CHECK_EQ(numeroEnLetras(101), std::string("CIENTO UNO"));
    CHECK_EQ(numeroEnLetras(1000), std::string("MIL"));
    CHECK_EQ(numeroEnLetras(21000), std::string("VEINTIÚN MIL"));
    CHECK_EQ(numeroEnLetras(2531001), std::string("DOS MILLONES QUINIENTOS TREINTA Y UN MIL UNO"));
    CHECK_EQ(montoEnLetras(100), std::string("UN LEMPIRA CON 00/100"));
    CHECK_EQ(montoEnLetras(2100), std::string("VEINTIÚN LEMPIRAS CON 00/100"));
    CHECK_EQ(montoEnLetras(457550), std::string("CUATRO MIL QUINIENTOS SETENTA Y CINCO LEMPIRAS CON 50/100"));
    CHECK_EQ(montoEnLetras(100000000), std::string("UN MILLÓN DE LEMPIRAS CON 00/100"));
    CHECK_EQ(montoEnLetras(5), std::string("CERO LEMPIRAS CON 05/100"));
}

static void pruebasTextoYModelos() {
    CHECK_EQ(normalizarBusqueda("José PEÑA Ú"), std::string("jose pena u"));
    CHECK(coincideBusqueda("0801-1990-12345", "08011990"));
    CHECK(coincideBusqueda("María López", "maria lo"));
    CHECK(!coincideBusqueda("Juan Pérez", "pedro"));
    CHECK(coincideBusqueda("cualquiera", "  "));
    CHECK_EQ(recortar("  hola \n"), std::string("hola"));

    Factura f;
    f.clienteId = 1;
    f.fecha = Fecha(2026, 11, 21);
    f.lineas.push_back({CAFE_SECO, 125.5, 0, 45.75, 0});
    f.lineas.push_back({CAFE_UVA, 300, 40, 8.0, 0});
    f.pagado = 700000;
    recalcularFactura(f);
    CHECK_EQ(f.total, 574163LL + 240000LL);
    CHECK_EQ(f.saldo, 814163LL - 700000LL);
    CHECK_EQ(f.totalLibras, 425.5);
    CHECK(validarFactura(f).empty());
    f.pagado = f.total + 1;
    recalcularFactura(f);
    CHECK(!validarFactura(f).empty());

    Deuda d;
    d.monto = d.saldo = 1000;
    const Fecha hoy(2026, 11, 21);
    CHECK_EQ(estadoDeuda(d, hoy), std::string("Pendiente"));
    d.vencimiento = Fecha(2026, 11, 18);
    CHECK_EQ(estadoDeuda(d, hoy), std::string("Vencida hace 3 días"));
    CHECK_EQ(urgenciaDeuda(d, hoy, 3), -1);
    d.vencimiento = hoy;
    CHECK_EQ(estadoDeuda(d, hoy), std::string("Vence hoy"));
    d.vencimiento = Fecha(2026, 11, 23);
    CHECK_EQ(urgenciaDeuda(d, hoy, 3), 1);
    CHECK_EQ(urgenciaDeuda(d, hoy, 1), 2);
    d.saldo = 0;
    CHECK_EQ(estadoDeuda(d, hoy), std::string("Pagada"));

    Empresa e;
    e.rtn = "08019002267079";
    e.telefono = "2222-3333";
    CHECK_EQ(e.lineaDatos(), std::string("RTN 08019002267079  ·  Tel. 2222-3333"));
}

// ====================================================================== Base de datos
static void pruebasRepositorio(db::Conexion& con, db::Dialecto dialecto) {
    db::crearEsquema(con, dialecto);
    db::crearEsquema(con, dialecto);  // debe poder repetirse sin error
    Repositorio repo(con, dialecto);

    const Fecha hoy = Fecha::hoy();
    const Fecha ayer = hoy.sumarDias(-1);
    const Fecha anteayer = hoy.sumarDias(-2);

    // --- Configuración
    CHECK_EQ(repo.config("EmpresaNombre"), std::string("Beneficio Unimax"));
    repo.setConfig("SaldoAperturaCaja", "1000.00");
    repo.setConfig("NumeroInicialFactura", "100");
    repo.setConfig("ClaveNueva", "x");
    CHECK_EQ(repo.config("ClaveNueva"), std::string("x"));
    CHECK_EQ(repo.configEntero("NumeroInicialFactura", 1), 100L);

    // --- Clientes
    Cliente juan;
    juan.nombre = "  Juan Pérez ";
    juan.dni = "0801199012345";
    juan.telefono = "9999-0000";
    repo.guardarCliente(juan);
    CHECK(juan.id > 0);
    CHECK_EQ(juan.dni, std::string("0801-1990-12345"));
    CHECK_EQ(juan.nombre, std::string("Juan Pérez"));
    Cliente duplicado;
    duplicado.nombre = "Otro";
    duplicado.dni = "0801-1990-12345";
    CHECK_ERROR_NEGOCIO(repo.guardarCliente(duplicado));
    Cliente sinDni;
    sinDni.nombre = "Sin DNI";
    sinDni.dni = "123";
    CHECK_ERROR_NEGOCIO(repo.guardarCliente(sinDni));
    CHECK(repo.clientePorDni("0801 1990 12345").has_value());
    CHECK_EQ(repo.clientePorDni("0801199012345")->nombre, std::string("Juan Pérez"));

    Cliente maria = repo.obtenerORegistrarCliente("0703-1985-00456", "María López", "", CLIENTE_PRODUCTOR);
    CHECK(maria.id > 0);
    Cliente maria2 = repo.obtenerORegistrarCliente("0703198500456", "otro nombre", "", CLIENTE_COMPRADOR);
    CHECK_EQ(maria2.id, maria.id);
    CHECK_EQ(repo.clientePorId(maria.id)->tipo, static_cast<int>(CLIENTE_AMBOS));
    Cliente exportadora = repo.obtenerORegistrarCliente("08019002267079", "Exportadora de Café S.A.", "", CLIENTE_COMPRADOR);
    CHECK_EQ(exportadora.dni, std::string("0801-9002-267079"));
    CHECK_EQ(repo.clientes().size(), static_cast<size_t>(3));

    // --- Precios
    PreciosDia p;
    p.fecha = hoy;
    p.precioLibra[CAFE_UVA] = 8.5;
    p.precioLibra[CAFE_HUMEDO] = 30;
    p.precioLibra[CAFE_SECO] = 45.75;
    repo.guardarPrecios(p);
    p.precioLibra[CAFE_SECO] = 46.0;  // corregir el mismo día
    repo.guardarPrecios(p);
    PreciosDia leido = repo.preciosDelDia(hoy);
    CHECK_EQ(leido.precioLibra[CAFE_SECO], 46.0);
    CHECK_EQ(leido.precioLibra[CAFE_UVA], 8.5);
    CHECK(repo.preciosDelDia(ayer).vacio());
    CHECK_EQ(repo.ultimosPrecios(hoy.sumarDias(5)).fecha.iso(), hoy.iso());
    CHECK(repo.ultimosPrecios(ayer).vacio());
    PreciosDia vacio;
    vacio.fecha = hoy;
    CHECK_ERROR_NEGOCIO(repo.guardarPrecios(vacio));
    CHECK_EQ(repo.historialPrecios(hoy.sumarDias(-30), hoy).size(), static_cast<size_t>(1));

    // --- Comprobante de compra con pago parcial
    Factura f;
    f.clienteId = juan.id;
    f.fecha = hoy;
    f.lineas.push_back({CAFE_SECO, 125.5, 0, 46.0, 0});   // 5773.00
    f.lineas.push_back({CAFE_UVA, 300, 40, 8.5, 0});       // 2550.00
    f.pagado = 500000;                                     // paga L 5,000.00
    f.vencimientoSaldo = hoy.sumarDias(2);
    f.observaciones = "Primera entrega";
    repo.registrarFactura(f);
    CHECK(f.id > 0);
    CHECK_EQ(f.numero, 100L);
    CHECK_EQ(f.total, 832300LL);
    CHECK_EQ(f.saldo, 332300LL);
    auto fl = repo.facturaPorId(f.id);
    CHECK(fl.has_value());
    CHECK_EQ(fl->lineas.size(), static_cast<size_t>(2));
    CHECK_EQ(fl->lineas[1].galones, 40.0);
    CHECK_EQ(fl->total, 832300LL);
    CHECK_EQ(fl->totalLibras, 425.5);
    CHECK_EQ(fl->clienteNombre, std::string("Juan Pérez"));
    CHECK(fl->vencimientoSaldo.has_value() && *fl->vencimientoSaldo == hoy.sumarDias(2));
    CHECK_EQ(repo.siguienteNumeroFactura(), 101L);

    // La deuda del beneficio con Juan quedó registrada.
    auto rd = repo.resumenDeudas(juan.id);
    CHECK_EQ(rd.porPagar, 332300LL);
    CHECK_EQ(rd.cantPorPagar, 1);

    // Factura inválida (pagado mayor que total).
    Factura mala = f;
    mala.id = 0;
    mala.pagado = 99999999;
    CHECK_ERROR_NEGOCIO(repo.registrarFactura(mala));

    // --- Préstamo a María (sale de caja)
    Deuda prestamo;
    prestamo.clienteId = maria.id;
    prestamo.tipo = TipoDeuda::PorCobrar;
    prestamo.origen = "Préstamo";
    prestamo.fecha = hoy;
    prestamo.vencimiento = ayer;  // ya vencido (para el recordatorio)
    prestamo.monto = 200000;
    prestamo.concepto = "Préstamo para abono";
    CHECK_ERROR_NEGOCIO(repo.registrarDeuda(prestamo, true));  // vence antes de la fecha
    prestamo.fecha = anteayer;
    repo.registrarDeuda(prestamo, true);
    CHECK(prestamo.id > 0);

    // Abonos al préstamo
    CHECK_ERROR_NEGOCIO(repo.registrarAbono(prestamo.id, hoy, 300000, ""));  // mayor que el saldo
    const long abono1 = repo.registrarAbono(prestamo.id, hoy, 50000, "Primer abono");
    CHECK(abono1 > 0);
    CHECK_EQ(repo.deudaPorId(prestamo.id)->saldo, 150000LL);

    // Abono del beneficio a Juan (saldo de la factura)
    auto deudasJuan = repo.deudas(FiltroDeudas{1, true, juan.id});
    CHECK_EQ(deudasJuan.size(), static_cast<size_t>(1));
    const long deudaFactura = deudasJuan[0].id;
    const long abono2 = repo.registrarAbono(deudaFactura, hoy, 100000, "");
    CHECK_EQ(repo.deudaPorId(deudaFactura)->saldo, 232300LL);

    // No se puede anular la factura con abonos; se anula primero el abono.
    CHECK_ERROR_NEGOCIO(repo.anularFactura(f.id, "Error de peso", hoy));
    CHECK_ERROR_NEGOCIO(repo.anularDeuda(deudaFactura, hoy));
    repo.anularAbono(abono2, hoy);
    CHECK_ERROR_NEGOCIO(repo.anularAbono(abono2, hoy));
    CHECK_EQ(repo.deudaPorId(deudaFactura)->saldo, 332300LL);
    CHECK_EQ(repo.abonos(deudaFactura).size(), static_cast<size_t>(1));

    // --- Venta con cobro parcial
    Venta v;
    v.clienteId = exportadora.id;
    v.fecha = hoy;
    v.tipoCafe = CAFE_SECO;
    v.libras = 1000;
    v.precioLibra = 52.0;   // 52,000.00
    v.recibido = 4000000;   // 40,000.00
    repo.registrarVenta(v);
    CHECK_EQ(v.numero, 1L);
    CHECK_EQ(v.total, 5200000LL);
    CHECK_EQ(v.saldo, 1200000LL);
    CHECK_EQ(repo.ventas(hoy, hoy).size(), static_cast<size_t>(1));
    CHECK_EQ(repo.resumenDeudas(exportadora.id).porCobrar, 1200000LL);

    // --- Caja de hoy
    // Apertura 1000; préstamo de anteayer -2000 => saldo inicial de hoy = -1000.
    CHECK_EQ(repo.saldoInicial(hoy), -100000LL);
    // Hoy: entradas = abono 500 + anulación abono 1000 + venta 40000 = 41500
    //      salidas  = pago factura 5000 + pago a Juan 1000 = 6000
    ResumenCaja rc = repo.resumenCaja(hoy);
    CHECK_EQ(rc.ingresos, 4150000LL);
    CHECK_EQ(rc.egresos, 600000LL);
    CHECK_EQ(rc.esperado, -100000LL + 4150000LL - 600000LL);
    CHECK(!rc.bloqueada);
    CHECK_EQ(repo.movimientos(hoy).size(), static_cast<size_t>(5));

    // Movimiento manual y su eliminación
    const long manual = repo.registrarMovimientoManual(hoy, TipoMovimiento::Ingreso, 1000000, "Fondo del dueño");
    CHECK_EQ(repo.resumenCaja(hoy).ingresos, 5150000LL);
    repo.eliminarMovimientoManual(manual);
    CHECK_EQ(repo.resumenCaja(hoy).ingresos, 4150000LL);
    CHECK_ERROR_NEGOCIO(repo.eliminarMovimientoManual(repo.movimientos(hoy)[0].id));  // no es manual
    CHECK_ERROR_NEGOCIO(repo.registrarMovimientoManual(hoy, TipoMovimiento::Egreso, 0, "x"));

    // Día pendiente de cierre: anteayer tuvo movimiento (préstamo).
    CHECK(repo.diaSinCerrar(hoy).has_value() && *repo.diaSinCerrar(hoy) == anteayer);

    // --- Recordatorio
    Recordatorio rec = repo.recordatorio(hoy);
    CHECK_EQ(rec.vencidas.size(), static_cast<size_t>(1));   // préstamo de María
    CHECK_EQ(rec.proximas.size(), static_cast<size_t>(1));   // saldo de la factura (vence en 2 días)
    CHECK_EQ(rec.venceHoy.size(), static_cast<size_t>(0));
    CHECK_EQ(rec.pendientesSinFecha, 1);                     // saldo de la venta
    CHECK(!rec.faltaPrecioHoy);
    CHECK(rec.hayAlgo());
    CHECK_EQ(rec.totales.porCobrar, 150000LL + 1200000LL);

    // --- Cierres de caja
    CHECK_ERROR_NEGOCIO(repo.cerrarCaja(hoy.sumarDias(1), 0, ""));  // fecha futura
    repo.cerrarCaja(anteayer, 0, "Sin efectivo");                     // contado 0 (esperado -1000)
    auto c1 = repo.cierre(anteayer);
    CHECK(c1.has_value());
    CHECK_EQ(c1->saldoEsperado, -100000LL);
    CHECK_EQ(c1->diferencia, 100000LL);
    CHECK(!repo.diaSinCerrar(hoy).has_value());
    // Ahora el saldo inicial de hoy parte del efectivo contado anteayer (0).
    CHECK_EQ(repo.saldoInicial(hoy), 0LL);
    CHECK(repo.cajaBloqueada(ayer) == false);
    CHECK(repo.cajaBloqueada(anteayer));
    CHECK_ERROR_NEGOCIO(repo.registrarMovimientoManual(anteayer, TipoMovimiento::Ingreso, 100, "tarde"));

    repo.cerrarCaja(hoy, 3500000, "");
    CHECK(repo.resumenCaja(hoy).bloqueada);
    CHECK_ERROR_NEGOCIO(repo.registrarAbono(prestamo.id, hoy, 100, ""));
    CHECK_ERROR_NEGOCIO(repo.cerrarCaja(hoy, 0, ""));       // ya cerrada
    CHECK_ERROR_NEGOCIO(repo.reabrirCaja(anteayer));        // no es el último cierre
    CHECK_EQ(repo.saldoInicial(hoy.sumarDias(1)), 3500000LL);
    CHECK_EQ(repo.cierres(anteayer, hoy).size(), static_cast<size_t>(2));
    repo.reabrirCaja(hoy);
    CHECK(!repo.resumenCaja(hoy).bloqueada);

    // --- Anular la factura (devuelve el pago a caja y anula su deuda)
    repo.anularFactura(f.id, "Error de peso", hoy);
    CHECK(repo.facturaPorId(f.id)->anulada);
    CHECK_EQ(repo.resumenDeudas(juan.id).porPagar, 0LL);
    CHECK_EQ(repo.resumenCaja(hoy).ingresos, 4150000LL + 500000LL);
    CHECK_ERROR_NEGOCIO(repo.anularFactura(f.id, "otra vez", hoy));
    CHECK(repo.detalleCompras(hoy, hoy).empty());           // las anuladas no salen en el reporte
    CHECK_EQ(repo.facturas(hoy, hoy).size(), static_cast<size_t>(1));

    // --- Anular la venta
    CHECK_ERROR_NEGOCIO(repo.anularVenta(v.id, "", hoy));   // falta el motivo
    repo.anularVenta(v.id, "Devolución", hoy);
    CHECK_EQ(repo.resumenDeudas(exportadora.id).porCobrar, 0LL);

    // --- Anular préstamo con abonos: primero el abono
    CHECK_ERROR_NEGOCIO(repo.anularDeuda(prestamo.id, hoy));
    repo.anularAbono(abono1, hoy);
    repo.anularDeuda(prestamo.id, hoy);
    CHECK(repo.deudaPorId(prestamo.id)->anulada);
    CHECK(repo.deudas(FiltroDeudas{}).empty());
    CHECK_EQ(repo.deudas(FiltroDeudas{0, false, 0}).size(), static_cast<size_t>(3));

    // --- Eliminar clientes
    Cliente nuevo = repo.obtenerORegistrarCliente("1503-2000-11111", "Cliente sin movimientos", "", CLIENTE_PRODUCTOR);
    CHECK(repo.eliminarCliente(nuevo.id) == ResultadoEliminar::Eliminado);
    CHECK(repo.eliminarCliente(juan.id) == ResultadoEliminar::Desactivado);
    CHECK_EQ(repo.clientes().size(), static_cast<size_t>(2));
    CHECK_EQ(repo.clientes(true).size(), static_cast<size_t>(3));

    // --- Documentos a partir de datos reales
    const Empresa emp = repo.empresa();
    Documento doc = docComprobanteCompra(emp, *repo.facturaPorId(f.id), *repo.clientePorId(juan.id));
    CHECK_EQ(doc.nombreArchivo, std::string("Comprobante_000100"));
    bool letras = false, anulado = false;
    for (const auto& c : doc.celdas) {
        if (c.first == "TOTAL_LETRAS")
            letras = std::get<std::string>(c.second) == "OCHO MIL TRESCIENTOS VEINTITRÉS LEMPIRAS CON 00/100";
        if (c.first == "ESTADO") anulado = std::get<std::string>(c.second) == "ANULADO";
    }
    CHECK(letras);
    CHECK(anulado);
    CHECK_EQ(doc.tablas[0].filas.size(), static_cast<size_t>(2));
    Documento dc = docCierreCaja(emp, repo.resumenCaja(hoy), repo.movimientos(hoy));
    CHECK_EQ(dc.tablas[0].filas.size(), repo.movimientos(hoy).size());
    Documento dv = docReporteVentas(emp, hoy, hoy, repo.ventas(hoy, hoy));
    CHECK(dv.tablas[1].filas.empty());  // la única venta fue anulada
}

static int ejecutarPruebasBD() {
    db::Conexion con;
#ifdef _WIN32
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring ruta = std::wstring(tmp) + L"pruebas_beneficio.accdb";
    DeleteFileW(ruta.c_str());
    const std::string error = db::crearArchivoAccess(ruta);
    if (!error.empty()) {
        std::cout << "No se pudo crear la base de datos de Access: " << error << "\n";
        return 1;
    }
    std::string rutaUtf8 = db::desdeUtf16(std::u16string(reinterpret_cast<const char16_t*>(ruta.c_str())));
    con.abrir(db::cadenaConexionAccess(rutaUtf8));
    pruebasRepositorio(con, db::Dialecto::Access);
    con.cerrar();
    DeleteFileW(ruta.c_str());
#else
    const char* base = std::getenv("TMPDIR");
    std::string ruta = std::string(base ? base : "/tmp") + "/pruebas_beneficio.db";
    std::remove(ruta.c_str());
    con.abrir("Driver=SQLite3;Database=" + ruta + ";");
    pruebasRepositorio(con, db::Dialecto::SQLite);
    con.cerrar();
    std::remove(ruta.c_str());
#endif
    return 0;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc > 1 && std::strcmp(argv[1], "--nombres") == 0) {
        for (const auto& p : nombresPorPlantilla()) {
            std::cout << p.first;
            for (const auto& n : p.second) std::cout << " " << n;
            std::cout << "\n";
        }
        return 0;
    }
    pruebasFecha();
    pruebasDinero();
    pruebasIdentificacion();
    pruebasNumeroLetras();
    pruebasTextoYModelos();
    try {
        if (ejecutarPruebasBD() != 0) return 1;
    } catch (const std::exception& e) {
        std::cout << "EXCEPCIÓN: " << e.what() << "\n";
        return 1;
    }
    std::cout << (g_fallos == 0 ? "OK" : "HAY FALLAS") << ": " << (g_total - g_fallos) << " de " << g_total
              << " comprobaciones correctas.\n";
    return g_fallos == 0 ? 0 : 1;
}
