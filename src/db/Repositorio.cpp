#include "db/Repositorio.h"

#include <algorithm>
#include <cctype>

#include "core/Identificacion.h"
#include "core/Texto.h"

namespace bu {

using db::Sentencia;

namespace {

const char* const SELECT_DEUDA =
    "SELECT d.Id, d.ClienteId, c.Nombre, c.DNI, d.Tipo, d.Origen, d.FacturaId, d.VentaId, d.Fecha, "
    "d.Vencimiento, d.Monto, d.Saldo, d.Concepto, d.AfectoCaja, d.Anulada "
    "FROM Deudas AS d INNER JOIN Clientes AS c ON d.ClienteId = c.Id";

Deuda leerDeuda(const Sentencia& s) {
    Deuda d;
    d.id = s.entero(1);
    d.clienteId = s.entero(2);
    d.clienteNombre = s.texto(3);
    d.clienteDni = s.texto(4);
    d.tipo = s.entero(5) == 1 ? TipoDeuda::PorPagar : TipoDeuda::PorCobrar;
    d.origen = s.texto(6);
    d.facturaId = s.entero(7);
    d.ventaId = s.entero(8);
    d.fecha = s.fecha(9).value_or(Fecha());
    d.vencimiento = s.fecha(10);
    d.monto = s.dinero(11);
    d.saldo = s.dinero(12);
    d.concepto = s.texto(13);
    d.afectoCaja = s.entero(14) != 0;
    d.anulada = s.entero(15) != 0;
    return d;
}

const char* const SELECT_FACTURA =
    "SELECT f.Id, f.Numero, f.Fecha, f.HoraRegistro, f.ClienteId, c.Nombre, c.DNI, f.Total, f.Pagado, "
    "f.Saldo, f.Observaciones, f.Anulada, "
    "(SELECT SUM(x.Libras) FROM FacturaDetalle AS x WHERE x.FacturaId = f.Id), f.Usuario "
    "FROM Facturas AS f INNER JOIN Clientes AS c ON f.ClienteId = c.Id";

Factura leerFactura(const Sentencia& s) {
    Factura f;
    f.id = s.entero(1);
    f.numero = s.entero(2);
    f.fecha = s.fecha(3).value_or(Fecha());
    f.hora = s.texto(4);
    f.clienteId = s.entero(5);
    f.clienteNombre = s.texto(6);
    f.clienteDni = s.texto(7);
    f.total = s.dinero(8);
    f.pagado = s.dinero(9);
    f.saldo = s.dinero(10);
    f.observaciones = s.texto(11);
    f.anulada = s.entero(12) != 0;
    f.totalLibras = s.decimal(13);
    f.usuario = s.texto(14);
    return f;
}

const char* const SELECT_VENTA =
    "SELECT v.Id, v.Numero, v.Fecha, v.ClienteId, c.Nombre, c.DNI, v.TipoCafeId, v.Libras, v.PrecioLibra, "
    "v.Total, v.Recibido, v.Saldo, v.Observaciones, v.Anulada "
    "FROM Ventas AS v INNER JOIN Clientes AS c ON v.ClienteId = c.Id";

Venta leerVenta(const Sentencia& s) {
    Venta v;
    v.id = s.entero(1);
    v.numero = s.entero(2);
    v.fecha = s.fecha(3).value_or(Fecha());
    v.clienteId = s.entero(4);
    v.clienteNombre = s.texto(5);
    v.clienteDni = s.texto(6);
    v.tipoCafe = static_cast<int>(s.entero(7));
    v.libras = s.decimal(8);
    v.precioLibra = s.decimal(9);
    v.total = s.dinero(10);
    v.recibido = s.dinero(11);
    v.saldo = s.dinero(12);
    v.observaciones = s.texto(13);
    v.anulada = s.entero(14) != 0;
    return v;
}

const char* const SELECT_CIERRE =
    "SELECT Fecha, Hora, SaldoInicial, Ingresos, Egresos, SaldoEsperado, EfectivoContado, Diferencia, Notas "
    "FROM CierresCaja";

CierreCaja leerCierre(const Sentencia& s) {
    CierreCaja c;
    c.fecha = s.fecha(1).value_or(Fecha());
    c.hora = s.texto(2);
    c.saldoInicial = s.dinero(3);
    c.ingresos = s.dinero(4);
    c.egresos = s.dinero(5);
    c.saldoEsperado = s.dinero(6);
    c.efectivoContado = s.dinero(7);
    c.diferencia = s.dinero(8);
    c.notas = s.texto(9);
    return c;
}

int anioActual() { return Fecha::hoy().anio; }

}  // namespace

// ================================================================ Configuración
std::string Repositorio::config(const std::string& clave, const std::string& porDefecto) {
    auto s = c_.preparar("SELECT Valor FROM Configuracion WHERE Clave = ?");
    s.pTexto(clave).ejecutar();
    if (s.siguiente()) return s.texto(1);
    return porDefecto;
}

long Repositorio::configEntero(const std::string& clave, long porDefecto) {
    const std::string v = recortar(config(clave));
    if (v.empty()) return porDefecto;
    try {
        return std::stol(v);
    } catch (...) {
        return porDefecto;
    }
}

void Repositorio::setConfig(const std::string& clave, const std::string& valor) {
    auto u = c_.preparar("UPDATE Configuracion SET Valor = ? WHERE Clave = ?");
    u.pTexto(valor).pTexto(clave).ejecutar();
    if (u.filasAfectadas() == 0)
        c_.preparar("INSERT INTO Configuracion (Clave, Valor) VALUES (?, ?)").pTexto(clave).pTexto(valor).ejecutar();
}

Empresa Repositorio::empresa() {
    Empresa e;
    e.nombre = config("EmpresaNombre", "Beneficio Unimax");
    if (recortar(e.nombre).empty()) e.nombre = "Beneficio Unimax";
    e.rtn = config("EmpresaRTN");
    e.direccion = config("EmpresaDireccion");
    e.telefono = config("EmpresaTelefono");
    return e;
}

long Repositorio::ultimoId() {
    auto s = c_.preparar(d_ == db::Dialecto::Access ? "SELECT @@IDENTITY" : "SELECT last_insert_rowid()");
    s.ejecutar();
    if (!s.siguiente()) throw db::ErrorBD("No se pudo obtener el número del registro nuevo.");
    return s.entero(1);
}

// ================================================================ Clientes
std::vector<Cliente> Repositorio::clientes(bool incluirInactivos) {
    std::string sql = "SELECT Id, Nombre, DNI, Telefono, Direccion, Tipo, Activo FROM Clientes";
    if (!incluirInactivos) sql += " WHERE Activo = 1";
    sql += " ORDER BY Nombre";
    auto s = c_.preparar(sql);
    s.ejecutar();
    std::vector<Cliente> r;
    while (s.siguiente()) {
        Cliente c;
        c.id = s.entero(1);
        c.nombre = s.texto(2);
        c.dni = s.texto(3);
        c.telefono = s.texto(4);
        c.direccion = s.texto(5);
        c.tipo = static_cast<int>(s.entero(6));
        c.activo = s.entero(7) != 0;
        r.push_back(c);
    }
    return r;
}

std::optional<Cliente> Repositorio::clientePorId(long id) {
    auto s = c_.preparar("SELECT Id, Nombre, DNI, Telefono, Direccion, Tipo, Activo FROM Clientes WHERE Id = ?");
    s.pEntero(id).ejecutar();
    if (!s.siguiente()) return std::nullopt;
    Cliente c;
    c.id = s.entero(1);
    c.nombre = s.texto(2);
    c.dni = s.texto(3);
    c.telefono = s.texto(4);
    c.direccion = s.texto(5);
    c.tipo = static_cast<int>(s.entero(6));
    c.activo = s.entero(7) != 0;
    return c;
}

std::optional<Cliente> Repositorio::clientePorDni(const std::string& dni) {
    const auto v = validarIdentificacion(dni, anioActual());
    if (!v.valido) return std::nullopt;
    auto s = c_.preparar("SELECT Id FROM Clientes WHERE DNI = ?");
    s.pTexto(v.formateado).ejecutar();
    if (!s.siguiente()) return std::nullopt;
    return clientePorId(s.entero(1));
}

long Repositorio::guardarCliente(Cliente& c) {
    c.nombre = recortar(c.nombre);
    c.telefono = recortar(c.telefono);
    c.direccion = recortar(c.direccion);
    if (c.nombre.empty()) throw ErrorNegocio("Escriba el nombre del cliente.");
    const auto v = validarIdentificacion(c.dni, anioActual());
    if (!v.valido) throw ErrorNegocio(v.error);
    c.dni = v.formateado;
    if (c.tipo < CLIENTE_PRODUCTOR || c.tipo > CLIENTE_AMBOS) c.tipo = CLIENTE_PRODUCTOR;

    auto dup = c_.preparar("SELECT Id, Nombre FROM Clientes WHERE DNI = ? AND Id <> ?");
    dup.pTexto(c.dni).pEntero(c.id).ejecutar();
    if (dup.siguiente())
        throw ErrorNegocio("Ya existe un cliente con el DNI/RTN " + c.dni + ": " + dup.texto(2) + ".");

    if (c.id == 0) {
        c_.preparar(
              "INSERT INTO Clientes (Nombre, DNI, Telefono, Direccion, Tipo, Activo, FechaRegistro) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)")
            .pTexto(c.nombre)
            .pTexto(c.dni)
            .pTexto(c.telefono)
            .pTexto(c.direccion)
            .pEntero(c.tipo)
            .pEntero(c.activo ? 1 : 0)
            .pFecha(Fecha::hoy())
            .ejecutar();
        c.id = ultimoId();
    } else {
        c_.preparar("UPDATE Clientes SET Nombre = ?, DNI = ?, Telefono = ?, Direccion = ?, Tipo = ?, Activo = ? WHERE Id = ?")
            .pTexto(c.nombre)
            .pTexto(c.dni)
            .pTexto(c.telefono)
            .pTexto(c.direccion)
            .pEntero(c.tipo)
            .pEntero(c.activo ? 1 : 0)
            .pEntero(c.id)
            .ejecutar();
    }
    return c.id;
}

ResultadoEliminar Repositorio::eliminarCliente(long id) {
    long usos = 0;
    for (const char* tabla : {"Facturas", "Ventas", "Deudas"}) {
        auto s = c_.preparar(std::string("SELECT COUNT(*) FROM ") + tabla + " WHERE ClienteId = ?");
        s.pEntero(id).ejecutar();
        if (s.siguiente()) usos += s.entero(1);
    }
    if (usos > 0) {
        c_.preparar("UPDATE Clientes SET Activo = 0 WHERE Id = ?").pEntero(id).ejecutar();
        return ResultadoEliminar::Desactivado;
    }
    c_.preparar("DELETE FROM Clientes WHERE Id = ?").pEntero(id).ejecutar();
    return ResultadoEliminar::Eliminado;
}

Cliente Repositorio::obtenerORegistrarCliente(const std::string& dni, const std::string& nombre,
                                              const std::string& telefono, int tipo) {
    const auto v = validarIdentificacion(dni, anioActual());
    if (!v.valido) throw ErrorNegocio(v.error);
    if (auto c = clientePorDni(v.formateado)) {
        bool cambiar = !c->activo;
        c->activo = true;
        if ((tipo == CLIENTE_PRODUCTOR && c->tipo == CLIENTE_COMPRADOR) ||
            (tipo == CLIENTE_COMPRADOR && c->tipo == CLIENTE_PRODUCTOR)) {
            c->tipo = CLIENTE_AMBOS;
            cambiar = true;
        }
        if (c->telefono.empty() && !recortar(telefono).empty()) {
            c->telefono = telefono;
            cambiar = true;
        }
        if (cambiar) guardarCliente(*c);
        return *c;
    }
    Cliente n;
    n.dni = v.formateado;
    n.nombre = nombre;
    n.telefono = telefono;
    n.tipo = tipo;
    guardarCliente(n);
    return n;
}

// ================================================================ Precios
void Repositorio::guardarPrecios(const PreciosDia& p) {
    if (!p.fecha.valida()) throw ErrorNegocio("La fecha no es válida.");
    for (int t = 1; t <= NUM_TIPOS_CAFE; ++t)
        if (p.precioLibra[t] < 0) throw ErrorNegocio("Los precios no pueden ser negativos.");
    if (p.vacio()) throw ErrorNegocio("Escriba al menos un precio.");
    db::Transaccion tx(c_);
    for (int t = 1; t <= NUM_TIPOS_CAFE; ++t) {
        c_.preparar("DELETE FROM PreciosDia WHERE Fecha = ? AND TipoCafeId = ?").pFecha(p.fecha).pEntero(t).ejecutar();
        if (p.tiene(t))
            c_.preparar("INSERT INTO PreciosDia (Fecha, TipoCafeId, PrecioLibra) VALUES (?, ?, ?)")
                .pFecha(p.fecha)
                .pEntero(t)
                .pDecimal(redondear(p.precioLibra[t], 4))
                .ejecutar();
    }
    tx.confirmar();
}

PreciosDia Repositorio::preciosDelDia(const Fecha& f) {
    PreciosDia p;
    p.fecha = f;
    auto s = c_.preparar("SELECT TipoCafeId, PrecioLibra FROM PreciosDia WHERE Fecha = ?");
    s.pFecha(f).ejecutar();
    while (s.siguiente()) {
        const long t = s.entero(1);
        if (t >= 1 && t <= NUM_TIPOS_CAFE) p.precioLibra[t] = s.decimal(2);
    }
    return p;
}

PreciosDia Repositorio::ultimosPrecios(const Fecha& f) {
    auto s = c_.preparar("SELECT MAX(Fecha) FROM PreciosDia WHERE Fecha <= ?");
    s.pFecha(f).ejecutar();
    if (s.siguiente()) {
        if (auto fecha = s.fecha(1)) return preciosDelDia(*fecha);
    }
    return PreciosDia();
}

std::vector<PreciosDia> Repositorio::historialPrecios(const Fecha& desde, const Fecha& hasta) {
    auto s = c_.preparar(
        "SELECT Fecha, TipoCafeId, PrecioLibra FROM PreciosDia WHERE Fecha >= ? AND Fecha <= ? "
        "ORDER BY Fecha DESC, TipoCafeId");
    s.pFecha(desde).pFecha(hasta).ejecutar();
    std::vector<PreciosDia> r;
    while (s.siguiente()) {
        const Fecha f = s.fecha(1).value_or(Fecha());
        if (r.empty() || r.back().fecha != f) {
            r.emplace_back();
            r.back().fecha = f;
        }
        const long t = s.entero(2);
        if (t >= 1 && t <= NUM_TIPOS_CAFE) r.back().precioLibra[t] = s.decimal(3);
    }
    return r;
}

// ================================================================ Compras
long Repositorio::siguienteNumeroFactura() {
    auto s = c_.preparar("SELECT MAX(Numero) FROM Facturas");
    s.ejecutar();
    long n = 0;
    if (s.siguiente()) n = s.entero(1);
    return std::max(n + 1, configEntero("NumeroInicialFactura", 1));
}

long Repositorio::registrarFactura(Factura& f) {
    recalcularFactura(f);
    const std::string error = validarFactura(f);
    if (!error.empty()) throw ErrorNegocio(error);
    const auto cliente = clientePorId(f.clienteId);
    if (!cliente) throw ErrorNegocio("El cliente seleccionado no existe.");
    if (f.saldo > 0 && f.vencimientoSaldo && *f.vencimientoSaldo < f.fecha)
        throw ErrorNegocio("La fecha de pago del saldo no puede ser anterior a la fecha del comprobante.");
    if (f.saldo <= 0) f.vencimientoSaldo.reset();
    if (f.pagado > 0) exigirCajaAbierta(f.fecha, "registrar un comprobante con pago en efectivo");

    db::Transaccion tx(c_);
    f.numero = siguienteNumeroFactura();
    f.hora = horaActual();
    f.clienteNombre = cliente->nombre;
    f.clienteDni = cliente->dni;
    f.anulada = false;
    f.observaciones = recortar(f.observaciones);
    c_.preparar(
          "INSERT INTO Facturas (Numero, Fecha, HoraRegistro, ClienteId, Total, Pagado, Saldo, Observaciones, Anulada, "
          "Usuario) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, ?)")
        .pEntero(f.numero)
        .pFecha(f.fecha)
        .pTexto(f.hora)
        .pEntero(f.clienteId)
        .pDinero(f.total)
        .pDinero(f.pagado)
        .pDinero(f.saldo)
        .pTexto(f.observaciones)
        .pTexto(usuario_)
        .ejecutar();
    f.id = ultimoId();
    f.usuario = usuario_;
    for (const auto& l : f.lineas) {
        c_.preparar(
              "INSERT INTO FacturaDetalle (FacturaId, TipoCafeId, Libras, Galones, PrecioLibra, Subtotal) "
              "VALUES (?, ?, ?, ?, ?, ?)")
            .pEntero(f.id)
            .pEntero(l.tipoCafe)
            .pDecimal(l.libras)
            .pDecimal(l.galones)
            .pDecimal(l.precioLibra)
            .pDinero(l.subtotal)
            .ejecutar();
    }
    const std::string ref = "Comprobante #" + std::to_string(f.numero);
    if (f.saldo > 0) {
        Deuda d;
        d.clienteId = f.clienteId;
        d.tipo = TipoDeuda::PorPagar;
        d.origen = "Compra";
        d.facturaId = f.id;
        d.fecha = f.fecha;
        d.vencimiento = f.vencimientoSaldo;
        d.monto = f.saldo;
        d.saldo = f.saldo;
        d.concepto = "Saldo pendiente del comprobante de compra #" + std::to_string(f.numero);
        insertarDeuda(d);
    }
    if (f.pagado > 0)
        insertarMovimiento(f.fecha, TipoMovimiento::Egreso, "COMPRA", "Pago de café a " + cliente->nombre, ref, f.pagado);
    tx.confirmar();
    return f.id;
}

std::optional<Factura> Repositorio::facturaPorId(long id) {
    auto s = c_.preparar(std::string(SELECT_FACTURA) + " WHERE f.Id = ?");
    s.pEntero(id).ejecutar();
    if (!s.siguiente()) return std::nullopt;
    Factura f = leerFactura(s);
    auto l = c_.preparar(
        "SELECT TipoCafeId, Libras, Galones, PrecioLibra, Subtotal FROM FacturaDetalle WHERE FacturaId = ? ORDER BY Id");
    l.pEntero(id).ejecutar();
    while (l.siguiente()) {
        LineaFactura lf;
        lf.tipoCafe = static_cast<int>(l.entero(1));
        lf.libras = l.decimal(2);
        lf.galones = l.decimal(3);
        lf.precioLibra = l.decimal(4);
        lf.subtotal = l.dinero(5);
        f.lineas.push_back(lf);
    }
    if (auto d = deudaDeComprobante("FacturaId", id)) f.vencimientoSaldo = d->vencimiento;
    return f;
}

std::vector<Factura> Repositorio::facturas(const Fecha& desde, const Fecha& hasta) {
    auto s = c_.preparar(std::string(SELECT_FACTURA) + " WHERE f.Fecha >= ? AND f.Fecha <= ? ORDER BY f.Numero DESC");
    s.pFecha(desde).pFecha(hasta).ejecutar();
    std::vector<Factura> r;
    while (s.siguiente()) r.push_back(leerFactura(s));
    return r;
}

std::vector<FilaCompra> Repositorio::detalleCompras(const Fecha& desde, const Fecha& hasta) {
    auto s = c_.preparar(
        "SELECT f.Numero, f.Fecha, c.Nombre, c.DNI, d.TipoCafeId, d.Libras, d.Galones, d.PrecioLibra, d.Subtotal "
        "FROM (Facturas AS f INNER JOIN Clientes AS c ON f.ClienteId = c.Id) "
        "INNER JOIN FacturaDetalle AS d ON d.FacturaId = f.Id "
        "WHERE f.Anulada = 0 AND f.Fecha >= ? AND f.Fecha <= ? ORDER BY f.Numero, d.Id");
    s.pFecha(desde).pFecha(hasta).ejecutar();
    std::vector<FilaCompra> r;
    while (s.siguiente()) {
        FilaCompra fc;
        fc.numero = s.entero(1);
        fc.fecha = s.fecha(2).value_or(Fecha());
        fc.clienteNombre = s.texto(3);
        fc.clienteDni = s.texto(4);
        fc.linea.tipoCafe = static_cast<int>(s.entero(5));
        fc.linea.libras = s.decimal(6);
        fc.linea.galones = s.decimal(7);
        fc.linea.precioLibra = s.decimal(8);
        fc.linea.subtotal = s.dinero(9);
        r.push_back(fc);
    }
    return r;
}

void Repositorio::anularFactura(long id, const std::string& motivo, const Fecha& hoy) {
    const auto f = facturaPorId(id);
    if (!f) throw ErrorNegocio("El comprobante no existe.");
    if (f->anulada) throw ErrorNegocio("El comprobante #" + std::to_string(f->numero) + " ya está anulado.");
    if (recortar(motivo).empty()) throw ErrorNegocio("Escriba el motivo de la anulación.");
    const auto d = deudaDeComprobante("FacturaId", id);
    if (d && !d->anulada && abonosActivos(d->id) > 0)
        throw ErrorNegocio(
            "El saldo de este comprobante ya tiene abonos registrados. "
            "Anule primero esos abonos en la pestaña Deudas.");
    if (f->pagado > 0) exigirCajaAbierta(hoy, "anular el comprobante");

    db::Transaccion tx(c_);
    c_.preparar("UPDATE Facturas SET Anulada = 1, FechaAnulacion = ?, MotivoAnulacion = ? WHERE Id = ?")
        .pFecha(hoy)
        .pTexto(recortar(motivo))
        .pEntero(id)
        .ejecutar();
    if (d && !d->anulada) c_.preparar("UPDATE Deudas SET Anulada = 1 WHERE Id = ?").pEntero(d->id).ejecutar();
    const std::string ref = "Comprobante #" + std::to_string(f->numero);
    if (f->pagado > 0)
        insertarMovimiento(hoy, TipoMovimiento::Ingreso, "ANULACION",
                           "Anulación del comprobante #" + std::to_string(f->numero) + " (devolución del pago)", ref,
                           f->pagado);
    tx.confirmar();
}

// ================================================================ Ventas
long Repositorio::siguienteNumeroVenta() {
    auto s = c_.preparar("SELECT MAX(Numero) FROM Ventas");
    s.ejecutar();
    long n = 0;
    if (s.siguiente()) n = s.entero(1);
    return std::max(n + 1, configEntero("NumeroInicialVenta", 1));
}

long Repositorio::registrarVenta(Venta& v) {
    v.libras = redondear(v.libras, 2);
    v.precioLibra = redondear(v.precioLibra, 4);
    v.total = calcularSubtotal(v.libras, v.precioLibra);
    v.saldo = v.total - v.recibido;
    const auto cliente = clientePorId(v.clienteId);
    if (!cliente) throw ErrorNegocio("Seleccione o registre el comprador.");
    if (!v.fecha.valida()) throw ErrorNegocio("La fecha de la venta no es válida.");
    if (v.tipoCafe < 1 || v.tipoCafe > NUM_TIPOS_CAFE) throw ErrorNegocio("Seleccione el tipo de café.");
    if (v.libras <= 0) throw ErrorNegocio("Escriba el peso vendido.");
    if (v.precioLibra <= 0) throw ErrorNegocio("Escriba el precio por libra.");
    if (v.recibido < 0) throw ErrorNegocio("El monto recibido no puede ser negativo.");
    if (v.recibido > v.total) throw ErrorNegocio("El monto recibido no puede ser mayor que el total de la venta.");
    if (v.saldo > 0 && v.vencimientoSaldo && *v.vencimientoSaldo < v.fecha)
        throw ErrorNegocio("La fecha de cobro del saldo no puede ser anterior a la fecha de la venta.");
    if (v.recibido > 0) exigirCajaAbierta(v.fecha, "registrar una venta cobrada en efectivo");

    db::Transaccion tx(c_);
    v.numero = siguienteNumeroVenta();
    v.clienteNombre = cliente->nombre;
    v.clienteDni = cliente->dni;
    v.anulada = false;
    v.observaciones = recortar(v.observaciones);
    c_.preparar(
          "INSERT INTO Ventas (Numero, Fecha, ClienteId, TipoCafeId, Libras, PrecioLibra, Total, Recibido, Saldo, "
          "Observaciones, Anulada, Usuario) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?)")
        .pEntero(v.numero)
        .pFecha(v.fecha)
        .pEntero(v.clienteId)
        .pEntero(v.tipoCafe)
        .pDecimal(v.libras)
        .pDecimal(v.precioLibra)
        .pDinero(v.total)
        .pDinero(v.recibido)
        .pDinero(v.saldo)
        .pTexto(v.observaciones)
        .pTexto(usuario_)
        .ejecutar();
    v.id = ultimoId();
    if (v.saldo > 0) {
        Deuda d;
        d.clienteId = v.clienteId;
        d.tipo = TipoDeuda::PorCobrar;
        d.origen = "Venta";
        d.ventaId = v.id;
        d.fecha = v.fecha;
        d.vencimiento = v.vencimientoSaldo;
        d.monto = v.saldo;
        d.saldo = v.saldo;
        d.concepto = "Saldo pendiente de la venta #" + std::to_string(v.numero);
        insertarDeuda(d);
    }
    if (v.recibido > 0)
        insertarMovimiento(v.fecha, TipoMovimiento::Ingreso, "VENTA", "Venta de café a " + cliente->nombre,
                           "Venta #" + std::to_string(v.numero), v.recibido);
    tx.confirmar();
    return v.id;
}

std::vector<Venta> Repositorio::ventas(const Fecha& desde, const Fecha& hasta) {
    auto s = c_.preparar(std::string(SELECT_VENTA) + " WHERE v.Fecha >= ? AND v.Fecha <= ? ORDER BY v.Numero DESC");
    s.pFecha(desde).pFecha(hasta).ejecutar();
    std::vector<Venta> r;
    while (s.siguiente()) r.push_back(leerVenta(s));
    for (auto& v : r)
        if (auto d = deudaDeComprobante("VentaId", v.id)) v.vencimientoSaldo = d->vencimiento;
    return r;
}

void Repositorio::anularVenta(long id, const std::string& motivo, const Fecha& hoy) {
    auto s = c_.preparar(std::string(SELECT_VENTA) + " WHERE v.Id = ?");
    s.pEntero(id).ejecutar();
    if (!s.siguiente()) throw ErrorNegocio("La venta no existe.");
    const Venta v = leerVenta(s);
    if (v.anulada) throw ErrorNegocio("La venta #" + std::to_string(v.numero) + " ya está anulada.");
    if (recortar(motivo).empty()) throw ErrorNegocio("Escriba el motivo de la anulación.");
    const auto d = deudaDeComprobante("VentaId", id);
    if (d && !d->anulada && abonosActivos(d->id) > 0)
        throw ErrorNegocio("El saldo de esta venta ya tiene abonos. Anule primero esos abonos en la pestaña Deudas.");
    if (v.recibido > 0) exigirCajaAbierta(hoy, "anular la venta");

    db::Transaccion tx(c_);
    c_.preparar("UPDATE Ventas SET Anulada = 1, FechaAnulacion = ?, MotivoAnulacion = ? WHERE Id = ?")
        .pFecha(hoy)
        .pTexto(recortar(motivo))
        .pEntero(id)
        .ejecutar();
    if (d && !d->anulada) c_.preparar("UPDATE Deudas SET Anulada = 1 WHERE Id = ?").pEntero(d->id).ejecutar();
    if (v.recibido > 0)
        insertarMovimiento(hoy, TipoMovimiento::Egreso, "ANULACION",
                           "Anulación de la venta #" + std::to_string(v.numero) + " (devolución del cobro)",
                           "Venta #" + std::to_string(v.numero), v.recibido);
    tx.confirmar();
}

// ================================================================ Deudas
long Repositorio::insertarDeuda(const Deuda& d) {
    auto s = c_.preparar(
        "INSERT INTO Deudas (ClienteId, Tipo, Origen, FacturaId, VentaId, Fecha, Vencimiento, Monto, Saldo, "
        "Concepto, AfectoCaja, Anulada, Usuario) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    s.pEntero(d.clienteId).pEntero(static_cast<long>(d.tipo)).pTexto(d.origen);
    if (d.facturaId) s.pEntero(d.facturaId); else s.pNulo();
    if (d.ventaId) s.pEntero(d.ventaId); else s.pNulo();
    s.pFecha(d.fecha)
        .pFechaOpc(d.vencimiento)
        .pDinero(d.monto)
        .pDinero(d.saldo)
        .pTexto(d.concepto)
        .pEntero(d.afectoCaja ? 1 : 0)
        .pEntero(d.anulada ? 1 : 0)
        .pTexto(usuario_)
        .ejecutar();
    return ultimoId();
}

long Repositorio::registrarDeuda(Deuda& d, bool afectaCaja) {
    const auto cliente = clientePorId(d.clienteId);
    if (!cliente) throw ErrorNegocio("Seleccione o registre el cliente.");
    if (d.monto <= 0) throw ErrorNegocio("El monto debe ser mayor que cero.");
    if (!d.fecha.valida()) throw ErrorNegocio("La fecha no es válida.");
    if (d.vencimiento && *d.vencimiento < d.fecha)
        throw ErrorNegocio("La fecha de pago no puede ser anterior a la fecha de la deuda.");
    d.origen = recortar(d.origen);
    if (d.origen.empty()) d.origen = "Otro";
    d.concepto = recortar(d.concepto);
    if (d.concepto.empty()) d.concepto = d.origen;
    if (afectaCaja) exigirCajaAbierta(d.fecha, "registrar un préstamo que mueve dinero de caja");

    db::Transaccion tx(c_);
    d.saldo = d.monto;
    d.afectoCaja = afectaCaja;
    d.anulada = false;
    d.facturaId = 0;
    d.ventaId = 0;
    d.clienteNombre = cliente->nombre;
    d.clienteDni = cliente->dni;
    d.id = insertarDeuda(d);
    const std::string ref = "Deuda #" + std::to_string(d.id);
    if (afectaCaja) {
        if (d.tipo == TipoDeuda::PorCobrar)
            insertarMovimiento(d.fecha, TipoMovimiento::Egreso, "PRESTAMO",
                               d.origen + " a " + cliente->nombre + ": " + d.concepto, ref, d.monto);
        else
            insertarMovimiento(d.fecha, TipoMovimiento::Ingreso, "PRESTAMO",
                               "Dinero recibido de " + cliente->nombre + ": " + d.concepto, ref, d.monto);
    }
    tx.confirmar();
    return d.id;
}

std::vector<Deuda> Repositorio::deudas(const FiltroDeudas& filtro) {
    std::string sql = std::string(SELECT_DEUDA) + " WHERE 1 = 1";
    if (filtro.tipo == 1 || filtro.tipo == 2) sql += " AND d.Tipo = ?";
    if (filtro.soloPendientes) sql += " AND d.Anulada = 0 AND d.Saldo > 0";
    if (filtro.clienteId > 0) sql += " AND d.ClienteId = ?";
    sql += " ORDER BY d.Fecha, d.Id";
    auto s = c_.preparar(sql);
    if (filtro.tipo == 1 || filtro.tipo == 2) s.pEntero(filtro.tipo);
    if (filtro.clienteId > 0) s.pEntero(filtro.clienteId);
    s.ejecutar();
    std::vector<Deuda> r;
    while (s.siguiente()) r.push_back(leerDeuda(s));
    // Pendientes primero, ordenadas por fecha de pago (las sin fecha al final).
    std::stable_sort(r.begin(), r.end(), [](const Deuda& a, const Deuda& b) {
        if (a.pendiente() != b.pendiente()) return a.pendiente();
        if (a.pendiente()) {
            if (a.vencimiento.has_value() != b.vencimiento.has_value()) return a.vencimiento.has_value();
            if (a.vencimiento && *a.vencimiento != *b.vencimiento) return *a.vencimiento < *b.vencimiento;
            return false;
        }
        return a.fecha > b.fecha;
    });
    return r;
}

std::optional<Deuda> Repositorio::deudaPorId(long id) {
    auto s = c_.preparar(std::string(SELECT_DEUDA) + " WHERE d.Id = ?");
    s.pEntero(id).ejecutar();
    if (!s.siguiente()) return std::nullopt;
    return leerDeuda(s);
}

std::optional<Deuda> Repositorio::deudaDeComprobante(const char* columna, long id) {
    auto s = c_.preparar(std::string(SELECT_DEUDA) + " WHERE d." + columna + " = ? ORDER BY d.Id DESC");
    s.pEntero(id).ejecutar();
    if (!s.siguiente()) return std::nullopt;
    return leerDeuda(s);
}

int Repositorio::abonosActivos(long deudaId) {
    auto s = c_.preparar("SELECT COUNT(*) FROM Abonos WHERE DeudaId = ? AND Anulado = 0");
    s.pEntero(deudaId).ejecutar();
    return s.siguiente() ? static_cast<int>(s.entero(1)) : 0;
}

long Repositorio::registrarAbono(long deudaId, const Fecha& fecha, Centavos monto, const std::string& nota) {
    const auto d = deudaPorId(deudaId);
    if (!d) throw ErrorNegocio("La deuda no existe.");
    if (d->anulada) throw ErrorNegocio("La deuda está anulada.");
    if (d->saldo <= 0) throw ErrorNegocio("Esta deuda ya está pagada.");
    if (monto <= 0) throw ErrorNegocio("El abono debe ser mayor que cero.");
    if (monto > d->saldo)
        throw ErrorNegocio("El abono (" + formatoLempiras(monto) + ") es mayor que el saldo pendiente (" +
                           formatoLempiras(d->saldo) + ").");
    if (!fecha.valida()) throw ErrorNegocio("La fecha del abono no es válida.");
    exigirCajaAbierta(fecha, "registrar el abono");

    db::Transaccion tx(c_);
    c_.preparar(
          "INSERT INTO Abonos (DeudaId, Fecha, HoraRegistro, Monto, Nota, Anulado, Usuario) VALUES (?, ?, ?, ?, ?, 0, ?)")
        .pEntero(deudaId)
        .pFecha(fecha)
        .pTexto(horaActual())
        .pDinero(monto)
        .pTexto(recortar(nota))
        .pTexto(usuario_)
        .ejecutar();
    const long abonoId = ultimoId();
    c_.preparar("UPDATE Deudas SET Saldo = ? WHERE Id = ?").pDinero(d->saldo - monto).pEntero(deudaId).ejecutar();
    const std::string ref = "Deuda #" + std::to_string(deudaId);
    if (d->tipo == TipoDeuda::PorCobrar)
        insertarMovimiento(fecha, TipoMovimiento::Ingreso, "ABONO", "Abono de " + d->clienteNombre + " a su deuda", ref,
                           monto);
    else
        insertarMovimiento(fecha, TipoMovimiento::Egreso, "ABONO", "Pago a " + d->clienteNombre + " (deuda del beneficio)",
                           ref, monto);
    tx.confirmar();
    return abonoId;
}

std::vector<Abono> Repositorio::abonos(long deudaId) {
    auto s = c_.preparar(
        "SELECT Id, DeudaId, Fecha, HoraRegistro, Monto, Nota, Anulado FROM Abonos WHERE DeudaId = ? ORDER BY Fecha, Id");
    s.pEntero(deudaId).ejecutar();
    std::vector<Abono> r;
    while (s.siguiente()) {
        Abono a;
        a.id = s.entero(1);
        a.deudaId = s.entero(2);
        a.fecha = s.fecha(3).value_or(Fecha());
        a.hora = s.texto(4);
        a.monto = s.dinero(5);
        a.nota = s.texto(6);
        a.anulado = s.entero(7) != 0;
        r.push_back(a);
    }
    return r;
}

void Repositorio::anularAbono(long abonoId, const Fecha& hoy) {
    auto s = c_.preparar("SELECT DeudaId, Monto, Anulado FROM Abonos WHERE Id = ?");
    s.pEntero(abonoId).ejecutar();
    if (!s.siguiente()) throw ErrorNegocio("El abono no existe.");
    const long deudaId = s.entero(1);
    const Centavos monto = s.dinero(2);
    if (s.entero(3) != 0) throw ErrorNegocio("El abono ya está anulado.");
    const auto d = deudaPorId(deudaId);
    if (!d) throw ErrorNegocio("La deuda del abono no existe.");
    exigirCajaAbierta(hoy, "anular el abono");

    db::Transaccion tx(c_);
    c_.preparar("UPDATE Abonos SET Anulado = 1 WHERE Id = ?").pEntero(abonoId).ejecutar();
    c_.preparar("UPDATE Deudas SET Saldo = ? WHERE Id = ?").pDinero(d->saldo + monto).pEntero(deudaId).ejecutar();
    const std::string ref = "Deuda #" + std::to_string(deudaId);
    if (d->tipo == TipoDeuda::PorCobrar)
        insertarMovimiento(hoy, TipoMovimiento::Egreso, "ANULACION", "Anulación de abono de " + d->clienteNombre, ref,
                           monto);
    else
        insertarMovimiento(hoy, TipoMovimiento::Ingreso, "ANULACION", "Anulación de pago a " + d->clienteNombre, ref,
                           monto);
    tx.confirmar();
}

void Repositorio::anularDeuda(long deudaId, const Fecha& hoy) {
    const auto d = deudaPorId(deudaId);
    if (!d) throw ErrorNegocio("La deuda no existe.");
    if (d->anulada) throw ErrorNegocio("La deuda ya está anulada.");
    if (d->facturaId)
        throw ErrorNegocio(
            "Esta deuda es el saldo de un comprobante de compra. Para quitarla, anule el comprobante en la pestaña "
            "Facturas.");
    if (d->ventaId)
        throw ErrorNegocio("Esta deuda es el saldo de una venta. Para quitarla, anule la venta en la pestaña Ventas.");
    if (abonosActivos(deudaId) > 0) throw ErrorNegocio("La deuda tiene abonos. Anule primero los abonos.");
    if (d->afectoCaja) exigirCajaAbierta(hoy, "anular la deuda");

    db::Transaccion tx(c_);
    c_.preparar("UPDATE Deudas SET Anulada = 1 WHERE Id = ?").pEntero(deudaId).ejecutar();
    const std::string ref = "Deuda #" + std::to_string(deudaId);
    if (d->afectoCaja) {
        if (d->tipo == TipoDeuda::PorCobrar)
            insertarMovimiento(hoy, TipoMovimiento::Ingreso, "ANULACION",
                               "Anulación de préstamo a " + d->clienteNombre + " (devolución)", ref, d->monto);
        else
            insertarMovimiento(hoy, TipoMovimiento::Egreso, "ANULACION",
                               "Anulación de dinero recibido de " + d->clienteNombre + " (devolución)", ref, d->monto);
    }
    tx.confirmar();
}

ResumenDeudas Repositorio::resumenDeudas(long clienteId) {
    std::string sql = "SELECT Tipo, SUM(Saldo), COUNT(*) FROM Deudas WHERE Anulada = 0 AND Saldo > 0";
    if (clienteId > 0) sql += " AND ClienteId = ?";
    sql += " GROUP BY Tipo";
    auto s = c_.preparar(sql);
    if (clienteId > 0) s.pEntero(clienteId);
    s.ejecutar();
    ResumenDeudas r;
    while (s.siguiente()) {
        if (s.entero(1) == 1) {
            r.porPagar = s.dinero(2);
            r.cantPorPagar = static_cast<int>(s.entero(3));
        } else {
            r.porCobrar = s.dinero(2);
            r.cantPorCobrar = static_cast<int>(s.entero(3));
        }
    }
    return r;
}

std::map<long, ResumenDeudas> Repositorio::resumenDeudasPorCliente() {
    auto s = c_.preparar(
        "SELECT ClienteId, Tipo, SUM(Saldo), COUNT(*) FROM Deudas WHERE Anulada = 0 AND Saldo > 0 "
        "GROUP BY ClienteId, Tipo");
    s.ejecutar();
    std::map<long, ResumenDeudas> r;
    while (s.siguiente()) {
        auto& rd = r[s.entero(1)];
        if (s.entero(2) == 1) {
            rd.porPagar = s.dinero(3);
            rd.cantPorPagar = static_cast<int>(s.entero(4));
        } else {
            rd.porCobrar = s.dinero(3);
            rd.cantPorCobrar = static_cast<int>(s.entero(4));
        }
    }
    return r;
}

// ================================================================ Caja
long Repositorio::insertarMovimiento(const Fecha& f, TipoMovimiento t, const std::string& origen,
                                     const std::string& concepto, const std::string& referencia, Centavos monto) {
    c_.preparar(
          "INSERT INTO MovimientosCaja (Fecha, Hora, Tipo, Origen, Concepto, Referencia, Monto, Usuario) "
          "VALUES (?, ?, ?, ?, ?, ?, ?, ?)")
        .pFecha(f)
        .pTexto(horaActual())
        .pEntero(static_cast<long>(t))
        .pTexto(origen)
        .pTexto(concepto.empty() ? origen : concepto)
        .pTexto(referencia)
        .pDinero(monto)
        .pTexto(usuario_)
        .ejecutar();
    return ultimoId();
}

std::optional<Fecha> Repositorio::ultimoCierre(const std::optional<Fecha>& antesDe) {
    std::string sql = "SELECT MAX(Fecha) FROM CierresCaja";
    if (antesDe) sql += " WHERE Fecha < ?";
    auto s = c_.preparar(sql);
    if (antesDe) s.pFecha(*antesDe);
    s.ejecutar();
    if (!s.siguiente()) return std::nullopt;
    return s.fecha(1);
}

bool Repositorio::cajaBloqueada(const Fecha& f) {
    auto s = c_.preparar("SELECT COUNT(*) FROM CierresCaja WHERE Fecha >= ?");
    s.pFecha(f).ejecutar();
    return s.siguiente() && s.entero(1) > 0;
}

void Repositorio::exigirCajaAbierta(const Fecha& f, const std::string& accion) {
    if (cajaBloqueada(f))
        throw ErrorNegocio("La caja del " + f.texto() + " ya está cerrada. Para " + accion +
                           " con esa fecha, primero reabra la caja en la pestaña Caja "
                           "(solo se puede reabrir el último cierre).");
}

std::pair<Centavos, Centavos> Repositorio::sumarMovimientos(const std::string& condicion,
                                                            const std::vector<Fecha>& fechas) {
    auto s = c_.preparar("SELECT Tipo, SUM(Monto) FROM MovimientosCaja WHERE " + condicion + " GROUP BY Tipo");
    for (const auto& f : fechas) s.pFecha(f);
    s.ejecutar();
    std::pair<Centavos, Centavos> r{0, 0};
    while (s.siguiente()) {
        if (s.entero(1) == static_cast<long>(TipoMovimiento::Ingreso))
            r.first = s.dinero(2);
        else
            r.second = s.dinero(2);
    }
    return r;
}

Centavos Repositorio::saldoInicial(const Fecha& f) {
    const auto anterior = ultimoCierre(f);
    Centavos base = 0;
    std::pair<Centavos, Centavos> sumas;
    if (anterior) {
        base = cierre(*anterior)->efectivoContado;
        sumas = sumarMovimientos("Fecha > ? AND Fecha < ?", {*anterior, f});
    } else {
        base = parsearMonto(config("SaldoAperturaCaja", "0")).value_or(0);
        sumas = sumarMovimientos("Fecha < ?", {f});
    }
    return base + sumas.first - sumas.second;
}

std::vector<MovimientoCaja> Repositorio::movimientos(const Fecha& f) {
    auto s = c_.preparar(
        "SELECT Id, Fecha, Hora, Tipo, Origen, Concepto, Referencia, Monto, Usuario FROM MovimientosCaja "
        "WHERE Fecha = ? ORDER BY Hora, Id");
    s.pFecha(f).ejecutar();
    std::vector<MovimientoCaja> r;
    while (s.siguiente()) {
        MovimientoCaja m;
        m.id = s.entero(1);
        m.fecha = s.fecha(2).value_or(Fecha());
        m.hora = s.texto(3);
        m.tipo = s.entero(4) == 1 ? TipoMovimiento::Ingreso : TipoMovimiento::Egreso;
        m.origen = s.texto(5);
        m.concepto = s.texto(6);
        m.referencia = s.texto(7);
        m.monto = s.dinero(8);
        m.usuario = s.texto(9);
        r.push_back(m);
    }
    return r;
}

ResumenCaja Repositorio::resumenCaja(const Fecha& f) {
    ResumenCaja r;
    r.fecha = f;
    r.saldoInicial = saldoInicial(f);
    const auto sumas = sumarMovimientos("Fecha = ?", {f});
    r.ingresos = sumas.first;
    r.egresos = sumas.second;
    r.esperado = r.saldoInicial + r.ingresos - r.egresos;
    r.cierre = cierre(f);
    r.bloqueada = cajaBloqueada(f);
    r.cierreAnterior = ultimoCierre(f);
    return r;
}

std::optional<CierreCaja> Repositorio::cierre(const Fecha& f) {
    auto s = c_.preparar(std::string(SELECT_CIERRE) + " WHERE Fecha = ?");
    s.pFecha(f).ejecutar();
    if (!s.siguiente()) return std::nullopt;
    return leerCierre(s);
}

std::vector<CierreCaja> Repositorio::cierres(const Fecha& desde, const Fecha& hasta) {
    auto s = c_.preparar(std::string(SELECT_CIERRE) + " WHERE Fecha >= ? AND Fecha <= ? ORDER BY Fecha");
    s.pFecha(desde).pFecha(hasta).ejecutar();
    std::vector<CierreCaja> r;
    while (s.siguiente()) r.push_back(leerCierre(s));
    return r;
}

void Repositorio::cerrarCaja(const Fecha& f, Centavos efectivoContado, const std::string& notas) {
    if (!f.valida()) throw ErrorNegocio("La fecha no es válida.");
    if (f > Fecha::hoy()) throw ErrorNegocio("No se puede cerrar la caja de una fecha futura.");
    if (efectivoContado < 0) throw ErrorNegocio("El dinero contado no puede ser negativo.");
    exigirCajaAbierta(f, "cerrar la caja");
    const ResumenCaja r = resumenCaja(f);
    c_.preparar(
          "INSERT INTO CierresCaja (Fecha, Hora, SaldoInicial, Ingresos, Egresos, SaldoEsperado, EfectivoContado, "
          "Diferencia, Notas, Usuario) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
        .pFecha(f)
        .pTexto(horaActual())
        .pDinero(r.saldoInicial)
        .pDinero(r.ingresos)
        .pDinero(r.egresos)
        .pDinero(r.esperado)
        .pDinero(efectivoContado)
        .pDinero(efectivoContado - r.esperado)
        .pTexto(recortar(notas))
        .pTexto(usuario_)
        .ejecutar();
}

void Repositorio::reabrirCaja(const Fecha& f) {
    if (!cierre(f)) throw ErrorNegocio("La caja del " + f.texto() + " no está cerrada.");
    const auto ultimo = ultimoCierre(std::nullopt);
    if (ultimo && *ultimo != f)
        throw ErrorNegocio("Solo se puede reabrir el último cierre de caja (" + ultimo->texto() + ").");
    c_.preparar("DELETE FROM CierresCaja WHERE Fecha = ?").pFecha(f).ejecutar();
}

long Repositorio::registrarMovimientoManual(const Fecha& f, TipoMovimiento tipo, Centavos monto,
                                            const std::string& concepto) {
    if (!f.valida()) throw ErrorNegocio("La fecha no es válida.");
    if (monto <= 0) throw ErrorNegocio("El monto debe ser mayor que cero.");
    if (recortar(concepto).empty()) throw ErrorNegocio("Escriba el concepto del movimiento.");
    exigirCajaAbierta(f, "registrar el movimiento");
    return insertarMovimiento(f, tipo, "MANUAL", recortar(concepto), "", monto);
}

void Repositorio::eliminarMovimientoManual(long id) {
    auto s = c_.preparar("SELECT Fecha, Origen FROM MovimientosCaja WHERE Id = ?");
    s.pEntero(id).ejecutar();
    if (!s.siguiente()) throw ErrorNegocio("El movimiento no existe.");
    const Fecha f = s.fecha(1).value_or(Fecha());
    if (s.texto(2) != "MANUAL")
        throw ErrorNegocio(
            "Solo se pueden eliminar movimientos manuales. Los demás se corrigen anulando el comprobante, la venta, "
            "el abono o la deuda que los generó.");
    exigirCajaAbierta(f, "eliminar el movimiento");
    c_.preparar("DELETE FROM MovimientosCaja WHERE Id = ?").pEntero(id).ejecutar();
}

std::optional<Fecha> Repositorio::diaSinCerrar(const Fecha& hoy) {
    const auto ultimo = ultimoCierre(std::nullopt);
    std::string sql = "SELECT MIN(Fecha) FROM MovimientosCaja WHERE Fecha < ?";
    if (ultimo) sql += " AND Fecha > ?";
    auto s = c_.preparar(sql);
    s.pFecha(hoy);
    if (ultimo) s.pFecha(*ultimo);
    s.ejecutar();
    if (!s.siguiente()) return std::nullopt;
    return s.fecha(1);
}

// ================================================================ Recordatorio
Recordatorio Repositorio::recordatorio(const Fecha& hoy) {
    Recordatorio r;
    r.hoy = hoy;
    r.diasAnticipacion = static_cast<int>(std::max(0L, configEntero("DiasAnticipacion", 3)));
    for (const auto& d : deudas(FiltroDeudas{})) {
        if (!d.vencimiento) {
            ++r.pendientesSinFecha;
            continue;
        }
        switch (urgenciaDeuda(d, hoy, r.diasAnticipacion)) {
            case -1: r.vencidas.push_back(d); break;
            case 0: r.venceHoy.push_back(d); break;
            case 1: r.proximas.push_back(d); break;
            default: break;
        }
    }
    r.totales = resumenDeudas();
    r.faltaPrecioHoy = preciosDelDia(hoy).vacio();
    r.diaSinCerrar = diaSinCerrar(hoy);
    return r;
}

// ================================================================ Usuarios
namespace {
const char* const SELECT_USUARIO = "SELECT Id, Usuario, Nombre, Rol, Activo, DebeCambiar, Hash FROM Usuarios";

Usuario leerUsuario(const Sentencia& s) {
    Usuario u;
    u.id = s.entero(1);
    u.usuario = s.texto(2);
    u.nombre = s.texto(3);
    u.rol = s.entero(4) == ROL_ADMINISTRADOR ? ROL_ADMINISTRADOR : ROL_OPERADOR;
    u.activo = s.entero(5) != 0;
    u.debeCambiar = s.entero(6) != 0;
    return u;
}

std::string normalizarUsuario(const std::string& usuario) {
    std::string r;
    for (char ch : recortar(usuario)) r += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return r;
}
}  // namespace

void Repositorio::asegurarUsuarioMaestro(const std::vector<uint8_t>& sal) {
    auto s = c_.preparar("SELECT COUNT(*) FROM Usuarios");
    s.ejecutar();
    if (s.siguiente() && s.entero(1) > 0) return;
    Usuario u;
    u.usuario = USUARIO_MAESTRO;
    u.nombre = "Administrador";
    u.rol = ROL_ADMINISTRADOR;
    u.activo = true;
    u.debeCambiar = true;
    c_.preparar(
          "INSERT INTO Usuarios (Usuario, Nombre, Hash, Rol, Activo, DebeCambiar, FechaRegistro) "
          "VALUES (?, ?, ?, ?, 1, 1, ?)")
        .pTexto(u.usuario)
        .pTexto(u.nombre)
        .pTexto(crearHashContrasena(CONTRASENA_INICIAL, sal, iteraciones_))
        .pEntero(u.rol)
        .pFecha(Fecha::hoy())
        .ejecutar();
}

std::vector<Usuario> Repositorio::usuarios() {
    auto s = c_.preparar(std::string(SELECT_USUARIO) + " ORDER BY Usuario");
    s.ejecutar();
    std::vector<Usuario> r;
    while (s.siguiente()) r.push_back(leerUsuario(s));
    return r;
}

std::optional<Usuario> Repositorio::usuarioPorId(long id) {
    auto s = c_.preparar(std::string(SELECT_USUARIO) + " WHERE Id = ?");
    s.pEntero(id).ejecutar();
    if (!s.siguiente()) return std::nullopt;
    return leerUsuario(s);
}

std::optional<Usuario> Repositorio::autenticar(const std::string& usuario, const std::string& contrasena) {
    auto s = c_.preparar(std::string(SELECT_USUARIO) + " WHERE Usuario = ?");
    s.pTexto(normalizarUsuario(usuario)).ejecutar();
    if (!s.siguiente()) {
        // Se calcula igual un hash para que el tiempo de respuesta no revele si el usuario existe.
        verificarContrasena(contrasena, crearHashContrasena("x", {1, 2, 3, 4}, iteraciones_));
        return std::nullopt;
    }
    Usuario u = leerUsuario(s);
    const std::string hash = s.texto(7);
    if (!u.activo || !verificarContrasena(contrasena, hash)) return std::nullopt;
    c_.preparar("UPDATE Usuarios SET UltimoIngreso = ? WHERE Id = ?").pFecha(Fecha::hoy()).pEntero(u.id).ejecutar();
    return u;
}

long Repositorio::crearUsuario(Usuario& u, const std::string& contrasena, const std::vector<uint8_t>& sal) {
    u.usuario = normalizarUsuario(u.usuario);
    u.nombre = recortar(u.nombre);
    if (u.usuario.size() < 3) throw ErrorNegocio("El usuario debe tener al menos 3 letras.");
    for (char ch : u.usuario)
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '.' && ch != '_' && ch != '-')
            throw ErrorNegocio("El usuario solo puede tener letras sin tilde, números, punto, guion y guion bajo.");
    const std::string error = validarContrasenaNueva(contrasena, u.usuario);
    if (!error.empty()) throw ErrorNegocio(error);
    auto dup = c_.preparar("SELECT COUNT(*) FROM Usuarios WHERE Usuario = ?");
    dup.pTexto(u.usuario).ejecutar();
    if (dup.siguiente() && dup.entero(1) > 0) throw ErrorNegocio("Ya existe el usuario \"" + u.usuario + "\".");
    c_.preparar(
          "INSERT INTO Usuarios (Usuario, Nombre, Hash, Rol, Activo, DebeCambiar, FechaRegistro) "
          "VALUES (?, ?, ?, ?, ?, ?, ?)")
        .pTexto(u.usuario)
        .pTexto(u.nombre)
        .pTexto(crearHashContrasena(contrasena, sal, iteraciones_))
        .pEntero(u.rol == ROL_ADMINISTRADOR ? ROL_ADMINISTRADOR : ROL_OPERADOR)
        .pEntero(u.activo ? 1 : 0)
        .pEntero(u.debeCambiar ? 1 : 0)
        .pFecha(Fecha::hoy())
        .ejecutar();
    u.id = ultimoId();
    return u.id;
}

int Repositorio::administradoresActivos(long excepto) {
    auto s = c_.preparar("SELECT COUNT(*) FROM Usuarios WHERE Rol = ? AND Activo = 1 AND Id <> ?");
    s.pEntero(ROL_ADMINISTRADOR).pEntero(excepto).ejecutar();
    return s.siguiente() ? static_cast<int>(s.entero(1)) : 0;
}

void Repositorio::guardarUsuario(const Usuario& u) {
    const auto actual = usuarioPorId(u.id);
    if (!actual) throw ErrorNegocio("El usuario no existe.");
    const bool quedaAdmin = u.activo && u.rol == ROL_ADMINISTRADOR;
    if (actual->esAdministrador() && actual->activo && !quedaAdmin && administradoresActivos(u.id) == 0)
        throw ErrorNegocio("Debe quedar al menos un administrador activo.");
    c_.preparar("UPDATE Usuarios SET Nombre = ?, Rol = ?, Activo = ? WHERE Id = ?")
        .pTexto(recortar(u.nombre))
        .pEntero(u.rol == ROL_ADMINISTRADOR ? ROL_ADMINISTRADOR : ROL_OPERADOR)
        .pEntero(u.activo ? 1 : 0)
        .pEntero(u.id)
        .ejecutar();
}

void Repositorio::cambiarContrasena(long id, const std::string& nueva, const std::vector<uint8_t>& sal,
                                    bool debeCambiar) {
    const auto u = usuarioPorId(id);
    if (!u) throw ErrorNegocio("El usuario no existe.");
    const std::string error = validarContrasenaNueva(nueva, u->usuario);
    if (!error.empty()) throw ErrorNegocio(error);
    c_.preparar("UPDATE Usuarios SET Hash = ?, DebeCambiar = ? WHERE Id = ?")
        .pTexto(crearHashContrasena(nueva, sal, iteraciones_))
        .pEntero(debeCambiar ? 1 : 0)
        .pEntero(id)
        .ejecutar();
}

void Repositorio::guardarCodigoRecuperacion(const std::string& codigo, const std::vector<uint8_t>& sal) {
    setConfig("CodigoRecuperacion", crearHashContrasena(normalizarCodigo(codigo), sal, iteraciones_));
}

bool Repositorio::verificarCodigoRecuperacion(const std::string& codigo) {
    const std::string guardado = config("CodigoRecuperacion");
    return !guardado.empty() && verificarContrasena(normalizarCodigo(codigo), guardado);
}

}  // namespace bu
