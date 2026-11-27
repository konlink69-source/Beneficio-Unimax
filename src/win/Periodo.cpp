#include "win/Periodo.h"

#include "win/Tema.h"

namespace bu::win {

void SelectorPeriodo::configurar(HWND dlg, int idLista, int idDesde, int idA, int idHasta, int porDefecto) {
    dlg_ = dlg;
    idLista_ = idLista;
    idDesde_ = idDesde;
    idA_ = idA;
    idHasta_ = idHasta;
    configurarFecha(dlg, idDesde);
    configurarFecha(dlg, idHasta);
    for (int p = 0; p < NUM_PERIODOS; ++p) comboAgregar(dlg, idLista, nombrePeriodo(p), p);
    comboSeleccionar(dlg, idLista, porDefecto);
    actualizar();
}

void SelectorPeriodo::mostrarFechas(bool si) {
    bool cambio = false;
    for (int id : {idDesde_, idA_, idHasta_}) {
        HWND c = GetDlgItem(dlg_, id);
        if (!c || (IsWindowVisible(c) != 0) == si) continue;
        ShowWindow(c, si ? SW_SHOW : SW_HIDE);
        cambio = true;
    }
    if (cambio) tema::refrescar(dlg_);
}

void SelectorPeriodo::actualizar() {
    const int p = static_cast<int>(comboDato(dlg_, idLista_, PERIODO_MES));
    if (p != PERIODO_ELEGIR) {
        const auto r = rangoPeriodo(p, Fecha::hoy());
        ponerFecha(dlg_, idDesde_, r.first);
        ponerFecha(dlg_, idHasta_, r.second);
    }
    mostrarFechas(p == PERIODO_ELEGIR);
}

bool SelectorPeriodo::comando(int id, int codigo) {
    if (id != idLista_ || codigo != CBN_SELCHANGE) return false;
    actualizar();
    return true;
}

bool SelectorPeriodo::notificacion(const NMHDR* nm) {
    return (static_cast<int>(nm->idFrom) == idDesde_ || static_cast<int>(nm->idFrom) == idHasta_) &&
           nm->code == DTN_DATETIMECHANGE;
}

Fecha SelectorPeriodo::desde() const { return leerFecha(dlg_, idDesde_); }
Fecha SelectorPeriodo::hasta() const { return leerFecha(dlg_, idHasta_); }

}  // namespace bu::win
