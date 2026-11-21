// Pestaña "Historial": comprobantes de compra registrados (reimprimir y anular).
#include <cstdio>

#include "core/Documentos.h"
#include "core/Texto.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Excel.h"
#include "win/Paginas.h"
#include "win/resource.h"

namespace bu::win {

namespace {

class PaginaHistorial : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Historial de compras"; }

    void alIniciar() override {
        controlEstirable_ = IDC_H_LISTA;
        configurarFecha(h_, IDC_H_DESDE);
        configurarFecha(h_, IDC_H_HASTA);
        ponerFecha(h_, IDC_H_DESDE, Fecha::hoy().primerDiaDelMes());
        ponerFecha(h_, IDC_H_HASTA, Fecha::hoy());
        lvConfigurar(h_, IDC_H_LISTA,
                     {{L"No.", 40},
                      {L"Fecha", 50},
                      {L"Productor", 130},
                      {L"DNI / RTN", 74},
                      {L"Libras", 56, true},
                      {L"Quintales", 46, true},
                      {L"Total", 66, true},
                      {L"Pagado", 66, true},
                      {L"Saldo", 60, true},
                      {L"Estado", 50}});
    }

    void alMostrar() override { buscar(); }

    bool alComando(int id, int codigo) override {
        switch (id) {
            case IDC_H_BUSCAR:
                buscar();
                return true;
            case IDC_H_FILTRO:
                if (codigo == EN_CHANGE) mostrarLista();
                return true;
            case IDC_H_HOY:
                ponerFecha(h_, IDC_H_DESDE, Fecha::hoy());
                ponerFecha(h_, IDC_H_HASTA, Fecha::hoy());
                buscar();
                return true;
            case IDC_H_MES:
                ponerFecha(h_, IDC_H_DESDE, Fecha::hoy().primerDiaDelMes());
                ponerFecha(h_, IDC_H_HASTA, Fecha::hoy());
                buscar();
                return true;
            case IDC_H_IMPRIMIR:
                imprimir();
                return true;
            case IDC_H_ANULAR:
                anular();
                return true;
            default:
                return false;
        }
    }

    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if ((nm->idFrom == IDC_H_DESDE || nm->idFrom == IDC_H_HASTA) && nm->code == DTN_DATETIMECHANGE) {
            buscar();
            r = 0;
            return true;
        }
        if (nm->idFrom == IDC_H_LISTA && nm->code == NM_DBLCLK) {
            imprimir();
            r = 0;
            return true;
        }
        if (nm->idFrom == IDC_H_LISTA) return colorearFilas(nm, colores_, r);
        return false;
    }

    void alAceptar() override { buscar(); }

private:
    void buscar() {
        facturas_ = repo().facturas(leerFecha(h_, IDC_H_DESDE), leerFecha(h_, IDC_H_HASTA));
        mostrarLista();
    }

    void mostrarLista() {
        lvLimpiar(h_, IDC_H_LISTA);
        colores_.clear();
        const std::string filtro = texto(h_, IDC_H_FILTRO);
        int cantidad = 0;
        double libras = 0;
        Centavos total = 0, pagado = 0, saldo = 0;
        for (const auto& f : facturas_) {
            if (!coincideBusqueda(f.clienteNombre + " " + f.clienteDni, filtro)) continue;
            char num[16];
            std::snprintf(num, sizeof num, "%06ld", f.numero);
            lvAgregar(h_, IDC_H_LISTA,
                      {num, f.fecha.texto(), f.clienteNombre, f.clienteDni, formatoDecimal(f.totalLibras, 2),
                       formatoDecimal(aQuintales(f.totalLibras), 2), formatoLempiras(f.total), formatoLempiras(f.pagado),
                       formatoLempiras(f.saldo), f.anulada ? "ANULADO" : "Válido"},
                      f.id);
            if (f.anulada) {
                colores_[f.id] = COLOR_GRIS;
                continue;
            }
            ++cantidad;
            libras += f.totalLibras;
            total += f.total;
            pagado += f.pagado;
            saldo += f.saldo;
        }
        ponerTexto(h_, IDC_H_RESUMEN,
                   std::to_string(cantidad) + " comprobantes válidos  ·  " + formatoDecimal(libras, 2) + " lb (" +
                       formatoDecimal(aQuintales(libras), 2) + " qq)  ·  Total " + formatoLempiras(total) +
                       "  ·  Pagado " + formatoLempiras(pagado) + "  ·  Saldo " + formatoLempiras(saldo));
    }

    long seleccionada() {
        const long id = static_cast<long>(lvDatoSeleccionado(h_, IDC_H_LISTA, 0));
        if (!id) mostrarAviso(h_, "Seleccione un comprobante de la lista.");
        return id;
    }

    void imprimir() {
        const long id = seleccionada();
        if (!id) return;
        Documento doc;
        if (intentar(h_, [&] {
                const auto f = repo().facturaPorId(id);
                const auto c = repo().clientePorId(f->clienteId);
                doc = docComprobanteCompra(repo().empresa(), *f, *c);
            }))
            generarYMostrar(h_, doc);
    }

    void anular() {
        const long id = seleccionada();
        if (!id) return;
        const auto f = repo().facturaPorId(id);
        if (!f) return;
        if (f->anulada) {
            mostrarInfo(h_, "Ese comprobante ya está anulado.");
            return;
        }
        std::string aviso = "¿Anular el comprobante No. " + std::to_string(f->numero) + " de " + f->clienteNombre +
                            " por " + formatoLempiras(f->total) + "?";
        if (f->pagado > 0)
            aviso += "\n\nSe registrará en la caja de HOY una entrada de " + formatoLempiras(f->pagado) +
                     " (devolución de lo pagado).";
        if (f->saldo > 0) aviso += "\nTambién se anulará el saldo pendiente de " + formatoLempiras(f->saldo) + ".";
        if (!confirmar(h_, aviso)) return;
        const auto motivo = pedirTexto(h_, "Anular comprobante", "Escriba el motivo de la anulación:");
        if (!motivo) return;
        if (intentar(h_, [&] { repo().anularFactura(id, *motivo, Fecha::hoy()); })) {
            mostrarInfo(h_, "Comprobante anulado.");
            buscar();
        }
    }

    std::vector<Factura> facturas_;
    std::map<LPARAM, COLORREF> colores_;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaHistorial() { return std::make_unique<PaginaHistorial>(); }

}  // namespace bu::win
