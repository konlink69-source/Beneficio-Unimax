// Pestaña "Caja": saldo del día anterior, entradas, salidas, dinero contado y cierre.
#include "core/Documentos.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Excel.h"
#include "win/Paginas.h"
#include "win/Sesion.h"
#include "win/resource.h"

namespace bu::win {

namespace {

class PaginaCaja : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Caja"; }

    void alIniciar() override {
        controlEstirable_ = IDC_K_LISTA;
        configurarFecha(h_, IDC_K_FECHA);
        ponerFecha(h_, IDC_K_FECHA, Fecha::hoy());
        lvConfigurar(h_, IDC_K_LISTA,
                     {{L"Hora", 36},
                      {L"Tipo", 44},
                      {L"Origen", 76},
                      {L"Concepto", 200},
                      {L"Referencia", 80},
                      {L"Entrada", 70, true},
                      {L"Salida", 70, true},
                      {L"Usuario", 54}});
        ponerFuente(h_, IDC_K_ESTADO, fuenteNegrita());
        ponerFuente(h_, IDC_K_ESPERADO, fuenteGrande());
        ponerFuente(h_, IDC_K_DIFERENCIA, fuenteNegrita());
        colorTexto(IDC_K_ESPERADO, COLOR_CAFE);
        colorTexto(IDC_K_AVISO, COLOR_ROJO);
    }

    void alMostrar() override { cargar(true); }

    bool alComando(int id, int codigo) override {
        switch (id) {
            case IDC_K_HOY:
                ponerFecha(h_, IDC_K_FECHA, Fecha::hoy());
                cargar(true);
                return true;
            case IDC_K_CONTADO:
                if (codigo == EN_CHANGE) actualizarDiferencia();
                return true;
            case IDC_K_CERRAR:
                cerrar();
                return true;
            case IDC_K_REABRIR:
                reabrir();
                return true;
            case IDC_K_MOV:
                if (dialogoMovimiento(h_, leerFecha(h_, IDC_K_FECHA))) cargar(false);
                return true;
            case IDC_K_ELIMINAR:
                eliminar();
                return true;
            case IDC_K_REPORTE:
                imprimir();
                return true;
            default:
                return false;
        }
    }

    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if (nm->idFrom == IDC_K_FECHA && nm->code == DTN_DATETIMECHANGE) {
            cargar(true);
            r = 0;
            return true;
        }
        if (nm->idFrom == IDC_K_LISTA) return colorearFilas(nm, colores_, r);
        return false;
    }

    void alAceptar() override {
        if (GetFocus() == GetDlgItem(h_, IDC_K_CONTADO) && IsWindowEnabled(GetDlgItem(h_, IDC_K_CERRAR))) cerrar();
    }

private:
    void cargar(bool limpiarContado) {
        const Fecha f = leerFecha(h_, IDC_K_FECHA);
        resumen_ = repo().resumenCaja(f);
        ponerTexto(h_, IDC_K_INICIAL, formatoLempiras(resumen_.cierre ? resumen_.cierre->saldoInicial : resumen_.saldoInicial));
        ponerTexto(h_, IDC_K_INGRESOS, formatoLempiras(resumen_.ingresos));
        ponerTexto(h_, IDC_K_EGRESOS, formatoLempiras(resumen_.egresos));
        ponerTexto(h_, IDC_K_ESPERADO, formatoLempiras(resumen_.cierre ? resumen_.cierre->saldoEsperado : resumen_.esperado));

        const bool cerrada = resumen_.cierre.has_value();
        if (cerrada) {
            ponerTexto(h_, IDC_K_ESTADO, "CAJA CERRADA (" + resumen_.cierre->hora.substr(0, 5) + ")");
            colorTexto(IDC_K_ESTADO, COLOR_ROJO);
            ponerTexto(h_, IDC_K_CONTADO, formatoMonto(resumen_.cierre->efectivoContado));
            ponerTexto(h_, IDC_K_NOTAS, resumen_.cierre->notas);
        } else if (resumen_.bloqueada) {
            ponerTexto(h_, IDC_K_ESTADO, "BLOQUEADA (hay un cierre posterior)");
            colorTexto(IDC_K_ESTADO, COLOR_GRIS);
        } else {
            ponerTexto(h_, IDC_K_ESTADO, "CAJA ABIERTA");
            colorTexto(IDC_K_ESTADO, COLOR_VERDE);
            if (limpiarContado) {
                ponerTexto(h_, IDC_K_CONTADO, "");
                ponerTexto(h_, IDC_K_NOTAS, "");
            }
        }
        soloLectura(h_, IDC_K_CONTADO, cerrada || resumen_.bloqueada);
        soloLectura(h_, IDC_K_NOTAS, cerrada || resumen_.bloqueada);
        habilitar(h_, IDC_K_CERRAR, !resumen_.bloqueada);
        habilitar(h_, IDC_K_REABRIR, cerrada);
        habilitar(h_, IDC_K_MOV, !resumen_.bloqueada);
        habilitar(h_, IDC_K_ELIMINAR, !resumen_.bloqueada);
        actualizarDiferencia();

        std::string aviso;
        if (!resumen_.cierreAnterior)
            aviso = "Primer día de uso: el saldo inicial es el \"Saldo inicial de la caja\" de la pestaña Configuración.";
        if (const auto pendiente = repo().diaSinCerrar(f))
            aviso = "¡Atención! La caja del " + pendiente->texto() +
                    " no se cerró; sus movimientos ya están sumados en el saldo inicial de este día.";
        if (f == Fecha::hoy()) {
            App::instancia().actualizarCaja();
            if (App::instancia().cajaBaja())
                aviso = "¡Atención! Hay " + formatoLempiras(resumen_.cierre ? resumen_.cierre->efectivoContado : resumen_.esperado) +
                        " en caja, menos del mínimo de " + formatoLempiras(App::instancia().minimoCaja()) + ". " + aviso;
        }
        ponerTexto(h_, IDC_K_AVISO, aviso);

        lvLimpiar(h_, IDC_K_LISTA);
        colores_.clear();
        for (const auto& m : repo().movimientos(f)) {
            const bool entrada = m.tipo == TipoMovimiento::Ingreso;
            lvAgregar(h_, IDC_K_LISTA,
                      {m.hora.substr(0, 5), entrada ? "Entrada" : "Salida", nombreOrigenMovimiento(m.origen), m.concepto,
                       m.referencia, entrada ? formatoLempiras(m.monto) : std::string(),
                       entrada ? std::string() : formatoLempiras(m.monto), m.usuario},
                      m.id);
            colores_[m.id] = entrada ? COLOR_VERDE : COLOR_ROJO;
        }
    }

    void actualizarDiferencia() {
        const Centavos esperado = resumen_.cierre ? resumen_.cierre->saldoEsperado : resumen_.esperado;
        if (vacio(h_, IDC_K_CONTADO)) {
            ponerTexto(h_, IDC_K_DIFERENCIA, "Cuente el dinero y escríbalo.");
            colorTexto(IDC_K_DIFERENCIA, COLOR_GRIS);
            return;
        }
        const auto contado = leerMonto(h_, IDC_K_CONTADO);
        if (!contado) {
            ponerTexto(h_, IDC_K_DIFERENCIA, "Monto inválido");
            colorTexto(IDC_K_DIFERENCIA, COLOR_ROJO);
            return;
        }
        const Centavos d = *contado - esperado;
        if (d == 0) {
            ponerTexto(h_, IDC_K_DIFERENCIA, "Cuadra exacto");
            colorTexto(IDC_K_DIFERENCIA, COLOR_VERDE);
        } else {
            ponerTexto(h_, IDC_K_DIFERENCIA, (d > 0 ? "Sobran " : "Faltan ") + formatoLempiras(d > 0 ? d : -d));
            colorTexto(IDC_K_DIFERENCIA, COLOR_ROJO);
        }
    }

    void cerrar() {
        const Fecha f = leerFecha(h_, IDC_K_FECHA);
        const auto contado = leerMonto(h_, IDC_K_CONTADO);
        if (!contado) {
            mostrarAviso(h_, "Escriba el dinero contado en caja antes de cerrar.");
            enfocar(h_, IDC_K_CONTADO);
            return;
        }
        const Centavos diferencia = *contado - resumen_.esperado;
        std::string pregunta = "¿Cerrar la caja del " + f.texto() + "?\n\nDebe haber: " + formatoLempiras(resumen_.esperado) +
                               "\nContado: " + formatoLempiras(*contado) + "\nDiferencia: " + formatoLempiras(diferencia) +
                               "\n\nDespués del cierre no se pueden registrar más movimientos con esa fecha "
                               "(salvo que reabra la caja). Lo contado será el saldo inicial del día siguiente.";
        if (!confirmar(h_, pregunta)) return;
        if (!intentar(h_, [&] { repo().cerrarCaja(f, *contado, texto(h_, IDC_K_NOTAS)); })) return;
        cargar(false);
        if (confirmar(h_, "Caja cerrada. ¿Desea abrir el reporte del cierre en Excel para imprimirlo?")) imprimir();
    }

    void reabrir() {
        const Fecha f = leerFecha(h_, IDC_K_FECHA);
        if (!confirmar(h_, "¿Reabrir la caja del " + f.texto() + "? Se borrará el cierre y podrá registrar movimientos de nuevo."))
            return;
        if (!autorizar(h_, "reabrir la caja")) return;
        if (intentar(h_, [&] { repo().reabrirCaja(f); })) cargar(false);
    }

    void eliminar() {
        const long id = static_cast<long>(lvDatoSeleccionado(h_, IDC_K_LISTA, 0));
        if (!id) {
            mostrarAviso(h_, "Seleccione el movimiento manual que desea eliminar.");
            return;
        }
        if (!confirmar(h_, "¿Eliminar el movimiento seleccionado?")) return;
        if (!autorizar(h_, "eliminar un movimiento de caja")) return;
        if (intentar(h_, [&] { repo().eliminarMovimientoManual(id); })) cargar(false);
    }

    void imprimir() {
        const Fecha f = leerFecha(h_, IDC_K_FECHA);
        Documento doc;
        if (intentar(h_, [&] { doc = docCierreCaja(repo().empresa(), repo().resumenCaja(f), repo().movimientos(f)); }))
            generarYMostrar(h_, doc);
    }

    ResumenCaja resumen_;
    std::map<LPARAM, COLORREF> colores_;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaCaja() { return std::make_unique<PaginaCaja>(); }

}  // namespace bu::win
