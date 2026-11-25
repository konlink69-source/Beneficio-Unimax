// Pestaña "Ventas": venta de café a exportadoras u otros compradores.
#include <cstdio>

#include "core/Texto.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Paginas.h"
#include "win/Sesion.h"
#include "win/resource.h"

namespace bu::win {

namespace {

class PaginaVentas : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Ventas"; }

    void alIniciar() override {
        controlEstirable_ = IDC_V_LISTA;
        comprador_.configurar(h_, IDC_V_DNI, IDC_V_NOMBRE, 0, IDC_V_INFO, CLIENTE_COMPRADOR);
        for (int id : {IDC_V_FECHA, IDC_V_VENCE, IDC_V_DESDE, IDC_V_HASTA}) configurarFecha(h_, id);
        ponerFecha(h_, IDC_V_DESDE, Fecha::hoy().primerDiaDelMes());
        ponerFecha(h_, IDC_V_HASTA, Fecha::hoy());
        for (int t = 1; t <= NUM_TIPOS_CAFE; ++t) comboAgregar(h_, IDC_V_TIPO, nombreTipoCafe(t), t);
        comboAgregar(h_, IDC_V_UNIDAD, "Libras", static_cast<LPARAM>(Unidad::Libras));
        comboAgregar(h_, IDC_V_UNIDAD, "Quintales", static_cast<LPARAM>(Unidad::Quintales));
        lvConfigurar(h_, IDC_V_LISTA,
                     {{L"No.", 30},
                      {L"Fecha", 50},
                      {L"Comprador", 110},
                      {L"DNI / RTN", 74},
                      {L"Tipo", 66},
                      {L"Libras", 56, true},
                      {L"Quintales", 46, true},
                      {L"Precio/lb", 46, true},
                      {L"Total", 66, true},
                      {L"Recibido", 66, true},
                      {L"Saldo", 60, true},
                      {L"Estado", 46}});
        ponerFuente(h_, IDC_V_TOTAL, fuenteNegrita());
        ponerFuente(h_, IDC_V_NUMERO, fuenteNegrita());
        colorTexto(IDC_V_TOTAL, COLOR_CAFE);
        limpiar();
    }

    void alMostrar() override {
        ponerTexto(h_, IDC_V_NUMERO, std::to_string(repo().siguienteNumeroVenta()));
        listar();
    }

    bool alComando(int id, int codigo) override {
        if (comprador_.comando(id, codigo)) return true;
        switch (id) {
            case IDC_V_BUSCAR:
                comprador_.elegir();
                return true;
            case IDC_V_CANTIDAD:
            case IDC_V_PRECIO:
                if (codigo == EN_CHANGE) actualizarTotal();
                return true;
            case IDC_V_UNIDAD:
                if (codigo == CBN_SELCHANGE) actualizarTotal();
                return true;
            case IDC_V_RECIBIDO:
                if (codigo == EN_CHANGE && !actualizando_) recibidoEditado_ = true;
                return true;
            case IDC_V_GUARDAR:
                guardar();
                return true;
            case IDC_V_LIMPIAR:
                limpiar();
                return true;
            case IDC_V_LISTAR:
                listar();
                return true;
            case IDC_V_ANULAR:
                anular();
                return true;
            default:
                return false;
        }
    }

    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if ((nm->idFrom == IDC_V_DESDE || nm->idFrom == IDC_V_HASTA) && nm->code == DTN_DATETIMECHANGE) {
            listar();
            r = 0;
            return true;
        }
        if (nm->idFrom == IDC_V_LISTA) return colorearFilas(nm, colores_, r);
        return false;
    }

    void alAceptar() override {
        if (comprador_.alPresionarEnter()) return;
        guardar();
    }

private:
    Centavos totalActual() const {
        const auto cantidad = leerDecimal(h_, IDC_V_CANTIDAD);
        const auto precio = leerDecimal(h_, IDC_V_PRECIO);
        if (!cantidad || !precio || *cantidad <= 0 || *precio <= 0) return 0;
        const double libras = aLibras(*cantidad, static_cast<Unidad>(comboDato(h_, IDC_V_UNIDAD, 0)));
        return calcularSubtotal(redondear(libras, 2), redondear(*precio, 4));
    }

    void actualizarTotal() {
        const Centavos t = totalActual();
        ponerTexto(h_, IDC_V_TOTAL, t > 0 ? formatoLempiras(t) : std::string());
        if (!recibidoEditado_) {
            actualizando_ = true;
            ponerTexto(h_, IDC_V_RECIBIDO, t > 0 ? formatoMonto(t) : std::string());
            actualizando_ = false;
        }
    }

    void limpiar() {
        comprador_.limpiar();
        ponerFecha(h_, IDC_V_FECHA, Fecha::hoy());
        ponerFechaOpc(h_, IDC_V_VENCE, std::nullopt);
        comboSeleccionar(h_, IDC_V_TIPO, CAFE_SECO);
        comboSeleccionar(h_, IDC_V_UNIDAD, static_cast<LPARAM>(Unidad::Libras));
        for (int id : {IDC_V_CANTIDAD, IDC_V_PRECIO, IDC_V_RECIBIDO, IDC_V_OBS}) ponerTexto(h_, id, "");
        recibidoEditado_ = false;
        actualizarTotal();
        enfocar(h_, IDC_V_DNI);
    }

    void listar() {
        ventas_ = repo().ventas(leerFecha(h_, IDC_V_DESDE), leerFecha(h_, IDC_V_HASTA));
        lvLimpiar(h_, IDC_V_LISTA);
        colores_.clear();
        double libras = 0;
        Centavos total = 0, recibido = 0;
        int n = 0;
        for (const auto& v : ventas_) {
            lvAgregar(h_, IDC_V_LISTA,
                      {std::to_string(v.numero), v.fecha.texto(), v.clienteNombre, v.clienteDni, nombreTipoCafe(v.tipoCafe),
                       formatoDecimal(v.libras, 2), formatoDecimal(aQuintales(v.libras), 2), formatoPrecio(v.precioLibra),
                       formatoLempiras(v.total), formatoLempiras(v.recibido), formatoLempiras(v.saldo),
                       v.anulada ? "ANULADA" : "Válida"},
                      v.id);
            if (v.anulada) {
                colores_[v.id] = COLOR_GRIS;
                continue;
            }
            ++n;
            libras += v.libras;
            total += v.total;
            recibido += v.recibido;
        }
        ponerTexto(h_, IDC_V_RESUMEN,
                   std::to_string(n) + " ventas · " + formatoDecimal(aQuintales(libras), 2) + " qq · " +
                       formatoLempiras(total) + " · cobrado " + formatoLempiras(recibido));
    }

    void guardar() {
        const auto cantidad = leerDecimal(h_, IDC_V_CANTIDAD);
        const auto precio = leerDecimal(h_, IDC_V_PRECIO);
        if (!cantidad || *cantidad <= 0) {
            mostrarAviso(h_, "Escriba el peso vendido.");
            enfocar(h_, IDC_V_CANTIDAD);
            return;
        }
        if (!precio || *precio <= 0) {
            mostrarAviso(h_, "Escriba el precio por libra.");
            enfocar(h_, IDC_V_PRECIO);
            return;
        }
        const auto recibido = vacio(h_, IDC_V_RECIBIDO) ? std::optional<Centavos>(0) : leerMonto(h_, IDC_V_RECIBIDO);
        if (!recibido) {
            mostrarAviso(h_, "El monto recibido no es válido.");
            enfocar(h_, IDC_V_RECIBIDO);
            return;
        }
        Cliente c;
        if (!intentar(h_, [&] { c = comprador_.obtenerORegistrar(); })) return;
        Venta v;
        v.clienteId = c.id;
        v.fecha = leerFecha(h_, IDC_V_FECHA);
        v.tipoCafe = static_cast<int>(comboDato(h_, IDC_V_TIPO, CAFE_SECO));
        v.libras = aLibras(*cantidad, static_cast<Unidad>(comboDato(h_, IDC_V_UNIDAD, 0)));
        v.precioLibra = *precio;
        v.recibido = *recibido;
        v.vencimientoSaldo = leerFechaOpc(h_, IDC_V_VENCE);
        v.observaciones = texto(h_, IDC_V_OBS);
        const Centavos total = calcularSubtotal(redondear(v.libras, 2), redondear(v.precioLibra, 4));
        std::string pregunta = "¿Registrar la venta?\n\nComprador: " + c.nombre + "\n" + nombreTipoCafe(v.tipoCafe) + ": " +
                               formatoDecimal(v.libras, 2) + " lb (" + formatoDecimal(aQuintales(v.libras), 2) +
                               " qq)\nTotal: " + formatoLempiras(total) + "\nRecibido ahora: " + formatoLempiras(v.recibido);
        if (total - v.recibido > 0) pregunta += "\nQueda por cobrar: " + formatoLempiras(total - v.recibido);
        if (!confirmar(h_, pregunta)) return;
        if (!intentar(h_, [&] { repo().registrarVenta(v); })) return;
        mostrarInfo(h_, "Venta No. " + std::to_string(v.numero) + " registrada.");
        limpiar();
        alMostrar();
    }

    void anular() {
        const long id = static_cast<long>(lvDatoSeleccionado(h_, IDC_V_LISTA, 0));
        if (!id) {
            mostrarAviso(h_, "Seleccione una venta de la lista.");
            return;
        }
        for (const auto& v : ventas_) {
            if (v.id != id) continue;
            if (v.anulada) {
                mostrarInfo(h_, "Esa venta ya está anulada.");
                return;
            }
            std::string aviso = "¿Anular la venta No. " + std::to_string(v.numero) + " a " + v.clienteNombre + "?";
            if (v.recibido > 0)
                aviso += "\n\nSe registrará en la caja de HOY una salida de " + formatoLempiras(v.recibido) +
                         " (devolución de lo cobrado).";
            if (!confirmar(h_, aviso)) return;
            if (!autorizar(h_, "anular una venta")) return;
            const auto motivo = pedirTexto(h_, "Anular venta", "Escriba el motivo de la anulación:");
            if (!motivo) return;
            if (intentar(h_, [&] { repo().anularVenta(id, *motivo, Fecha::hoy()); })) listar();
            return;
        }
    }

    EntradaCliente comprador_;
    std::vector<Venta> ventas_;
    std::map<LPARAM, COLORREF> colores_;
    bool recibidoEditado_ = false;
    bool actualizando_ = false;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaVentas() { return std::make_unique<PaginaVentas>(); }

}  // namespace bu::win
