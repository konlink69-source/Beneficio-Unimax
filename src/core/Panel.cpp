#include "core/Panel.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace bu {

namespace {
int indiceMes(const Fecha& inicio, const Fecha& f) {
    const int i = (f.anio - inicio.anio) * 12 + (f.mes - inicio.mes);
    return std::max(0, std::min(11, i));
}
}  // namespace

DatosPanel calcularPanel(const std::vector<FilaCompra>& filas, const Fecha& hoy) {
    DatosPanel d;
    d.hoy = hoy;
    d.inicioCosecha = inicioCosecha(hoy);
    d.mesActual = indiceMes(d.inicioCosecha, hoy);
    std::vector<FilaCompra> hoyF, mesF;
    std::map<std::string, ProductorPanel> productores;
    for (const auto& f : filas) {
        if (f.fecha < d.inicioCosecha || f.fecha > hoy) continue;
        const int i = indiceMes(d.inicioCosecha, f.fecha);
        if (f.linea.tipoCafe == CAFE_UVA) d.montoUva[static_cast<size_t>(i)] += f.linea.subtotal;
        else d.montoPergamino[static_cast<size_t>(i)] += f.linea.subtotal;
        if (f.fecha == hoy) hoyF.push_back(f);
        if (f.fecha.anio == hoy.anio && f.fecha.mes == hoy.mes) {
            mesF.push_back(f);
            const std::string clave = f.clienteDni.empty() ? f.clienteNombre : f.clienteDni;
            auto& p = productores[clave];
            p.nombre = f.clienteNombre;
            p.libras += f.linea.libras;
            p.monto += f.linea.subtotal;
        }
    }
    d.deHoy = sumarResumenes(resumirPorDia(hoyF));
    d.delMes = sumarResumenes(resumirPorDia(mesF));
    d.deCosecha = sumarResumenes(resumirPorDia(filas));
    for (const auto& p : productores) d.mejoresDelMes.push_back(p.second);
    std::sort(d.mejoresDelMes.begin(), d.mejoresDelMes.end(),
              [](const ProductorPanel& a, const ProductorPanel& b) { return a.monto > b.monto; });
    if (d.mejoresDelMes.size() > 5) d.mejoresDelMes.resize(5);
    return d;
}

const char* nombrePeriodo(int p) {
    static const char* const NOMBRES[] = {"Hoy", "Esta semana", "Este mes", "Mes anterior", "Esta cosecha", "Este año",
                                          "Elegir fechas..."};
    return (p >= 0 && p < NUM_PERIODOS) ? NOMBRES[p] : "";
}

std::pair<Fecha, Fecha> rangoPeriodo(int p, const Fecha& hoy) {
    switch (p) {
        case PERIODO_SEMANA: {
            const long n = hoy.numeroDia();  // 1970-01-01 fue jueves
            const int desdeLunes = static_cast<int>(((n % 7) + 7 + 3) % 7);
            return {hoy.sumarDias(-desdeLunes), hoy};
        }
        case PERIODO_MES: return {hoy.primerDiaDelMes(), hoy};
        case PERIODO_MES_ANTERIOR: {
            const Fecha fin = hoy.primerDiaDelMes().sumarDias(-1);
            return {fin.primerDiaDelMes(), fin};
        }
        case PERIODO_COSECHA: return {inicioCosecha(hoy), hoy};
        case PERIODO_ANIO: return {Fecha(hoy.anio, 1, 1), hoy};
        default: return {hoy, hoy};
    }
}

const char* mesCosecha(int indice) {
    static const char* const NOMBRES[] = {"Oct", "Nov", "Dic", "Ene", "Feb", "Mar",
                                          "Abr", "May", "Jun", "Jul", "Ago", "Sep"};
    return NOMBRES[((indice % 12) + 12) % 12];
}

std::string montoCorto(Centavos c) {
    const double l = static_cast<double>(c) / 100.0;
    const double a = std::fabs(l);
    if (a >= 1e6) return "L " + formatoDecimal(l / 1e6, a >= 1e7 ? 1 : 2) + " M";
    if (a >= 1e4) return "L " + formatoDecimal(l / 1e3, a >= 1e5 ? 0 : 1) + " mil";
    return "L " + formatoDecimal(l, 0);
}

std::string formatoLibras(double libras) {
    const double redondeo = std::round(libras * 10) / 10;
    return formatoDecimal(redondeo, std::fabs(redondeo - std::round(redondeo)) < 1e-9 ? 0 : 1) + " lb";
}

}  // namespace bu
