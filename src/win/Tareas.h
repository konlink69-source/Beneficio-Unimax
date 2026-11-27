#pragma once
// Trabajos largos (subir a la nube, descargar una actualización) en segundo plano con una
// ventana de progreso, para que el programa no parezca "colgado".
#include "win/Util.h"

#include <atomic>
#include <functional>
#include <string>

namespace bu::win {

class Avance {
public:
    virtual ~Avance() = default;
    virtual void texto(const std::string& t) = 0;
    // 0..100, o -1 para "sin porcentaje" (barra en movimiento).
    virtual void porcentaje(int p) = 0;
    virtual bool cancelado() const = 0;
};

// Muestra la ventana y ejecuta "trabajo" en otro hilo. Devuelve true si terminó bien;
// si lanzó una excepción, devuelve false y deja el mensaje en "error". Si el usuario canceló,
// "error" queda vacío.
bool ejecutarConProgreso(HWND padre, const std::string& titulo, const std::function<void(Avance&)>& trabajo,
                         std::string& error, bool cancelable = true);

}  // namespace bu::win
