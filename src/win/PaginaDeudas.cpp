// Pestaña "Deudas": lo que el beneficio debe a los clientes y lo que le deben.
#include "core/Documentos.h"
#include "core/Texto.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Excel.h"
#include "win/Paginas.h"
#include "win/Tema.h"
#include "win/Sesion.h"
#include "win/resource.h"

namespace bu::win {

namespace {

class PaginaDeudas : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Deudas"; }

    void alIniciar() override {
        controlEstirable_ = IDC_D_LISTA;
        comboAgregar(h_, IDC_D_TIPO, "Todas las deudas", 0);
        comboAgregar(h_, IDC_D_TIPO, "El beneficio debe (por pagar)", 1);
        comboAgregar(h_, IDC_D_TIPO, "Le deben al beneficio (por cobrar)", 2);
        comboSeleccionar(h_, IDC_D_TIPO, 0);
        comboAgregar(h_, IDC_D_ESTADO, "Solo pendientes", 1);
        comboAgregar(h_, IDC_D_ESTADO, "Todas (pagadas y anuladas)", 0);
        comboSeleccionar(h_, IDC_D_ESTADO, 1);
        lvConfigurar(h_, IDC_D_LISTA,
                     {{L"No.", 30},
                      {L"Cliente", 120},
                      {L"DNI / RTN", 74},
                      {L"Tipo", 62},
                      {L"Origen", 50},
                      {L"Concepto", 130},
                      {L"Fecha", 50},
                      {L"Fecha de pago", 54},
                      {L"Monto", 62, true},
                      {L"Saldo", 62, true},
                      {L"Estado", 80}});
        textoGuia(h_, IDC_D_FILTRO, L"Buscar cliente o DNI...");
        tema::campoBusqueda(GetDlgItem(h_, IDC_D_FILTRO));
        ponerFuente(h_, IDC_D_RESUMEN, fuenteNegrita());
        colorTexto(IDC_D_RESUMEN, COLOR_CAFE);
    }

    void alMostrar() override { cargar(); }

    bool alComando(int id, int codigo) override {
        switch (id) {
            case IDC_D_TIPO:
            case IDC_D_ESTADO:
                if (codigo == CBN_SELCHANGE) cargar();
                return true;
            case IDC_D_FILTRO:
                if (codigo == EN_CHANGE) mostrarLista();
                return true;
            case IDC_D_NUEVA:
                if (dialogoNuevaDeuda(h_)) cargar();
                return true;
            case IDC_D_ABONO: {
                const long d = seleccionada();
                if (d && dialogoAbono(h_, d)) cargar();
                return true;
            }
            case IDC_D_DETALLE: {
                const long d = seleccionada();
                if (d) {
                    dialogoDetalleDeuda(h_, d);
                    cargar();
                }
                return true;
            }
            case IDC_D_ANULAR:
                anular();
                return true;
            case IDC_D_RECORDATORIO:
                App::instancia().mostrarRecordatorio(h_, true);
                return true;
            case IDC_D_REPORTE: {
                Documento doc;
                if (intentar(h_, [&] { doc = docDeudasPendientes(repo().empresa(), repo().deudas(FiltroDeudas{}), Fecha::hoy()); }))
                    generarYMostrar(h_, doc);
                return true;
            }
            default:
                return false;
        }
    }

    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if (nm->idFrom != IDC_D_LISTA) return false;
        if (nm->code == NM_DBLCLK) {
            const long d = static_cast<long>(lvDatoSeleccionado(h_, IDC_D_LISTA, 0));
            if (d) {
                dialogoDetalleDeuda(h_, d);
                cargar();
            }
            r = 0;
            return true;
        }
        return colorearFilas(nm, colores_, r);
    }

    void alAceptar() override { cargar(); }

private:
    void cargar() {
        FiltroDeudas f;
        f.tipo = static_cast<int>(comboDato(h_, IDC_D_TIPO, 0));
        f.soloPendientes = comboDato(h_, IDC_D_ESTADO, 1) == 1;
        deudas_ = repo().deudas(f);
        dias_ = static_cast<int>(repo().configEntero("DiasAnticipacion", 3));
        const ResumenDeudas rd = repo().resumenDeudas();
        ponerTexto(h_, IDC_D_RESUMEN,
                   "El beneficio debe en total: " + formatoLempiras(rd.porPagar) + " (" + std::to_string(rd.cantPorPagar) +
                       ")      ·      Le deben al beneficio: " + formatoLempiras(rd.porCobrar) + " (" +
                       std::to_string(rd.cantPorCobrar) + ")");
        mostrarLista();
    }

    void mostrarLista() {
        lvLimpiar(h_, IDC_D_LISTA);
        colores_.clear();
        const std::string filtro = texto(h_, IDC_D_FILTRO);
        const Fecha hoy = Fecha::hoy();
        for (const auto& d : deudas_) {
            if (!coincideBusqueda(d.clienteNombre + " " + d.clienteDni + " " + d.concepto, filtro)) continue;
            lvAgregar(h_, IDC_D_LISTA,
                      {std::to_string(d.id), d.clienteNombre, d.clienteDni, nombreTipoDeuda(d.tipo), d.origen, d.concepto,
                       d.fecha.texto(), d.vencimiento ? d.vencimiento->texto() : std::string(),
                       formatoLempiras(d.monto), formatoLempiras(d.anulada ? 0 : d.saldo), estadoDeuda(d, hoy)},
                      d.id);
            colores_[d.id] = colorDeuda(d, hoy, dias_);
        }
    }

    long seleccionada() {
        const long id = static_cast<long>(lvDatoSeleccionado(h_, IDC_D_LISTA, 0));
        if (!id) mostrarAviso(h_, "Seleccione una deuda de la lista.");
        return id;
    }

    void anular() {
        const long id = seleccionada();
        if (!id) return;
        const auto d = repo().deudaPorId(id);
        if (!d) return;
        std::string aviso = "¿Anular la deuda #" + std::to_string(id) + " de " + d->clienteNombre + " por " +
                            formatoLempiras(d->monto) + "?";
        if (d->afectoCaja)
            aviso += "\n\nComo esta deuda movió dinero de caja, se registrará el movimiento contrario en la caja de HOY.";
        if (!confirmar(h_, aviso)) return;
        if (!autorizar(h_, "anular una deuda")) return;
        if (intentar(h_, [&] { repo().anularDeuda(id, Fecha::hoy()); })) cargar();
    }

    std::vector<Deuda> deudas_;
    std::map<LPARAM, COLORREF> colores_;
    int dias_ = 3;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaDeudas() { return std::make_unique<PaginaDeudas>(); }

}  // namespace bu::win
