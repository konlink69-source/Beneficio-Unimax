#pragma once
// Aviso diario de Windows: una tarea del Programador de tareas ejecuta
// "BeneficioUnimax.exe /recordatorio" a la hora configurada, y el programa
// muestra una notificación con las deudas vencidas y por vencer.
#include "win/Util.h"

#include <string>

namespace bu::win {

bool avisoWindowsActivo();
bool activarAvisoWindows(const std::string& horaHHMM, std::string& error);
bool desactivarAvisoWindows(std::string& error);
bool horaValida(const std::string& horaHHMM);

// Modo "/recordatorio": muestra la notificación y termina.
// "forzar" = mostrarla aunque no haya pendientes (para probar).
int ejecutarModoAviso(HINSTANCE hInst, bool forzar);

// Abre el programa (otra instancia) con los argumentos dados.
void lanzarPrograma(const std::wstring& argumentos);

}  // namespace bu::win
