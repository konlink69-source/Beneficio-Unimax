#pragma once
// Utilidades para la interfaz de Windows (Win32).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>

#include <cstdint>
#include <exception>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "core/Dinero.h"
#include "core/Fecha.h"

namespace bu::win {

// ------------------------------------------------ Texto
std::wstring W(const std::string& utf8);
std::string U(const std::wstring& utf16);
std::string formatoPrecio(double precio);  // 2 decimales, o hasta 4 si hacen falta

std::string texto(HWND dlg, int id);
void ponerTexto(HWND dlg, int id, const std::string& s);
std::optional<Centavos> leerMonto(HWND dlg, int id);   // vacío -> nullopt
std::optional<double> leerDecimal(HWND dlg, int id);   // vacío -> nullopt
bool vacio(HWND dlg, int id);
void habilitar(HWND dlg, int id, bool si);
void soloLectura(HWND dlg, int id, bool si);
void enfocar(HWND dlg, int id);

// ------------------------------------------------ Fechas (SysDateTimePick32)
void configurarFecha(HWND dlg, int id);
Fecha leerFecha(HWND dlg, int id);
std::optional<Fecha> leerFechaOpc(HWND dlg, int id);
void ponerFecha(HWND dlg, int id, const Fecha& f);
void ponerFechaOpc(HWND dlg, int id, const std::optional<Fecha>& f);

// ------------------------------------------------ Listas desplegables
void comboAgregar(HWND dlg, int id, const std::string& texto, LPARAM dato);
LPARAM comboDato(HWND dlg, int id, LPARAM porDefecto = -1);
void comboSeleccionar(HWND dlg, int id, LPARAM dato);

// ------------------------------------------------ Tablas (ListView)
struct Columna {
    const wchar_t* titulo;
    int ancho;  // en unidades de diálogo
    bool derecha = false;
};
void lvConfigurar(HWND dlg, int id, std::initializer_list<Columna> columnas);
void lvLimpiar(HWND dlg, int id);
int lvAgregar(HWND dlg, int id, const std::vector<std::string>& celdas, LPARAM dato);
int lvSeleccion(HWND dlg, int id);
LPARAM lvDato(HWND dlg, int id, int fila);
// Dato de la fila seleccionada o "porDefecto" si no hay selección.
LPARAM lvDatoSeleccionado(HWND dlg, int id, LPARAM porDefecto = 0);
void lvSeleccionar(HWND dlg, int id, int fila);

// ------------------------------------------------ Mensajes
void mostrarError(HWND padre, const std::string& msg);
void mostrarAviso(HWND padre, const std::string& msg);
void mostrarInfo(HWND padre, const std::string& msg);
bool confirmar(HWND padre, const std::string& msg);
void mostrarExcepcion(HWND padre, const std::exception& e);

// Ejecuta "accion" mostrando cualquier error. Devuelve true si no hubo errores.
template <class F>
bool intentar(HWND padre, F&& accion) {
    try {
        accion();
        return true;
    } catch (const std::exception& e) {
        mostrarExcepcion(padre, e);
    }
    return false;
}

// ------------------------------------------------ Fuentes y colores
// Tamaño de letra de las ventanas (en puntos). Se elige al iniciar según el tamaño de la
// pantalla, para que las pestañas llenen la ventana (todas las ventanas crecen con la letra).
void establecerTamanoLetra(int puntos);
int tamanoLetra();
HFONT fuente(double puntos, bool negrita);
HFONT fuenteNormal();
HFONT fuenteNegrita();
HFONT fuenteGrande();
HFONT fuenteEtiqueta();  // etiquetas destacadas (DNI, Nombre, Teléfono)
// Copia de una plantilla de diálogo de app.rc con el tamaño de letra actual.
std::vector<BYTE> plantillaDialogo(int id, int puntos);
// Texto gris de ayuda dentro de un campo vacío.
void textoGuia(HWND dlg, int id, const std::wstring& texto);
void ponerFuente(HWND dlg, int id, HFONT f);

constexpr COLORREF COLOR_ROJO = RGB(192, 0, 0);
constexpr COLORREF COLOR_NARANJA = RGB(200, 100, 0);
constexpr COLORREF COLOR_VERDE = RGB(0, 120, 0);
constexpr COLORREF COLOR_CAFE = RGB(74, 30, 27);  // café del logo
constexpr COLORREF COLOR_GRIS = RGB(110, 110, 110);

// ------------------------------------------------ Archivos
bool existeArchivo(const std::wstring& ruta);
void crearCarpetas(const std::wstring& ruta);
void abrirCarpeta(const std::wstring& ruta);
std::wstring carpetaDe(const std::wstring& ruta);

// ------------------------------------------------ Imágenes incluidas en el .exe (BMP)
struct ImagenDib {
    const BITMAPINFO* info = nullptr;
    const void* bits = nullptr;
    int ancho = 0;
    int alto = 0;
};
ImagenDib imagenRecurso(int id);

// ------------------------------------------------ Números aleatorios seguros (sal y códigos)
std::vector<uint8_t> bytesAleatorios(size_t n);

// Cursor de espera mientras exista el objeto.
class CursorEspera {
public:
    CursorEspera() : anterior_(SetCursor(LoadCursor(nullptr, IDC_WAIT))) {}
    ~CursorEspera() { SetCursor(anterior_); }

private:
    HCURSOR anterior_;
};

}  // namespace bu::win
