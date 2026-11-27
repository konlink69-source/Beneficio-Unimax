#pragma once
// Sincronización con Google Drive: respaldos automáticos y seguir trabajando desde otra computadora.
//
//  * Al abrir: si otra computadora subió datos más nuevos, se descargan (los de aquí quedan como
//    respaldo local). Si los datos están abiertos en otra computadora, este equipo entra en modo
//    SOLO CONSULTA con la última copia de la nube (no se puede cambiar nada).
//  * Mientras se trabaja: cada hora (si hubo cambios) y al cerrar la caja se sube la base de datos
//    y se guarda un respaldo con fecha.
//  * Al salir: se suben los cambios, se guarda un respaldo y se libera el equipo.
// Sin Internet se trabaja normal; los cambios se suben cuando vuelva la conexión.
#include "win/Util.h"

#include <atomic>
#include <mutex>
#include <string>

#include "core/Sincronizacion.h"

namespace bu::win {

class Avance;

class Nube {
public:
    static Nube& instancia();

    // ¿Hay una cuenta de Google conectada en esta computadora?
    bool conectada();
    bool soloConsulta() const { return soloConsulta_; }
    std::string motivoSoloConsulta() const { return motivoSoloConsulta_; }
    // Línea corta para el encabezado ("Nube: al día (15:30)", "Nube: sin Internet"...). Vacía si no hay nube.
    std::string resumen();

    // Antes de abrir la base de datos (y antes de pedir la contraseña). "baseDatos" puede cambiar
    // a la copia de consulta si los datos están abiertos en otra computadora.
    void alIniciar(std::wstring& baseDatos);
    // Cada minuto (temporizador de la ventana principal): latido, subida cada hora, reintentos.
    void cadaMinuto();
    // Después de cerrar la caja: sube y guarda un respaldo con fecha (en segundo plano).
    void respaldoCierreCaja();
    // Al salir del programa (o antes de actualizarlo). Muestra el progreso.
    void alSalir(HWND padre, const std::string& motivo);
    // Menú "Nube".
    void sincronizarAhora(HWND padre);
    void dialogoNube(HWND padre);
    void dialogoRespaldos(HWND padre);

    // Otra computadora tomó los datos mientras este equipo estaba sin conexión (WM_APP_NUBE, wp = 1).
    void perdioBloqueo(HWND padre);
    // Primera conexión con el programa abierto.
    void despuesDeConectar(HWND padre);
    // Cambia los datos por un respaldo de Drive (los actuales se guardan antes como respaldo).
    bool restaurar(HWND padre, const std::string& idRespaldo, std::string& error);
    std::string idRespaldos();
    std::string ultimoError();

    // La base de datos cambió (lo llama la conexión).
    void marcarCambio();

private:
    Nube() = default;
    void cargarLocal();
    void guardarLocal();
    std::string carpetaRaiz();              // id de "Beneficio Unimax" en Mi unidad
    std::string subcarpeta(const std::string& nombre);
    bool leerEstado(EstadoNube& e, std::string& idEstado, std::string& idDatos);
    void escribirEstado(const EstadoNube& e, const std::string& idEstado);
    Bloqueo miBloqueo(Momento desde);
    // Sube el archivo indicado como la base de datos actual (versión nueva) y, si hay motivo,
    // guarda una copia con fecha en "Respaldos".
    void subirDatos(const std::wstring& archivo, const std::string& motivo, bool liberar, unsigned long cambiosHasta,
                    Avance* avance);
    void respaldoEnNube(const std::string& idDatos, const std::string& motivo);
    void limpiarRespaldos();
    void subirArchivosNuevos(Avance* avance);
    void bajarPlantillasFaltantes();
    void enSegundoPlano(const std::string& motivo, bool conCopia);
    std::wstring instantanea();
    void conectarInteractivo(HWND padre);
    void tomarBloqueo();
    void esperarLibre();

    std::recursive_mutex m_;
    std::atomic<bool> ocupado_{false};     // una operación con Drive a la vez
    std::string respaldoPendiente_;        // respaldo con fecha que no se pudo hacer todavía
    EstadoLocal local_;
    bool localCargado_ = false;
    std::string idRaiz_, idDatos_, idEstado_;
    std::atomic<bool> soloConsulta_{false};
    std::string motivoSoloConsulta_;
    std::atomic<unsigned long> cambios_{0};  // cambios desde que abrió el programa
    std::atomic<bool> sinInternet_{false};
    std::atomic<bool> nubeMasNueva_{false};  // otra computadora subió datos mientras tanto
    std::atomic<bool> tengoBloqueo_{false};
    std::string ultimoError_;
    std::atomic<Momento> desdeBloqueo_{0};
    std::atomic<Momento> ultimoLatido_{0};
    Momento ultimaSubidaHora_ = 0;
    std::atomic<bool> iniciada_{false};
};

// Desfase de la hora local respecto a UTC (en segundos) y hora actual UTC.
long desfaseLocal();
Momento ahoraUtc();
// Nombre de esta computadora.
std::string nombreEquipo();

}  // namespace bu::win
