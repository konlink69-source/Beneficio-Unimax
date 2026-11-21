// Pestaña "Precio del día": precio por libra (o por quintal) de cada tipo de café.
#include "win/App.h"
#include "win/Paginas.h"
#include "win/resource.h"

namespace bu::win {

namespace {

const int ID_LIBRA[NUM_TIPOS_CAFE + 1] = {0, IDC_P_LB1, IDC_P_LB2, IDC_P_LB3};
const int ID_QUINTAL[NUM_TIPOS_CAFE + 1] = {0, IDC_P_QQ1, IDC_P_QQ2, IDC_P_QQ3};

class PaginaPrecios : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Precio del día"; }

    void alIniciar() override {
        controlEstirable_ = IDC_P_LISTA;
        configurarFecha(h_, IDC_P_FECHA);
        ponerFecha(h_, IDC_P_FECHA, Fecha::hoy());
        lvConfigurar(h_, IDC_P_LISTA,
                     {{L"Fecha", 60},
                      {L"Uva (L/lb)", 66, true},
                      {L"P. húmedo (L/lb)", 74, true},
                      {L"P. seco (L/lb)", 70, true},
                      {L"Uva (L/qq)", 70, true},
                      {L"P. húmedo (L/qq)", 76, true},
                      {L"P. seco (L/qq)", 74, true}});
        ponerFuente(h_, IDC_P_ESTADO, fuenteNegrita());
    }

    void alMostrar() override { cargar(); }

    bool alComando(int id, int codigo) override {
        if (id == IDC_P_GUARDAR) {
            guardar();
            return true;
        }
        if (codigo != EN_CHANGE || sincronizando_) return false;
        for (int t = 1; t <= NUM_TIPOS_CAFE; ++t) {
            if (id == ID_LIBRA[t]) {
                sincronizar(ID_LIBRA[t], ID_QUINTAL[t], LIBRAS_POR_QUINTAL);
                return true;
            }
            if (id == ID_QUINTAL[t]) {
                sincronizar(ID_QUINTAL[t], ID_LIBRA[t], 1.0 / LIBRAS_POR_QUINTAL);
                return true;
            }
        }
        return false;
    }

    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if (nm->idFrom == IDC_P_FECHA && nm->code == DTN_DATETIMECHANGE) {
            cargar();
            r = 0;
            return true;
        }
        return false;
    }

    void alAceptar() override { guardar(); }

private:
    // Escribe en "destino" el valor de "origen" multiplicado por "factor".
    void sincronizar(int origen, int destino, double factor) {
        sincronizando_ = true;
        const auto v = leerDecimal(h_, origen);
        ponerTexto(h_, destino, v && *v > 0 ? formatoPrecio(redondear(*v * factor, 4)) : std::string());
        sincronizando_ = false;
    }

    void ponerPrecios(const PreciosDia& p) {
        sincronizando_ = true;
        for (int t = 1; t <= NUM_TIPOS_CAFE; ++t) {
            ponerTexto(h_, ID_LIBRA[t], p.tiene(t) ? formatoPrecio(p.precioLibra[t]) : std::string());
            ponerTexto(h_, ID_QUINTAL[t], p.tiene(t) ? formatoPrecio(p.precioLibra[t] * LIBRAS_POR_QUINTAL) : std::string());
        }
        sincronizando_ = false;
    }

    void cargar() {
        const Fecha f = leerFecha(h_, IDC_P_FECHA);
        const PreciosDia p = repo().preciosDelDia(f);
        if (!p.vacio()) {
            ponerPrecios(p);
            ponerTexto(h_, IDC_P_ESTADO, "Precios guardados para esta fecha.");
            colorTexto(IDC_P_ESTADO, COLOR_VERDE);
        } else {
            const PreciosDia u = repo().ultimosPrecios(f);
            ponerPrecios(u);
            if (u.vacio())
                ponerTexto(h_, IDC_P_ESTADO, "Sin precios. Escríbalos y presione Guardar.");
            else
                ponerTexto(h_, IDC_P_ESTADO, "Sin guardar. Se muestran los del " + u.fecha.texto() + ".");
            colorTexto(IDC_P_ESTADO, COLOR_ROJO);
        }
        lvLimpiar(h_, IDC_P_LISTA);
        const Fecha hasta = f > Fecha::hoy() ? f : Fecha::hoy();
        for (const auto& h : repo().historialPrecios(hasta.sumarDias(-90), hasta)) {
            std::vector<std::string> celdas = {h.fecha.texto()};
            for (int t = 1; t <= NUM_TIPOS_CAFE; ++t)
                celdas.push_back(h.tiene(t) ? formatoPrecio(h.precioLibra[t]) : std::string("—"));
            for (int t = 1; t <= NUM_TIPOS_CAFE; ++t)
                celdas.push_back(h.tiene(t) ? formatoPrecio(h.precioLibra[t] * LIBRAS_POR_QUINTAL) : std::string("—"));
            lvAgregar(h_, IDC_P_LISTA, celdas, 0);
        }
    }

    void guardar() {
        PreciosDia p;
        p.fecha = leerFecha(h_, IDC_P_FECHA);
        for (int t = 1; t <= NUM_TIPOS_CAFE; ++t) {
            if (vacio(h_, ID_LIBRA[t])) continue;
            const auto v = leerDecimal(h_, ID_LIBRA[t]);
            if (!v || *v < 0) {
                mostrarAviso(h_, std::string("El precio de ") + nombreTipoCafe(t) + " no es válido.");
                enfocar(h_, ID_LIBRA[t]);
                return;
            }
            p.precioLibra[t] = *v;
        }
        if (intentar(h_, [&] { repo().guardarPrecios(p); })) {
            cargar();
            mostrarInfo(h_, "Precios del " + p.fecha.texto() + " guardados.");
        }
    }

    bool sincronizando_ = false;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaPrecios() { return std::make_unique<PaginaPrecios>(); }

}  // namespace bu::win
