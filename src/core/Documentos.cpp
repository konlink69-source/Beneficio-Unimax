#include "core/Documentos.h"

#include <cstdio>

#include "core/Identificacion.h"
#include "core/NumeroLetras.h"

namespace bu {

const char* const FMT_TEXTO = "@";
const char* const FMT_LEMPIRAS = "\"L \"#,##0.00";
const char* const FMT_PESO = "#,##0.00";
const char* const FMT_PRECIO = "#,##0.00##";
const char* const FMT_FECHA = "dd/mm/yyyy";
const char* const FMT_ENTERO = "0";

namespace {

Valor L(Centavos c) { return aLempiras(c); }
Valor T(const std::string& s) { return s; }
Valor N(double d) { return d; }
Valor F(const Fecha& f) { return f.vacia() ? Valor() : Valor(f); }
Valor FO(const std::optional<Fecha>& f) { return f ? Valor(*f) : Valor(std::string("Sin fecha")); }

std::string numero6(long n) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%06ld", n);
    return buf;
}

std::string rango(const Fecha& desde, const Fecha& hasta) { return desde.iso() + "_a_" + hasta.iso(); }

void encabezado(Documento& d, const Empresa& e) {
    d.celdas.push_back({"EMPRESA", T(e.nombre)});
    d.celdas.push_back({"EMPRESA_DATOS", T(e.lineaDatos())});
}

}  // namespace

// ------------------------------------------------------------------ Comprobante de compra
Documento docComprobanteCompra(const Empresa& e, const Factura& f, const Cliente& c) {
    Documento d;
    d.plantilla = "Factura.xlsx";
    d.subcarpeta = "Comprobantes";
    d.nombreArchivo = "Comprobante_" + numero6(f.numero);
    encabezado(d, e);
    d.celdas.push_back({"NUMERO", T(numero6(f.numero))});
    d.celdas.push_back({"FECHA", F(f.fecha)});
    d.celdas.push_back({"CLIENTE", T(c.nombre)});
    d.celdas.push_back({"DNI", T(c.dni)});
    d.celdas.push_back({"TELEFONO", T(c.telefono)});
    double libras = 0;
    double galones = 0;
    TablaDocumento t;
    t.nombre = "DETALLE";
    t.formatos = {FMT_TEXTO, FMT_PESO, FMT_PESO, FMT_PESO, FMT_PRECIO, FMT_LEMPIRAS};
    for (const auto& l : f.lineas) {
        t.filas.push_back({T(nombreTipoCafe(l.tipoCafe)), N(l.libras), N(l.quintales()),
                           l.galones > 0 ? N(l.galones) : Valor(), N(l.precioLibra), L(l.subtotal)});
        libras += l.libras;
        galones += l.galones;
    }
    d.tablas.push_back(t);
    d.celdas.push_back({"TOTAL_LIBRAS", N(libras)});
    d.celdas.push_back({"TOTAL_QUINTALES", N(aQuintales(libras))});
    d.celdas.push_back({"TOTAL_GALONES", galones > 0 ? N(galones) : Valor()});
    d.celdas.push_back({"TOTAL", L(f.total)});
    d.celdas.push_back({"PAGADO", L(f.pagado)});
    d.celdas.push_back({"SALDO", L(f.saldo)});
    d.celdas.push_back({"VENCE", f.saldo > 0 ? FO(f.vencimientoSaldo) : Valor(std::string("—"))});
    d.celdas.push_back({"TOTAL_LETRAS", T(montoEnLetras(f.total))});
    d.celdas.push_back({"OBSERVACIONES", T(f.observaciones)});
    d.celdas.push_back({"ESTADO", T(f.anulada ? "ANULADO" : "")});
    return d;
}

// ------------------------------------------------------------------ Cierre de caja
Documento docCierreCaja(const Empresa& e, const ResumenCaja& r, const std::vector<MovimientoCaja>& movimientos) {
    Documento d;
    d.plantilla = "CierreCaja.xlsx";
    d.subcarpeta = "Caja";
    d.nombreArchivo = "Caja_" + r.fecha.iso();
    encabezado(d, e);
    d.celdas.push_back({"FECHA", F(r.fecha)});
    d.celdas.push_back({"ESTADO", T(r.cierre ? "CERRADA a las " + r.cierre->hora.substr(0, 5) : "ABIERTA (sin cerrar)")});
    d.celdas.push_back({"SALDO_INICIAL", L(r.cierre ? r.cierre->saldoInicial : r.saldoInicial)});
    d.celdas.push_back({"INGRESOS", L(r.ingresos)});
    d.celdas.push_back({"EGRESOS", L(r.egresos)});
    d.celdas.push_back({"ESPERADO", L(r.cierre ? r.cierre->saldoEsperado : r.esperado)});
    d.celdas.push_back({"CONTADO", r.cierre ? L(r.cierre->efectivoContado) : Valor(std::string("—"))});
    d.celdas.push_back({"DIFERENCIA", r.cierre ? L(r.cierre->diferencia) : Valor(std::string("—"))});
    d.celdas.push_back({"NOTAS", T(r.cierre ? r.cierre->notas : std::string())});
    TablaDocumento t;
    t.nombre = "MOVIMIENTOS";
    t.formatos = {FMT_TEXTO, FMT_TEXTO, FMT_TEXTO, FMT_TEXTO, FMT_TEXTO, FMT_LEMPIRAS, FMT_LEMPIRAS};
    for (const auto& m : movimientos) {
        const bool entrada = m.tipo == TipoMovimiento::Ingreso;
        t.filas.push_back({T(m.hora.substr(0, 5)), T(entrada ? "Entrada" : "Salida"), T(nombreOrigenMovimiento(m.origen)),
                           T(m.concepto), T(m.referencia), entrada ? L(m.monto) : Valor(), entrada ? Valor() : L(m.monto)});
    }
    d.tablas.push_back(t);
    return d;
}

// ------------------------------------------------------------------ Estado de cuenta
Documento docEstadoCuenta(const Empresa& e, const Cliente& c, const std::vector<Deuda>& deudas,
                          const std::vector<std::vector<Abono>>& abonosPorDeuda, const Fecha& hoy) {
    Documento d;
    d.plantilla = "EstadoCuenta.xlsx";
    d.subcarpeta = "Estados de cuenta";
    d.nombreArchivo = "EstadoCuenta_" + soloDigitos(c.dni) + "_" + hoy.iso();
    encabezado(d, e);
    d.celdas.push_back({"FECHA", F(hoy)});
    d.celdas.push_back({"CLIENTE", T(c.nombre)});
    d.celdas.push_back({"DNI", T(c.dni)});
    d.celdas.push_back({"TELEFONO", T(c.telefono)});

    Centavos porPagar = 0;
    Centavos porCobrar = 0;
    TablaDocumento td;
    td.nombre = "DEUDAS";
    td.formatos = {FMT_ENTERO, FMT_FECHA, FMT_TEXTO, FMT_TEXTO, FMT_TEXTO, FMT_FECHA,
                   FMT_LEMPIRAS, FMT_LEMPIRAS, FMT_LEMPIRAS, FMT_TEXTO};
    TablaDocumento ta;
    ta.nombre = "ABONOS";
    ta.formatos = {FMT_FECHA, FMT_ENTERO, FMT_TEXTO, FMT_LEMPIRAS, FMT_TEXTO, FMT_TEXTO};
    for (size_t i = 0; i < deudas.size(); ++i) {
        const Deuda& x = deudas[i];
        Centavos abonado = 0;
        if (i < abonosPorDeuda.size()) {
            for (const auto& a : abonosPorDeuda[i]) {
                if (!a.anulado) abonado += a.monto;
                ta.filas.push_back({F(a.fecha), N(static_cast<double>(x.id)), T(nombreTipoDeuda(x.tipo)), L(a.monto),
                                    T(a.nota), T(a.anulado ? "Anulado" : "Válido")});
            }
        }
        if (x.pendiente()) {
            if (x.tipo == TipoDeuda::PorPagar)
                porPagar += x.saldo;
            else
                porCobrar += x.saldo;
        }
        td.filas.push_back({N(static_cast<double>(x.id)), F(x.fecha), T(nombreTipoDeuda(x.tipo)), T(x.origen), T(x.concepto),
                            x.vencimiento ? Valor(*x.vencimiento) : Valor(), L(x.monto), L(abonado),
                            L(x.anulada ? 0 : x.saldo), T(estadoDeuda(x, hoy))});
    }
    d.tablas.push_back(td);
    d.tablas.push_back(ta);
    d.celdas.push_back({"POR_PAGAR", L(porPagar)});
    d.celdas.push_back({"POR_COBRAR", L(porCobrar)});
    std::string conclusion;
    const Centavos neto = porCobrar - porPagar;
    if (neto > 0)
        conclusion = "En total, el cliente le debe al beneficio " + formatoLempiras(neto) + ".";
    else if (neto < 0)
        conclusion = "En total, el beneficio le debe al cliente " + formatoLempiras(-neto) + ".";
    else
        conclusion = (porPagar == 0) ? "El cliente no tiene saldos pendientes." : "Los saldos a favor y en contra se compensan.";
    d.celdas.push_back({"CONCLUSION", T(conclusion)});
    return d;
}

// ------------------------------------------------------------------ Deudas pendientes
Documento docDeudasPendientes(const Empresa& e, const std::vector<Deuda>& deudas, const Fecha& hoy) {
    Documento d;
    d.plantilla = "ReporteDeudas.xlsx";
    d.subcarpeta = "Reportes";
    d.nombreArchivo = "Deudas_pendientes_" + hoy.iso();
    encabezado(d, e);
    d.celdas.push_back({"FECHA", F(hoy)});
    Centavos porPagar = 0;
    Centavos porCobrar = 0;
    int cantPagar = 0;
    int cantCobrar = 0;
    int vencidas = 0;
    TablaDocumento t;
    t.nombre = "DEUDAS";
    t.formatos = {FMT_ENTERO, FMT_TEXTO, FMT_TEXTO, FMT_TEXTO, FMT_TEXTO, FMT_TEXTO,
                  FMT_FECHA, FMT_FECHA, FMT_LEMPIRAS, FMT_LEMPIRAS, FMT_TEXTO};
    for (const auto& x : deudas) {
        if (!x.pendiente()) continue;
        if (x.tipo == TipoDeuda::PorPagar) {
            porPagar += x.saldo;
            ++cantPagar;
        } else {
            porCobrar += x.saldo;
            ++cantCobrar;
        }
        if (urgenciaDeuda(x, hoy, 0) == -1) ++vencidas;
        t.filas.push_back({N(static_cast<double>(x.id)), T(x.clienteNombre), T(x.clienteDni), T(nombreTipoDeuda(x.tipo)),
                           T(x.origen), T(x.concepto), F(x.fecha), x.vencimiento ? Valor(*x.vencimiento) : Valor(),
                           L(x.monto), L(x.saldo), T(estadoDeuda(x, hoy))});
    }
    d.tablas.push_back(t);
    d.celdas.push_back({"POR_PAGAR", L(porPagar)});
    d.celdas.push_back({"POR_COBRAR", L(porCobrar)});
    d.celdas.push_back({"CANT_POR_PAGAR", N(cantPagar)});
    d.celdas.push_back({"CANT_POR_COBRAR", N(cantCobrar)});
    d.celdas.push_back({"CANT_VENCIDAS", N(vencidas)});
    return d;
}

// ------------------------------------------------------------------ Compras del período
Documento docReporteCompras(const Empresa& e, const Fecha& desde, const Fecha& hasta,
                            const std::vector<Factura>& facturas, const std::vector<FilaCompra>& filas) {
    Documento d;
    d.plantilla = "ReporteCompras.xlsx";
    d.subcarpeta = "Reportes";
    d.nombreArchivo = "Compras_" + rango(desde, hasta);
    encabezado(d, e);
    d.celdas.push_back({"DESDE", F(desde)});
    d.celdas.push_back({"HASTA", F(hasta)});

    int cantidad = 0;
    Centavos total = 0, pagado = 0, saldo = 0;
    for (const auto& f : facturas) {
        if (f.anulada) continue;
        ++cantidad;
        total += f.total;
        pagado += f.pagado;
        saldo += f.saldo;
    }
    double libras[NUM_TIPOS_CAFE + 1] = {0, 0, 0, 0};
    double galones[NUM_TIPOS_CAFE + 1] = {0, 0, 0, 0};
    Centavos monto[NUM_TIPOS_CAFE + 1] = {0, 0, 0, 0};
    TablaDocumento det;
    det.nombre = "DETALLE";
    det.formatos = {FMT_ENTERO, FMT_FECHA, FMT_TEXTO, FMT_TEXTO, FMT_TEXTO,
                    FMT_PESO, FMT_PESO, FMT_PESO, FMT_PRECIO, FMT_LEMPIRAS};
    double totalLibras = 0;
    for (const auto& fc : filas) {
        const int t = fc.linea.tipoCafe;
        if (t >= 1 && t <= NUM_TIPOS_CAFE) {
            libras[t] += fc.linea.libras;
            galones[t] += fc.linea.galones;
            monto[t] += fc.linea.subtotal;
        }
        totalLibras += fc.linea.libras;
        det.filas.push_back({N(static_cast<double>(fc.numero)), F(fc.fecha), T(fc.clienteNombre), T(fc.clienteDni),
                             T(nombreTipoCafe(t)), N(fc.linea.libras), N(fc.linea.quintales()),
                             fc.linea.galones > 0 ? N(fc.linea.galones) : Valor(), N(fc.linea.precioLibra),
                             L(fc.linea.subtotal)});
    }
    TablaDocumento res;
    res.nombre = "RESUMEN";
    res.formatos = {FMT_TEXTO, FMT_PESO, FMT_PESO, FMT_PESO, FMT_PRECIO, FMT_LEMPIRAS};
    for (int t = 1; t <= NUM_TIPOS_CAFE; ++t) {
        const double promedio = libras[t] > 0 ? aLempiras(monto[t]) / libras[t] : 0.0;
        res.filas.push_back({T(nombreTipoCafe(t)), N(libras[t]), N(aQuintales(libras[t])), N(galones[t]),
                             libras[t] > 0 ? N(redondear(promedio, 4)) : Valor(), L(monto[t])});
    }
    d.tablas.push_back(res);
    d.tablas.push_back(det);
    d.celdas.push_back({"CANT_COMPROBANTES", N(cantidad)});
    d.celdas.push_back({"TOTAL_LIBRAS", N(totalLibras)});
    d.celdas.push_back({"TOTAL_QUINTALES", N(aQuintales(totalLibras))});
    d.celdas.push_back({"TOTAL_MONTO", L(total)});
    d.celdas.push_back({"TOTAL_PAGADO", L(pagado)});
    d.celdas.push_back({"TOTAL_SALDO", L(saldo)});
    return d;
}

// ------------------------------------------------------------------ Ventas del período
Documento docReporteVentas(const Empresa& e, const Fecha& desde, const Fecha& hasta, const std::vector<Venta>& ventas) {
    Documento d;
    d.plantilla = "ReporteVentas.xlsx";
    d.subcarpeta = "Reportes";
    d.nombreArchivo = "Ventas_" + rango(desde, hasta);
    encabezado(d, e);
    d.celdas.push_back({"DESDE", F(desde)});
    d.celdas.push_back({"HASTA", F(hasta)});
    int cantidad = 0;
    double totalLibras = 0;
    Centavos total = 0, recibido = 0, saldo = 0;
    double libras[NUM_TIPOS_CAFE + 1] = {0, 0, 0, 0};
    Centavos monto[NUM_TIPOS_CAFE + 1] = {0, 0, 0, 0};
    TablaDocumento det;
    det.nombre = "DETALLE";
    det.formatos = {FMT_ENTERO, FMT_FECHA, FMT_TEXTO, FMT_TEXTO, FMT_TEXTO, FMT_PESO,
                    FMT_PESO, FMT_PRECIO, FMT_LEMPIRAS, FMT_LEMPIRAS, FMT_LEMPIRAS};
    // Del más antiguo al más reciente.
    for (auto it = ventas.rbegin(); it != ventas.rend(); ++it) {
        const Venta& v = *it;
        if (v.anulada) continue;
        ++cantidad;
        totalLibras += v.libras;
        total += v.total;
        recibido += v.recibido;
        saldo += v.saldo;
        if (v.tipoCafe >= 1 && v.tipoCafe <= NUM_TIPOS_CAFE) {
            libras[v.tipoCafe] += v.libras;
            monto[v.tipoCafe] += v.total;
        }
        det.filas.push_back({N(static_cast<double>(v.numero)), F(v.fecha), T(v.clienteNombre), T(v.clienteDni),
                             T(nombreTipoCafe(v.tipoCafe)), N(v.libras), N(aQuintales(v.libras)), N(v.precioLibra),
                             L(v.total), L(v.recibido), L(v.saldo)});
    }
    TablaDocumento res;
    res.nombre = "RESUMEN";
    res.formatos = {FMT_TEXTO, FMT_PESO, FMT_PESO, FMT_LEMPIRAS};
    for (int t = 1; t <= NUM_TIPOS_CAFE; ++t)
        res.filas.push_back({T(nombreTipoCafe(t)), N(libras[t]), N(aQuintales(libras[t])), L(monto[t])});
    d.tablas.push_back(res);
    d.tablas.push_back(det);
    d.celdas.push_back({"CANT_VENTAS", N(cantidad)});
    d.celdas.push_back({"TOTAL_LIBRAS", N(totalLibras)});
    d.celdas.push_back({"TOTAL_QUINTALES", N(aQuintales(totalLibras))});
    d.celdas.push_back({"TOTAL_MONTO", L(total)});
    d.celdas.push_back({"TOTAL_RECIBIDO", L(recibido)});
    d.celdas.push_back({"TOTAL_SALDO", L(saldo)});
    return d;
}

// ------------------------------------------------------------------ Caja del período
Documento docReporteCaja(const Empresa& e, const Fecha& desde, const Fecha& hasta, const std::vector<CierreCaja>& cierres) {
    Documento d;
    d.plantilla = "ReporteCaja.xlsx";
    d.subcarpeta = "Reportes";
    d.nombreArchivo = "Caja_" + rango(desde, hasta);
    encabezado(d, e);
    d.celdas.push_back({"DESDE", F(desde)});
    d.celdas.push_back({"HASTA", F(hasta)});
    Centavos entradas = 0, salidas = 0, diferencias = 0;
    TablaDocumento t;
    t.nombre = "DIAS";
    t.formatos = {FMT_FECHA, FMT_LEMPIRAS, FMT_LEMPIRAS, FMT_LEMPIRAS, FMT_LEMPIRAS, FMT_LEMPIRAS, FMT_LEMPIRAS, FMT_TEXTO};
    for (const auto& c : cierres) {
        entradas += c.ingresos;
        salidas += c.egresos;
        diferencias += c.diferencia;
        t.filas.push_back({F(c.fecha), L(c.saldoInicial), L(c.ingresos), L(c.egresos), L(c.saldoEsperado),
                           L(c.efectivoContado), L(c.diferencia), T(c.notas)});
    }
    d.tablas.push_back(t);
    d.celdas.push_back({"DIAS_CERRADOS", N(static_cast<double>(cierres.size()))});
    d.celdas.push_back({"TOTAL_ENTRADAS", L(entradas)});
    d.celdas.push_back({"TOTAL_SALIDAS", L(salidas)});
    d.celdas.push_back({"TOTAL_DIFERENCIAS", L(diferencias)});
    return d;
}

// ------------------------------------------------------------------ Nombres usados
std::vector<std::pair<std::string, std::vector<std::string>>> nombresPorPlantilla() {
    const Empresa e;
    const Fecha f = Fecha::hoy();
    Factura fac;
    fac.lineas.push_back(LineaFactura());
    std::vector<Documento> docs = {
        docComprobanteCompra(e, fac, Cliente()),
        docCierreCaja(e, ResumenCaja(), {}),
        docEstadoCuenta(e, Cliente(), {}, {}, f),
        docDeudasPendientes(e, {}, f),
        docReporteCompras(e, f, f, {}, {}),
        docReporteVentas(e, f, f, {}),
        docReporteCaja(e, f, f, {}),
    };
    std::vector<std::pair<std::string, std::vector<std::string>>> r;
    for (const auto& d : docs) {
        std::vector<std::string> nombres;
        for (const auto& c : d.celdas) nombres.push_back(c.first);
        for (const auto& t : d.tablas) nombres.push_back(t.nombre);
        r.push_back({d.plantilla, nombres});
    }
    return r;
}

}  // namespace bu
