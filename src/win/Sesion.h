#pragma once
// Inicio de sesión, contraseñas y usuarios.
#include "win/Util.h"

#include <string>

namespace bu::win {

// Muestra la ventana de ingreso. Devuelve false si el usuario la cierra sin ingresar.
bool iniciarSesion(HWND padre);

// Pide la contraseña de un administrador antes de una acción delicada (anular, reabrir caja,
// editar clientes, cambiar la configuración...). Si quien está usando el programa es
// administrador, solo confirma su propia contraseña.
bool autorizar(HWND padre, const std::string& accion);

void dialogoCambiarMiContrasena(HWND padre);
void dialogoUsuarios(HWND padre);

}  // namespace bu::win
