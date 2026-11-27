#pragma once
// Dibujo del comprobante de compra como una lista de elementos (textos, líneas,
// rectángulos e imágenes) en una hoja carta de 850 x 1100 unidades (1/100 de pulgada).
// La misma lista se dibuja en la pantalla, en la impresora y en el PDF (win/Impresion.cpp),
// y en las pruebas se convierte a imagen para revisarla (tools/vista_comprobante.py).
#include <cstdint>
#include <string>
#include <vector>

#include "core/Modelos.h"

namespace bu {

enum class TipoElemento { Texto, Linea, Rectangulo, Imagen };
enum Alineacion { ALINEAR_IZQUIERDA = 0, ALINEAR_CENTRO = 1, ALINEAR_DERECHA = 2 };
constexpr uint32_t SIN_COLOR = 0xFFFFFFFF;

struct ElementoGrafico {
    TipoElemento tipo = TipoElemento::Texto;
    double x = 0, y = 0, ancho = 0, alto = 0;
    std::string texto;
    double puntos = 10;        // tamaño de letra en puntos
    bool negrita = false;
    int alineacion = ALINEAR_IZQUIERDA;
    bool variasLineas = false;  // ajustar el texto al ancho
    uint32_t color = 0x000000;  // 0xRRGGBB (texto, línea o borde)
    uint32_t relleno = SIN_COLOR;
    double grosor = 1;
    std::string imagen;  // "logo", "marca1" o "marca2" (se ajusta al cuadro sin deformarse)
};

struct Lienzo {
    double ancho = 850;
    double alto = 1100;
    std::vector<ElementoGrafico> elementos;

    void texto(double x, double y, double ancho, const std::string& t, double puntos, bool negrita = false,
               uint32_t color = 0x000000, int alineacion = ALINEAR_IZQUIERDA);
    void parrafo(double x, double y, double ancho, double alto, const std::string& t, double puntos,
                 uint32_t color = 0x000000);
    void linea(double x1, double y1, double x2, double y2, uint32_t color = 0x000000, double grosor = 1);
    void rectangulo(double x, double y, double ancho, double alto, uint32_t relleno, uint32_t borde = SIN_COLOR,
                    double grosor = 1);
    void imagen(double x, double y, double ancho, double alto, const std::string& nombre);
};

// Diseños del comprobante. Cuando una actualización cambia el diseño, el anterior se conserva aquí
// (con su número) y se puede seguir usando desde Configuración.
struct DisenoDocumento {
    int numero;
    std::string nombre;
};
constexpr int DISENO_COMPROBANTE_ACTUAL = 1;
std::vector<DisenoDocumento> disenosComprobante();
Lienzo lienzoComprobante(const Empresa& e, const Factura& f, const Cliente& c, int marcaAgua,
                         int diseno = DISENO_COMPROBANTE_ACTUAL);

// Informe del día: libras y quintales de uva y pergamino (hoy, el mes y la cosecha).
Lienzo lienzoInformeDia(const Empresa& e, const Fecha& dia, const ResumenDia& hoy, const ResumenDia& mes,
                        const ResumenDia& cosecha, const Fecha& desdeCosecha);

// Para revisar el dibujo fuera de Windows.
std::string lienzoJson(const Lienzo& l);

}  // namespace bu
