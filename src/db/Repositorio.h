#pragma once
// Todas las operaciones del negocio sobre la base de datos.
// Cada operación que mueve dinero registra automáticamente el movimiento de caja
// y las deudas que correspondan, dentro de una transacción.
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/Modelos.h"
#include "core/Seguridad.h"
#include "db/Esquema.h"
#include "db/Odbc.h"

namespace bu {

// Error de validación del negocio (mensaje listo para mostrar al usuario).
class ErrorNegocio : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct FiltroDeudas {
    int tipo = 0;  // 0 = todas, 1 = por pagar, 2 = por cobrar
    bool soloPendientes = true;
    long clienteId = 0;
};

enum class ResultadoEliminar { Eliminado, Desactivado };

class Repositorio {
public:
    Repositorio(db::Conexion& c, db::Dialecto d) : c_(c), d_(d) {}

    // Usuario que queda registrado en cada operación (el que inició sesión).
    void usarUsuario(const std::string& usuario) { usuario_ = usuario; }
    const std::string& usuarioActual() const { return usuario_; }
    // Para las pruebas: menos iteraciones del cifrado de contraseñas.
    void usarIteracionesHash(uint32_t n) { iteraciones_ = n; }

    // ------------------------------------------------ Usuarios
    // Si no hay usuarios, crea el usuario maestro "admin" con la contraseña inicial.
    void asegurarUsuarioMaestro(const std::vector<uint8_t>& sal);
    std::vector<Usuario> usuarios();
    std::optional<Usuario> usuarioPorId(long id);
    // Devuelve el usuario si el nombre y la contraseña son correctos (y está activo).
    std::optional<Usuario> autenticar(const std::string& usuario, const std::string& contrasena);
    long crearUsuario(Usuario& u, const std::string& contrasena, const std::vector<uint8_t>& sal);
    void guardarUsuario(const Usuario& u);  // nombre, rol y si está activo
    void cambiarContrasena(long id, const std::string& nueva, const std::vector<uint8_t>& sal, bool debeCambiar = false);
    // Código para recuperar el acceso si se olvida la contraseña del administrador.
    void guardarCodigoRecuperacion(const std::string& codigo, const std::vector<uint8_t>& sal);
    bool verificarCodigoRecuperacion(const std::string& codigo);

    // ------------------------------------------------ Configuración
    std::string config(const std::string& clave, const std::string& porDefecto = std::string());
    long configEntero(const std::string& clave, long porDefecto);
    void setConfig(const std::string& clave, const std::string& valor);
    Empresa empresa();

    // ------------------------------------------------ Clientes
    std::vector<Cliente> clientes(bool incluirInactivos = false);
    std::optional<Cliente> clientePorId(long id);
    std::optional<Cliente> clientePorDni(const std::string& dni);  // acepta con o sin guiones
    long guardarCliente(Cliente& c);  // inserta o actualiza; valida DNI/RTN
    ResultadoEliminar eliminarCliente(long id);
    // Busca por DNI; si no existe lo registra con el nombre dado.
    Cliente obtenerORegistrarCliente(const std::string& dni, const std::string& nombre,
                                     const std::string& telefono, int tipo);

    // ------------------------------------------------ Precios del día
    void guardarPrecios(const PreciosDia& p);
    PreciosDia preciosDelDia(const Fecha& f);
    // Los precios más recientes con fecha igual o anterior a "f" (vacío si no hay).
    PreciosDia ultimosPrecios(const Fecha& f);
    std::vector<PreciosDia> historialPrecios(const Fecha& desde, const Fecha& hasta);

    // ------------------------------------------------ Compras (comprobantes)
    long siguienteNumeroFactura(int categoria);
    long registrarFactura(Factura& f);
    std::optional<Factura> facturaPorId(long id);
    std::vector<Factura> facturas(const Fecha& desde, const Fecha& hasta);
    std::vector<FilaCompra> detalleCompras(const Fecha& desde, const Fecha& hasta);
    void anularFactura(long id, const std::string& motivo, const Fecha& hoy);

    // ------------------------------------------------ Ventas
    long siguienteNumeroVenta();
    long registrarVenta(Venta& v);
    std::vector<Venta> ventas(const Fecha& desde, const Fecha& hasta);
    void anularVenta(long id, const std::string& motivo, const Fecha& hoy);

    // ------------------------------------------------ Deudas
    long registrarDeuda(Deuda& d, bool afectaCaja);
    std::vector<Deuda> deudas(const FiltroDeudas& filtro);
    std::optional<Deuda> deudaPorId(long id);
    long registrarAbono(long deudaId, const Fecha& fecha, Centavos monto, const std::string& nota);
    std::vector<Abono> abonos(long deudaId);
    void anularAbono(long abonoId, const Fecha& hoy);
    void anularDeuda(long deudaId, const Fecha& hoy);
    ResumenDeudas resumenDeudas(long clienteId = 0);
    std::map<long, ResumenDeudas> resumenDeudasPorCliente();

    // ------------------------------------------------ Caja
    bool cajaBloqueada(const Fecha& f);  // hay cierre en esa fecha o posterior
    Centavos saldoInicial(const Fecha& f);
    std::vector<MovimientoCaja> movimientos(const Fecha& f);
    ResumenCaja resumenCaja(const Fecha& f);
    std::optional<CierreCaja> cierre(const Fecha& f);
    std::vector<CierreCaja> cierres(const Fecha& desde, const Fecha& hasta);
    void cerrarCaja(const Fecha& f, Centavos efectivoContado, const std::string& notas);
    void reabrirCaja(const Fecha& f);
    long registrarMovimientoManual(const Fecha& f, TipoMovimiento tipo, Centavos monto, const std::string& concepto);
    void eliminarMovimientoManual(long id);
    // Primer día anterior a "hoy" con movimientos y sin cierre.
    std::optional<Fecha> diaSinCerrar(const Fecha& hoy);

    // ------------------------------------------------ Recordatorio diario
    Recordatorio recordatorio(const Fecha& hoy);

private:
    long ultimoId();
    std::optional<Fecha> ultimoCierre(const std::optional<Fecha>& antesDe);
    void exigirCajaAbierta(const Fecha& f, const std::string& accion);
    long insertarMovimiento(const Fecha& f, TipoMovimiento t, const std::string& origen, const std::string& concepto,
                            const std::string& referencia, Centavos monto);
    long insertarDeuda(const Deuda& d);
    int abonosActivos(long deudaId);
    std::optional<Deuda> deudaDeComprobante(const char* columna, long id);
    std::pair<Centavos, Centavos> sumarMovimientos(const std::string& condicion, const std::vector<Fecha>& fechas);

    int administradoresActivos(long excepto);

    db::Conexion& c_;
    db::Dialecto d_;
    std::string usuario_;
    uint32_t iteraciones_ = ITERACIONES_HASH;
};

}  // namespace bu
