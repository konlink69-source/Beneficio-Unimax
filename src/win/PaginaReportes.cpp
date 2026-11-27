// Pestaña "Reportes": documentos de Excel por período.
#include "core/Documentos.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Excel.h"
#include "win/Impresion.h"
#include "win/Paginas.h"
#include "win/Periodo.h"
#include "win/resource.h"

namespace bu::win {

namespace {

class PaginaReportes : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Reportes"; }

    void alIniciar() override {
        periodo_.configurar(h_, IDC_R_PERIODO, IDC_R_DESDE, IDC_R_LBL_A, IDC_R_HASTA, PERIODO_MES);
        ponerTexto(h_, IDC_R_INFO, "Los documentos se guardan en:\n" + U(App::instancia().rutas().documentos));
        colorTexto(IDC_R_INFO, COLOR_GRIS);
    }

    void alMostrar() override { periodo_.actualizar(); }

    bool alComando(int id, int codigo) override {
        if (periodo_.comando(id, codigo)) return true;
        const Fecha desde = periodo_.desde();
        const Fecha hasta = periodo_.hasta();
        const bool necesitaPeriodo = id == IDC_R_COMPRAS || id == IDC_R_VENTAS || id == IDC_R_CAJA;
        if (necesitaPeriodo && hasta < desde) {
            mostrarAviso(h_, "La fecha \"Hasta\" no puede ser anterior a \"Desde\".");
            return true;
        }
        Documento doc;
        bool generar = false;
        switch (id) {
            case IDC_R_COMPRAS:
                generar = intentar(h_, [&] {
                    doc = docReporteCompras(repo().empresa(), desde, hasta, repo().facturas(desde, hasta),
                                            repo().detalleCompras(desde, hasta));
                });
                break;
            case IDC_R_VENTAS:
                generar = intentar(h_, [&] { doc = docReporteVentas(repo().empresa(), desde, hasta, repo().ventas(desde, hasta)); });
                break;
            case IDC_R_CAJA:
                generar = intentar(h_, [&] { doc = docReporteCaja(repo().empresa(), desde, hasta, repo().cierres(desde, hasta)); });
                break;
            case IDC_R_CIERRE:
                generar = intentar(h_, [&] { doc = docCierreCaja(repo().empresa(), repo().resumenCaja(hasta), repo().movimientos(hasta)); });
                break;
            case IDC_R_DEUDAS:
                generar = intentar(h_, [&] { doc = docDeudasPendientes(repo().empresa(), repo().deudas(FiltroDeudas{}), Fecha::hoy()); });
                break;
            case IDC_R_ESTADO:
                if (const auto c = seleccionarCliente(h_)) generarEstadoCuenta(h_, c->id);
                return true;
            case IDC_R_INFORME:
                verInformeDia(h_, hasta);
                return true;
            case IDC_R_INVENTARIO:
                verInventario(h_, hasta);
                return true;
            case IDC_R_GENERAL:
                generarReporteGeneral(h_, hasta);
                return true;
            case IDC_R_CARPETA:
                abrirCarpeta(App::instancia().rutas().documentos);
                return true;
            default:
                return false;
        }
        if (generar) generarYMostrar(h_, doc);
        return true;
    }

private:
    SelectorPeriodo periodo_;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaReportes() { return std::make_unique<PaginaReportes>(); }

}  // namespace bu::win
