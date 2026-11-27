#pragma once
// Actualizaciones automáticas: al abrir, el programa revisa si hay una versión nueva publicada
// en GitHub (archivo version.txt), muestra qué cambió y la instala con un clic.
// La base de datos, las facturas y las plantillas no se tocan; antes se guarda un respaldo.
#include "win/Util.h"

#include <string>

#include "core/Sincronizacion.h"

namespace bu::win {

// Revisa en otro hilo. Si hay versión nueva (o si "manual" es true) avisa a la ventana con
// WM_APP_ACTUALIZACION: wp = manual, lp = InfoVersion* (el receptor lo borra). Si no se pudo
// revisar, InfoVersion::numero = -1 y "notas" trae el motivo.
void buscarActualizacion(HWND ventana, bool manual);

// Muestra la versión nueva. Devuelve true si se descargó y se lanzó el instalador: el programa
// debe cerrarse enseguida (los datos ya se guardaron).
bool ofrecerActualizacion(HWND ventana, const InfoVersion& v, bool hayTrabajoSinGuardar);

// Llamado al inicio con los argumentos "/actualizar <pid> <destino>": espera a que se cierre la
// versión anterior, la reemplaza y abre la nueva. Devuelve el código de salida.
int completarActualizacion(DWORD pid, const std::wstring& destino, const std::wstring& carpetaDatos);

// Después de actualizar ("/actualizado"): muestra qué cambió.
void mostrarNovedades(HWND ventana);

}  // namespace bu::win
