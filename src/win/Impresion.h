#pragma once
// Comprobante de compra dentro del programa: dibujo en pantalla, impresión directa
// (sin cuadros de diálogo) y copia en PDF en la carpeta de cada cliente.
#include "win/Util.h"

#include <string>
#include <vector>

#include "core/Grafico.h"

namespace bu::win {

// Dibuja el lienzo dentro de "destino" (en píxeles del dispositivo: pantalla o impresora).
void dibujarLienzo(HDC dc, const Lienzo& l, const RECT& destino);

std::vector<std::wstring> impresoras();
std::wstring impresoraPredeterminada();
// Impresora elegida en Configuración (o la predeterminada de Windows).
std::wstring impresoraConfigurada();

// Imprime sin preguntar. "archivoSalida" no vacío = escribir a ese archivo (para PDF).
bool imprimirLienzo(const Lienzo& l, const std::wstring& impresora, int copias, const std::wstring& documento,
                    const std::wstring& archivoSalida, std::string& error);

// Carpeta de las facturas de un cliente: Documentos\Facturas\<Nombre (DNI)>
std::wstring carpetaFacturasCliente(const Cliente& c);

// Comprobante listo para dibujar (con la marca de agua configurada).
Lienzo lienzoDeFactura(long facturaId, Factura* factura = nullptr, Cliente* cliente = nullptr);

// Imprime el comprobante en la impresora configurada. Devuelve el nombre de la impresora o lanza error.
std::wstring imprimirComprobante(long facturaId);
// Ruta donde está (o estaría) el PDF de una factura.
std::wstring rutaPdfComprobante(long facturaId);
// Guarda el PDF en la carpeta del cliente (con "Microsoft Print to PDF"). Devuelve la ruta o lanza error.
std::wstring guardarPdfComprobante(long facturaId);
// Muestra el comprobante en una ventana del programa (con botones Imprimir, PDF, carpeta y Excel).
void verComprobante(HWND padre, long facturaId);

// ¿Es una "impresora" que guarda archivos (PDF, XPS, OneNote, fax) en vez de imprimir en papel?
bool esImpresoraVirtual(const std::wstring& nombre);

// Informe del día (uva y pergamino por separado: hoy, el mes y la cosecha), dentro del programa.
void verInformeDia(HWND padre, const Fecha& dia);
// Reporte general en Excel: una fila por día desde el inicio de la cosecha hasta "hasta".
void generarReporteGeneral(HWND padre, const Fecha& hasta);

}  // namespace bu::win
