#pragma once
// Selector de período: lista (Hoy, Esta semana, Este mes, ...) y, solo con "Elegir fechas...",
// las fechas Desde / Hasta.
#include "win/Util.h"

#include "core/Panel.h"

namespace bu::win {

class SelectorPeriodo {
public:
    void configurar(HWND dlg, int idLista, int idDesde, int idA, int idHasta, int porDefecto);
    // Devuelven true si el período cambió (hay que volver a buscar).
    bool comando(int id, int codigo);
    bool notificacion(const NMHDR* nm);
    // Pone las fechas del período elegido según el día de hoy (al mostrar la pestaña).
    void actualizar();
    Fecha desde() const;
    Fecha hasta() const;

private:
    void mostrarFechas(bool si);
    HWND dlg_ = nullptr;
    int idLista_ = 0, idDesde_ = 0, idA_ = 0, idHasta_ = 0;
};

}  // namespace bu::win
