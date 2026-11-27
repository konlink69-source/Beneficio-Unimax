#pragma once
// Pestañas de la ventana principal. Cada pestaña es un diálogo hijo (sin borde)
// definido en app.rc.
#include "win/Util.h"

#include <map>
#include <memory>
#include <vector>

namespace bu::win {

class Pagina {
public:
    virtual ~Pagina() = default;
    HWND crear(HWND padre, int plantilla);
    HWND hwnd() const { return h_; }

    virtual const wchar_t* titulo() const = 0;
    virtual void alIniciar() {}
    // Se llama cada vez que se muestra la pestaña (para recargar los datos).
    virtual void alMostrar() {}
    virtual bool alComando(int id, int codigo) {
        (void)id;
        (void)codigo;
        return false;
    }
    virtual bool alNotificar(NMHDR* nm, LRESULT& resultado) {
        (void)nm;
        (void)resultado;
        return false;
    }
    // Tecla Enter dentro de la pestaña.
    virtual void alAceptar() {}
    // Se llama al cambiar el tamaño. Por defecto estira "controlEstirable_" hasta el borde.
    virtual void alRedimensionar(int ancho, int alto);
    // ¿Hay algo a medio hacer (por ejemplo, un comprobante sin guardar)? Se revisa antes de actualizar.
    virtual bool trabajoSinGuardar() const { return false; }
    // Dibujo propio (por ejemplo, el panel de inicio). Devuelve true si dibujó.
    virtual bool alPintar(HDC dc, const RECT& area) {
        (void)dc;
        (void)area;
        return false;
    }
    // Ratón sobre la pestaña (WM_MOUSEMOVE, WM_LBUTTONUP, WM_SETCURSOR). Devuelve true si lo atendió.
    virtual bool alRaton(UINT msg, int x, int y) {
        (void)msg;
        (void)x;
        (void)y;
        return false;
    }
    // Pone (o quita) la marca de agua en la tabla principal de la pestaña.
    void aplicarMarcaAgua(int marca);

protected:
    // Control (normalmente una tabla) que se estira hasta el borde inferior derecho.
    int controlEstirable_ = 0;
    // Colores de texto para etiquetas (STATIC) por identificador.
    std::map<int, COLORREF> coloresTexto_;
    void colorTexto(int id, COLORREF color);
    HWND h_ = nullptr;

    // Estira un control hasta el borde derecho y/o inferior de la pestaña.
    void estirar(int id, int ancho, int alto, bool horizontal, bool vertical);

private:
    // Escalado proporcional: los controles crecen o se achican con la ventana.
    struct Original {
        HWND control;
        RECT rect;
        LOGFONTW letra;
        bool tieneLetra;
        bool combo;
        bool tabla;
        bool marco;
    };
    void guardarDiseno();
    void escalar(int ancho, int alto);
    std::vector<Original> originales_;
    SIZE tamanoDiseno_{0, 0};
    double escalaActual_ = 1.0;
    double escalaX_ = 1.0;
    double escalaY_ = 1.0;

    static INT_PTR CALLBACK proc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
};

std::unique_ptr<Pagina> crearPaginaInicio();
std::unique_ptr<Pagina> crearPaginaFactura();
std::unique_ptr<Pagina> crearPaginaFacturas();
std::unique_ptr<Pagina> crearPaginaClientes();
std::unique_ptr<Pagina> crearPaginaPrecios();
std::unique_ptr<Pagina> crearPaginaDeudas();
std::unique_ptr<Pagina> crearPaginaCaja();
std::unique_ptr<Pagina> crearPaginaVentas();
std::unique_ptr<Pagina> crearPaginaReportes();
std::unique_ptr<Pagina> crearPaginaConfiguracion();

}  // namespace bu::win
