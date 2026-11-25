#pragma once
// Estado global del programa: rutas, conexión a Access y repositorio.
#include "win/Util.h"

#include <memory>
#include <string>

#include "db/Odbc.h"
#include "db/Repositorio.h"

namespace bu::win {

struct Rutas {
    std::wstring exe;            // ruta completa del programa
    std::wstring carpetaExe;     // carpeta del programa
    std::wstring datos;          // Documentos\BeneficioUnimax (o la indicada con /datos)
    std::wstring baseDatos;      // ...\BeneficioUnimax.accdb
    std::wstring plantillas;     // ...\Plantillas
    std::wstring documentos;     // ...\Documentos (comprobantes y reportes generados)
    std::wstring respaldos;      // ...\Respaldos
    bool datosPersonalizados = false;
};

// Páginas (pestañas) de la ventana principal.
enum IndicePagina {
    PAG_FACTURA = 0,
    PAG_FACTURAS,
    PAG_CLIENTES,
    PAG_PRECIOS,
    PAG_DEUDAS,
    PAG_CAJA,
    PAG_VENTAS,
    PAG_REPORTES,
    PAG_CONFIG,
    NUM_PAGINAS
};

constexpr UINT WM_APP_INICIO = WM_APP + 1;
constexpr UINT WM_APP_RECORDATORIO = WM_APP + 2;
constexpr UINT WM_APP_IR_PAGINA = WM_APP + 3;
constexpr UINT WM_APP_MARCA_AGUA = WM_APP + 4;  // cambió la marca de agua en Configuración
extern const wchar_t* const CLASE_VENTANA;

class App {
public:
    static App& instancia();

    // Prepara carpetas, crea la base de datos si no existe y la abre.
    bool iniciar(const std::wstring& carpetaDatos, std::string& error);
    Repositorio& repo() { return *repo_; }
    const Rutas& rutas() const { return rutas_; }
    std::wstring rutaPlantilla(const std::string& archivo) const;

    // Copia la base de datos a un archivo elegido por el usuario.
    void respaldar(HWND padre);
    void mostrarRecordatorio(HWND padre, bool forzado);
    void irAPagina(int indice);

    // ------------------------------------------------ Sesión
    const Usuario& usuario() const { return usuario_; }
    void establecerUsuario(const Usuario& u);
    bool haySesion() const { return usuario_.id != 0; }

    // ------------------------------------------------ Marca de agua (0 = ninguna, 1 o 2)
    int marcaAgua();
    // Imagen nueva para el fondo de una tabla (la tabla se queda con ella).
    HBITMAP marcaAguaParaLista(int marca);
    std::wstring rutaMarcaAguaPng(int marca) const;
    // Logo muy tenue sobre blanco, del tamaño indicado (para el fondo de una tabla).
    HBITMAP logoTenue(int ancho, int alto);
    // Avisa a la ventana principal para que actualice las tablas.
    void refrescarMarcaAgua();

    // ------------------------------------------------ Alerta de caja baja
    Centavos minimoCaja();
    // Recalcula el dinero en caja de hoy (para el encabezado) y devuelve true si está bajo el mínimo.
    bool actualizarCaja();
    bool cajaBaja() const { return cajaBaja_; }
    Centavos saldoCaja() const { return saldoCaja_; }
    // Después de un pago: muestra la alerta si la caja quedó bajo el mínimo.
    void avisarCajaBaja(HWND padre);

    HINSTANCE hInst = nullptr;
    HWND ventana = nullptr;

private:
    App() = default;
    void abrirBaseDatos();

    Rutas rutas_;
    db::Conexion conexion_;
    std::unique_ptr<Repositorio> repo_;
    Usuario usuario_;
    bool cajaBaja_ = false;
    Centavos saldoCaja_ = 0;
};

inline Repositorio& repo() { return App::instancia().repo(); }

}  // namespace bu::win
