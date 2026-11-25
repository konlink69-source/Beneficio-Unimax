#pragma once
// Envoltorio sencillo de ODBC. En Windows se conecta a Microsoft Access
// (archivo .accdb); en Linux se usa con SQLite para las pruebas automáticas.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/Dinero.h"
#include "core/Fecha.h"

namespace bu::db {

class ErrorBD : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::u16string aUtf16(const std::string& utf8);
std::string desdeUtf16(const std::u16string& utf16);

class Sentencia;

class Conexion {
public:
    Conexion() = default;
    ~Conexion();
    Conexion(const Conexion&) = delete;
    Conexion& operator=(const Conexion&) = delete;

    void abrir(const std::string& cadenaConexion);
    void cerrar();
    bool abierta() const { return abierta_; }

    void ejecutar(const std::string& sql);
    Sentencia preparar(const std::string& sql);
    bool existeTabla(const std::string& nombre);
    bool existeColumna(const std::string& tabla, const std::string& columna);

    void iniciarTransaccion();
    void confirmar();
    void revertir();

    SQLHDBC handle() const { return dbc_; }

private:
    SQLHENV env_ = SQL_NULL_HENV;
    SQLHDBC dbc_ = SQL_NULL_HDBC;
    bool abierta_ = false;
};

// Transacción con reversión automática si no se confirma.
class Transaccion {
public:
    explicit Transaccion(Conexion& c) : c_(c) { c_.iniciarTransaccion(); }
    ~Transaccion() {
        if (!terminada_) {
            try {
                c_.revertir();
            } catch (...) {
            }
        }
    }
    void confirmar() {
        c_.confirmar();
        terminada_ = true;
    }

private:
    Conexion& c_;
    bool terminada_ = false;
};

class Sentencia {
public:
    Sentencia(Conexion& c, const std::string& sql);
    ~Sentencia();
    Sentencia(Sentencia&& o) noexcept;
    Sentencia(const Sentencia&) = delete;
    Sentencia& operator=(const Sentencia&) = delete;
    Sentencia& operator=(Sentencia&&) = delete;

    // Parámetros (en el orden de los "?" del SQL).
    Sentencia& pEntero(long v);
    Sentencia& pDecimal(double v);
    Sentencia& pDinero(Centavos v);
    Sentencia& pTexto(const std::string& v);  // texto vacío se guarda como NULL
    Sentencia& pFecha(const Fecha& v);
    Sentencia& pFechaOpc(const std::optional<Fecha>& v);
    Sentencia& pNulo();

    Sentencia& ejecutar();
    bool siguiente();  // avanza a la siguiente fila del resultado
    long filasAfectadas() const;

    // Columnas de la fila actual (empiezan en 1).
    int columnas() const { return static_cast<int>(fila_.size()); }
    bool esNulo(int col) const;
    std::string texto(int col) const;
    long entero(int col) const;
    double decimal(int col) const;
    Centavos dinero(int col) const;
    std::optional<Fecha> fecha(int col) const;

private:
    struct Param;
    Sentencia& agregar(std::unique_ptr<Param> p);
    const std::optional<std::u16string>& celda(int col) const;

    Conexion* c_;
    SQLHSTMT h_ = SQL_NULL_HSTMT;
    std::vector<std::unique_ptr<Param>> params_;
    std::vector<std::optional<std::u16string>> fila_;
    int numColumnas_ = 0;
};

}  // namespace bu::db
