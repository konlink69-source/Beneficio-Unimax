#pragma once
// Buscador de clientes: una sola barra donde se escribe el nombre o el DNI. Mientras se escribe
// aparece una lista con los clientes registrados que coinciden; al elegir uno se muestran sus
// datos (nombre, DNI, teléfono, RTN, carnet y deudas). Al final de la lista está
// "+ Registrar cliente nuevo".
#include "win/Util.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/Modelos.h"

namespace bu::win {

class BuscadorCliente {
public:
    using Colorear = std::function<void(int id, COLORREF color)>;
    void configurar(HWND dlg, int idBusca, int idNombre, int idDatos, int idDeuda, int idCambiar, Colorear colorear,
                    std::function<void()> alElegir);
    bool comando(int id, int codigo);  // true si lo atendió
    void recargar();                   // vuelve a leer los clientes registrados
    void limpiar();
    void poner(const Cliente& c);
    const std::optional<Cliente>& cliente() const { return cliente_; }
    bool tieneFoco() const;
    // Enter en el buscador (con la lista cerrada): la abre o elige el único resultado.
    void alPresionarEnter();

    // ---- Uso interno (lista de sugerencias)
    struct Sugerencia {
        long id = 0;  // 0 = registrar nuevo
        std::string nombre;
        std::string detalle;
    };
    void elegir(int indice);
    void mover(int delta);
    void ocultar();
    bool listaVisible() const;
    const std::vector<Sugerencia>& sugerencias() const { return sugerencias_; }
    int seleccion() const { return seleccion_; }
    void seleccionar(int i);

private:
    void filtrar();
    void mostrarLista();
    void mostrarDatos();

    HWND dlg_ = nullptr;
    int idBusca_ = 0, idNombre_ = 0, idDatos_ = 0, idDeuda_ = 0, idCambiar_ = 0;
    Colorear colorear_;
    std::function<void()> alElegir_;
    std::vector<Cliente> clientes_;
    std::vector<Sugerencia> sugerencias_;
    int seleccion_ = 0;
    HWND lista_ = nullptr;
    std::optional<Cliente> cliente_;
    bool poniendo_ = false;
};

// Ventana para registrar un cliente nuevo (productor). "texto" llena el nombre o el DNI.
std::optional<Cliente> dialogoClienteNuevo(HWND padre, const std::string& texto);

}  // namespace bu::win
