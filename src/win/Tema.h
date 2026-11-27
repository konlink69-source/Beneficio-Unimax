#pragma once
// Estilo visual moderno y minimalista con la paleta del logo:
//   café oscuro #4A1E1B, café claro #8A5A3C, crema #F1EADB.
// - Fondo crema muy claro; los marcos (GROUPBOX) se dibujan como tarjetas blancas redondeadas.
// - Botones planos con esquinas redondeadas (el principal en café, los demás blancos con borde).
// - Tablas sin borde 3D, dentro de una tarjeta, con la selección de Windows moderna.
#include "win/Util.h"

#include <string>

namespace bu::win::tema {

constexpr COLORREF FONDO = RGB(247, 243, 236);
constexpr COLORREF TARJETA = RGB(255, 255, 255);
constexpr COLORREF BORDE = RGB(230, 222, 210);
constexpr COLORREF CAFE = RGB(74, 30, 27);
constexpr COLORREF CAFE_HOVER = RGB(101, 48, 41);
constexpr COLORREF CAFE_CLARO = RGB(138, 90, 60);
constexpr COLORREF CREMA = RGB(241, 234, 219);
constexpr COLORREF SELECCION = RGB(246, 238, 226);
constexpr COLORREF TEXTO = RGB(43, 33, 30);
constexpr COLORREF TEXTO_SUAVE = RGB(125, 112, 103);
constexpr COLORREF VERDE = RGB(22, 135, 66);
constexpr COLORREF ROJO = RGB(205, 55, 45);
constexpr COLORREF VINO = RGB(155, 27, 48);
constexpr COLORREF AMBAR = RGB(196, 120, 20);

// Inicio y fin de GDI+ (dibujo suavizado). Llamar al abrir y al cerrar el programa.
void iniciar();
void terminar();

// Convierte los controles de una ventana (pestaña o diálogo) al estilo nuevo. Llamar después de
// crear los controles (WM_INITDIALOG).
void preparar(HWND dlg);
// Fondo distinto al crema claro (por ejemplo, la ventana de ingreso). Llamar antes de preparar().
void usarFondo(HWND dlg, COLORREF color);
COLORREF fondoDe(HWND dlg);
// La ventana se cerró: olvida sus datos.
void olvidar(HWND dlg);

// Manejo de mensajes. Devuelven true si atendieron el mensaje.
bool pintarFondo(HWND dlg, HDC dc);                       // WM_ERASEBKGND
INT_PTR colorControl(HWND dlg, UINT msg, HDC dc, HWND control, COLORREF texto);  // WM_CTLCOLOR*
bool dibujarBoton(const DRAWITEMSTRUCT* d);                // WM_DRAWITEM

// ---- Dibujo (suavizado con GDI+)
void rectRedondeado(HDC dc, const RECT& r, int radio, COLORREF relleno, COLORREF borde, int grosor = 1);
void tarjeta(HDC dc, const RECT& r);                       // tarjeta blanca con borde y sombra leve
void circulo(HDC dc, int cx, int cy, int radio, COLORREF color);
// Anillo (gráfico de dona): ángulos en grados, 0 = arriba, sentido del reloj.
void arco(HDC dc, int cx, int cy, int radio, int grosor, double desde, double barrido, COLORREF color);
void linea(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int grosor = 1);
// Texto en una línea dentro de "r" con la alineación de DrawText (DT_LEFT, DT_RIGHT, DT_CENTER...).
void texto(HDC dc, const std::string& t, RECT r, HFONT f, COLORREF color, UINT formato = DT_LEFT);
int altoTexto(HDC dc, HFONT f);
int anchoTexto(HDC dc, HFONT f, const std::string& t);

HBRUSH pincel(COLORREF c);
// Fuente de íconos de Windows (Segoe Fluent Icons / Segoe MDL2 Assets); nullptr si no existe.
HFONT fuenteIconos(int pixeles);

}  // namespace bu::win::tema
