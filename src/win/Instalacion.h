#pragma once
// Instalación sencilla: copia el .exe a la carpeta de programas del usuario
// (%LOCALAPPDATA%\Programs\BeneficioUnimax) y crea un ícono en el escritorio
// y en el menú Inicio. No requiere permisos de administrador.
#include "win/Util.h"

#include <string>

namespace bu::win {

std::wstring carpetaInstalacion();
std::wstring exeInstalado();
bool ejecutandoDesdeInstalacion(const std::wstring& exe);

// El usuario respondió "No" a la pregunta de instalar (no se vuelve a preguntar).
bool noPreguntarInstalacion();
void guardarNoPreguntarInstalacion();

// Copia "exeActual" a la carpeta de instalación y crea los íconos.
// Devuelve false si no se pudo copiar el programa ("mensaje" explica por qué).
// Si devuelve true, "mensaje" puede traer un aviso (por ejemplo, si falló el ícono).
bool instalar(const std::wstring& exeActual, std::string& mensaje);

}  // namespace bu::win
