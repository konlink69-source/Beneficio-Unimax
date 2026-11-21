#pragma once
// Llena las plantillas de Excel por automatización COM (requiere Excel instalado).
#include "win/Util.h"

#include <string>

#include "core/Documentos.h"

namespace bu::win {

// Copia la plantilla a Documentos\<subcarpeta>\<nombre>.xlsx, la llena y la abre en Excel.
// Devuelve la ruta del archivo generado. Lanza std::runtime_error si algo falla.
std::wstring generarDocumento(const Documento& doc, bool tambienPdf, bool mostrar);

// Igual que la anterior, mostrando los errores al usuario y un cursor de espera.
bool generarYMostrar(HWND padre, const Documento& doc);

// Estado de cuenta de un cliente (deudas y abonos).
bool generarEstadoCuenta(HWND padre, long clienteId);

}  // namespace bu::win
