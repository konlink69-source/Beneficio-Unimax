#pragma once
// Ventanas de diálogo (modales) y el ayudante para ingresar el cliente por DNI.
#include "win/Util.h"

#include <map>
#include <optional>
#include <string>

#include "core/Modelos.h"

namespace bu::win {

// Base para diálogos modales definidos en app.rc.
class DialogoModal {
public:
    virtual ~DialogoModal() = default;
    INT_PTR mostrar(HWND padre, int plantilla);

protected:
    virtual void alIniciar() {}
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
    // Se llama al presionar Aceptar/Enter. Devuelve true para cerrar el diálogo.
    virtual bool alAceptar() { return true; }
    // Colores de fondo (WM_CTLCOLOR...). Devuelve un HBRUSH o 0 para usar los de Windows.
    virtual INT_PTR alColor(UINT msg, HDC dc, HWND control) {
        (void)msg;
        (void)dc;
        (void)control;
        return 0;
    }
    HWND h_ = nullptr;

private:
    static INT_PTR CALLBACK proc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
};

// Campos DNI + nombre (+ teléfono) que buscan al cliente por su DNI.
// Si el DNI ya está registrado, se completa el nombre; si no, el cliente se
// registra al guardar con el nombre escrito.
class EntradaCliente {
public:
    void configurar(HWND dlg, int idDni, int idNombre, int idTelefono, int idInfo, int tipoCliente);
    bool comando(int id, int codigo);  // procesa los eventos de los campos; true si los atendió
    void buscar();                     // busca por el DNI escrito
    void elegir(const std::string& filtro = std::string());  // ventana de búsqueda por nombre o DNI
    // Tecla Enter: en el campo DNI busca por DNI o, si se escribió un nombre, abre la búsqueda
    // por nombre; en el campo Nombre abre la búsqueda si hay clientes parecidos.
    // Devuelve true si atendió la tecla.
    bool alPresionarEnter();
    void poner(const Cliente& c);
    void limpiar();
    bool tieneFoco() const;
    bool hayCliente() const { return cliente_.has_value(); }  // el cliente ya está registrado
    // Devuelve el cliente registrado o lo registra (lanza ErrorNegocio si faltan datos).
    Cliente obtenerORegistrar();

private:
    void info(const std::string& texto, bool advertencia = false);
    HWND dlg_ = nullptr;
    int idDni_ = 0, idNombre_ = 0, idTelefono_ = 0, idInfo_ = 0;
    int tipo_ = CLIENTE_PRODUCTOR;
    std::optional<Cliente> cliente_;
    bool poniendo_ = false;
};

std::optional<Cliente> seleccionarCliente(HWND padre, const std::string& filtroInicial = std::string());
bool dialogoNuevaDeuda(HWND padre);
void dialogoDetalleDeuda(HWND padre, long deudaId);
bool dialogoAbono(HWND padre, long deudaId);
bool dialogoMovimiento(HWND padre, const Fecha& fecha);
std::optional<std::string> pedirTexto(HWND padre, const std::string& titulo, const std::string& pregunta,
                                      const std::string& inicial = std::string());
void dialogoRecordatorio(HWND padre, const Recordatorio& r);

// Color de texto para una deuda según su urgencia (vencida, vence hoy, ...).
COLORREF colorDeuda(const Deuda& d, const Fecha& hoy, int dias);

// Pinta cada fila de un ListView con el color asociado a su dato (LPARAM).
// Se usa al recibir NM_CUSTOMDRAW; devuelve true si atendió el mensaje.
bool colorearFilas(NMHDR* nm, const std::map<LPARAM, COLORREF>& colores, LRESULT& resultado);

}  // namespace bu::win
