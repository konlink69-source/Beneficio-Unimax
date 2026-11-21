#include "core/Modelos.h"

#include "core/Texto.h"

namespace bu {

const char* nombreTipoCafe(int tipo) {
    switch (tipo) {
        case CAFE_UVA: return "Uva";
        case CAFE_HUMEDO: return "Pergamino húmedo";
        case CAFE_SECO: return "Pergamino seco";
        default: return "?";
    }
}

const char* nombreTipoCliente(int tipo) {
    switch (tipo) {
        case CLIENTE_PRODUCTOR: return "Productor";
        case CLIENTE_COMPRADOR: return "Comprador";
        case CLIENTE_AMBOS: return "Productor y comprador";
        default: return "?";
    }
}

void recalcularFactura(Factura& f) {
    f.total = 0;
    f.totalLibras = 0;
    for (auto& l : f.lineas) {
        l.libras = redondear(l.libras, 2);
        l.galones = redondear(l.galones, 2);
        l.precioLibra = redondear(l.precioLibra, 4);
        l.subtotal = calcularSubtotal(l.libras, l.precioLibra);
        f.total += l.subtotal;
        f.totalLibras += l.libras;
    }
    f.saldo = f.total - f.pagado;
}

std::string validarFactura(const Factura& f) {
    if (f.clienteId <= 0) return "Seleccione o registre el cliente.";
    if (!f.fecha.valida()) return "La fecha del comprobante no es válida.";
    if (f.lineas.empty()) return "Agregue al menos una línea de café pesado.";
    if (static_cast<int>(f.lineas.size()) > MAX_LINEAS_FACTURA)
        return "Un comprobante admite como máximo " + std::to_string(MAX_LINEAS_FACTURA) + " líneas.";
    for (const auto& l : f.lineas) {
        if (l.tipoCafe < 1 || l.tipoCafe > NUM_TIPOS_CAFE) return "Hay una línea con tipo de café inválido.";
        if (l.libras <= 0) return "Hay una línea sin peso (las libras deben ser mayores que cero).";
        if (l.galones < 0) return "Los galones no pueden ser negativos.";
        if (l.precioLibra <= 0) return "Hay una línea sin precio por libra.";
    }
    if (f.total <= 0) return "El total del comprobante debe ser mayor que cero.";
    if (f.pagado < 0) return "El monto pagado no puede ser negativo.";
    if (f.pagado > f.total) return "El monto pagado no puede ser mayor que el total.";
    return std::string();
}

const char* nombreTipoDeuda(TipoDeuda t) {
    return t == TipoDeuda::PorPagar ? "Beneficio debe" : "Cliente debe";
}

const char* descripcionTipoDeuda(TipoDeuda t) {
    return t == TipoDeuda::PorPagar ? "Por pagar (el beneficio le debe al cliente)"
                                    : "Por cobrar (el cliente le debe al beneficio)";
}

int urgenciaDeuda(const Deuda& d, const Fecha& hoy, int dias) {
    if (!d.pendiente() || !d.vencimiento) return 2;
    const int k = hoy.diasHasta(*d.vencimiento);
    if (k < 0) return -1;
    if (k == 0) return 0;
    if (k <= dias) return 1;
    return 2;
}

std::string estadoDeuda(const Deuda& d, const Fecha& hoy) {
    if (d.anulada) return "Anulada";
    if (d.saldo <= 0) return "Pagada";
    if (!d.vencimiento) return "Pendiente";
    const int k = hoy.diasHasta(*d.vencimiento);
    if (k < 0) return "Vencida hace " + std::to_string(-k) + (k == -1 ? " día" : " días");
    if (k == 0) return "Vence hoy";
    if (k == 1) return "Vence mañana";
    return "Vence en " + std::to_string(k) + " días";
}

std::string nombreOrigenMovimiento(const std::string& origen) {
    if (origen == "COMPRA") return "Compra de café";
    if (origen == "VENTA") return "Venta de café";
    if (origen == "ABONO") return "Abono a deuda";
    if (origen == "PRESTAMO") return "Préstamo / deuda";
    if (origen == "ANULACION") return "Anulación";
    if (origen == "MANUAL") return "Manual";
    return origen;
}

std::string Recordatorio::resumenCorto() const {
    std::string r;
    auto agregar = [&r](const std::string& s) {
        if (!r.empty()) r += " · ";
        r += s;
    };
    if (!vencidas.empty())
        agregar(std::to_string(vencidas.size()) + (vencidas.size() == 1 ? " deuda vencida" : " deudas vencidas"));
    if (!venceHoy.empty())
        agregar(std::to_string(venceHoy.size()) + (venceHoy.size() == 1 ? " vence hoy" : " vencen hoy"));
    if (!proximas.empty())
        agregar(std::to_string(proximas.size()) + (proximas.size() == 1 ? " vence pronto" : " vencen pronto"));
    if (faltaPrecioHoy) agregar("falta el precio del café de hoy");
    if (diaSinCerrar) agregar("caja del " + diaSinCerrar->texto() + " sin cerrar");
    return r;
}

std::string Empresa::lineaDatos() const {
    std::string r;
    auto agregar = [&r](const std::string& s) {
        if (s.empty()) return;
        if (!r.empty()) r += "  ·  ";
        r += s;
    };
    if (!recortar(rtn).empty()) agregar("RTN " + recortar(rtn));
    agregar(recortar(direccion));
    if (!recortar(telefono).empty()) agregar("Tel. " + recortar(telefono));
    return r;
}

}  // namespace bu
