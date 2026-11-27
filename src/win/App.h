#pragma once
// Estado global del programa: rutas, conexión a Access y repositorio.
#include "win/Util.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
    PAG_INICIO,  // panel con el resumen del día (se muestra al abrir)
    NUM_PAGINAS
};

constexpr UINT WM_APP_INICIO = WM_APP + 1;
constexpr UINT WM_APP_RECORDATORIO = WM_APP + 2;
constexpr UINT WM_APP_IR_PAGINA = WM_APP + 3;
constexpr UINT WM_APP_MARCA_AGUA = WM_APP + 4;  // cambió la marca de agua en Configuración
constexpr UINT WM_APP_NUBE = WM_APP + 5;        // cambió el estado de Google Drive (wp = 1: otro equipo tomó los datos)
constexpr UINT WM_APP_ACTUALIZACION = WM_APP + 6;  // hay una versión nueva (lp = InfoVersion*)
extern const wchar_t* const CLASE_VENTANA;

class App {
public:
    static App& instancia();

    // Prepara carpetas, crea la base de datos si no existe y la abre.
    // "sincronizar": revisa Google Drive antes de abrir (no se usa en el aviso diario de Windows).
    bool iniciar(const std::wstring& carpetaDatos, std::string& error, bool sincronizar = false);
    // Copia la base de datos completa y consistente (cierra la conexión un instante).
    bool instantanea(const std::wstring& destino);
    // Cambia la base de datos por "nuevo"; la actual se mueve a "copiaAnterior". Vuelve a abrirla.
    bool reemplazarBase(const std::wstring& nuevo, const std::wstring& copiaAnterior, std::string& error);
    // Ruta de la base de datos abierta (la copia de consulta si el equipo está en SOLO CONSULTA).
    const std::wstring& baseAbierta() const { return baseAbierta_; }
    Repositorio& repo() { return *repo_; }
    const Rutas& rutas() const { return rutas_; }
    std::wstring rutaPlantilla(const std::string& archivo) const;
    // Versiones de plantillas disponibles: (carpeta relativa a Plantillas, nombre). "" = las actuales.
    std::vector<std::pair<std::string, std::string>> versionesPlantillas() const;
    // Ajustes propios de esta computadora (no viajan a otras por Google Drive), p. ej. la impresora.
    std::string configEquipo(const std::string& clave, const std::string& defecto = "") const;
    void setConfigEquipo(const std::string& clave, const std::string& valor);

    // Copia automática del día en Respaldos\Automáticos (se guardan las 30 más recientes).
    void respaldoAutomatico();
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
    void aplicarSoloConsulta();

    Rutas rutas_;
    std::wstring baseAbierta_;
    db::Conexion conexion_;
    std::unique_ptr<Repositorio> repo_;
    Usuario usuario_;
    bool cajaBaja_ = false;
    Centavos saldoCaja_ = 0;
};

inline Repositorio& repo() { return App::instancia().repo(); }

}  // namespace bu::win
