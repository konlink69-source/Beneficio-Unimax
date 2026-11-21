#include "db/Odbc.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace bu::db {

// ---------------------------------------------------------------- UTF-8 <-> UTF-16
std::u16string aUtf16(const std::string& s) {
    std::u16string r;
    r.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp = 0;
        int extra = 0;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            extra = 3;
        } else {
            cp = 0xFFFD;
        }
        ++i;
        for (int k = 0; k < extra; ++k) {
            if (i >= s.size() || (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
                cp = 0xFFFD;
                break;
            }
            cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
            ++i;
        }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            r += static_cast<char16_t>(0xD800 + (cp >> 10));
            r += static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
        } else {
            r += static_cast<char16_t>(cp);
        }
    }
    return r;
}

std::string desdeUtf16(const std::u16string& s) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char32_t cp = s[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size()) {
            const char32_t lo = s[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        if (cp < 0x80) {
            r += static_cast<char>(cp);
        } else if (cp < 0x800) {
            r += static_cast<char>(0xC0 | (cp >> 6));
            r += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            r += static_cast<char>(0xE0 | (cp >> 12));
            r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            r += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            r += static_cast<char>(0xF0 | (cp >> 18));
            r += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            r += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return r;
}

namespace {

SQLWCHAR* W(std::u16string& s) { return reinterpret_cast<SQLWCHAR*>(&s[0]); }

std::string diagnostico(SQLSMALLINT tipo, SQLHANDLE h) {
    std::string r;
    for (SQLSMALLINT i = 1; i <= 5; ++i) {
        char16_t estado[6] = {0};
        char16_t msg[1024] = {0};
        SQLINTEGER nativo = 0;
        SQLSMALLINT largo = 0;
        const SQLRETURN rc = SQLGetDiagRecW(tipo, h, i, reinterpret_cast<SQLWCHAR*>(estado), &nativo,
                                            reinterpret_cast<SQLWCHAR*>(msg), 1023, &largo);
        if (!SQL_SUCCEEDED(rc)) break;
        if (!r.empty()) r += "\n";
        r += "[" + desdeUtf16(std::u16string(estado)) + "] " + desdeUtf16(std::u16string(msg));
    }
    return r.empty() ? std::string("Error ODBC desconocido") : r;
}

void verificar(SQLRETURN rc, SQLSMALLINT tipo, SQLHANDLE h, const std::string& contexto) {
    if (SQL_SUCCEEDED(rc) || rc == SQL_NO_DATA) return;
    throw ErrorBD(contexto + ": " + diagnostico(tipo, h));
}

}  // namespace

// ---------------------------------------------------------------- Conexion
Conexion::~Conexion() { cerrar(); }

void Conexion::abrir(const std::string& cadenaConexion) {
    cerrar();
    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_);
    if (!SQL_SUCCEEDED(rc)) throw ErrorBD("No se pudo iniciar ODBC.");
    SQLSetEnvAttr(env_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(static_cast<std::uintptr_t>(SQL_OV_ODBC3)), 0);
    rc = SQLAllocHandle(SQL_HANDLE_DBC, env_, &dbc_);
    verificar(rc, SQL_HANDLE_ENV, env_, "Conexión");
    std::u16string cad = aUtf16(cadenaConexion);
    char16_t salida[1024];
    SQLSMALLINT largoSalida = 0;
    rc = SQLDriverConnectW(dbc_, nullptr, W(cad), static_cast<SQLSMALLINT>(cad.size()),
                           reinterpret_cast<SQLWCHAR*>(salida), 1023, &largoSalida, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(rc)) {
        const std::string msg = diagnostico(SQL_HANDLE_DBC, dbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
        SQLFreeHandle(SQL_HANDLE_ENV, env_);
        dbc_ = SQL_NULL_HDBC;
        env_ = SQL_NULL_HENV;
        throw ErrorBD("No se pudo abrir la base de datos: " + msg);
    }
    abierta_ = true;
}

void Conexion::cerrar() {
    if (dbc_ != SQL_NULL_HDBC) {
        if (abierta_) SQLDisconnect(dbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
        dbc_ = SQL_NULL_HDBC;
    }
    if (env_ != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, env_);
        env_ = SQL_NULL_HENV;
    }
    abierta_ = false;
}

void Conexion::ejecutar(const std::string& sql) { preparar(sql).ejecutar(); }

Sentencia Conexion::preparar(const std::string& sql) { return Sentencia(*this, sql); }

bool Conexion::existeTabla(const std::string& nombre) {
    SQLHSTMT h = SQL_NULL_HSTMT;
    verificar(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &h), SQL_HANDLE_DBC, dbc_, "Consulta de tablas");
    std::u16string n = aUtf16(nombre);
    std::u16string tipo = aUtf16("TABLE");
    SQLRETURN rc = SQLTablesW(h, nullptr, 0, nullptr, 0, W(n), static_cast<SQLSMALLINT>(n.size()), W(tipo),
                              static_cast<SQLSMALLINT>(tipo.size()));
    bool existe = false;
    if (SQL_SUCCEEDED(rc)) existe = SQL_SUCCEEDED(SQLFetch(h));
    SQLFreeHandle(SQL_HANDLE_STMT, h);
    return existe;
}

void Conexion::iniciarTransaccion() {
    verificar(SQLSetConnectAttr(dbc_, SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(static_cast<std::uintptr_t>(SQL_AUTOCOMMIT_OFF)), 0),
              SQL_HANDLE_DBC, dbc_, "Iniciar transacción");
}

void Conexion::confirmar() {
    const SQLRETURN rc = SQLEndTran(SQL_HANDLE_DBC, dbc_, SQL_COMMIT);
    SQLSetConnectAttr(dbc_, SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(static_cast<std::uintptr_t>(SQL_AUTOCOMMIT_ON)), 0);
    verificar(rc, SQL_HANDLE_DBC, dbc_, "Confirmar transacción");
}

void Conexion::revertir() {
    SQLEndTran(SQL_HANDLE_DBC, dbc_, SQL_ROLLBACK);
    SQLSetConnectAttr(dbc_, SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(static_cast<std::uintptr_t>(SQL_AUTOCOMMIT_ON)), 0);
}

// ---------------------------------------------------------------- Sentencia
struct Sentencia::Param {
    SQLSMALLINT tipoC = SQL_C_SLONG;
    SQLSMALLINT tipoSql = SQL_INTEGER;
    SQLULEN tamano = 0;
    SQLSMALLINT decimales = 0;
    std::u16string texto;
    SQLINTEGER entero = 0;
    double decimal = 0;
    SQL_TIMESTAMP_STRUCT fecha{};
    SQLLEN indicador = 0;
};

Sentencia::Sentencia(Conexion& c, const std::string& sql) : c_(&c) {
    verificar(SQLAllocHandle(SQL_HANDLE_STMT, c.handle(), &h_), SQL_HANDLE_DBC, c.handle(), "Preparar consulta");
    std::u16string s = aUtf16(sql);
    const SQLRETURN rc = SQLPrepareW(h_, W(s), static_cast<SQLINTEGER>(s.size()));
    if (!SQL_SUCCEEDED(rc)) {
        const std::string msg = diagnostico(SQL_HANDLE_STMT, h_);
        SQLFreeHandle(SQL_HANDLE_STMT, h_);
        h_ = SQL_NULL_HSTMT;
        throw ErrorBD("Error en la consulta: " + msg + "\nSQL: " + sql);
    }
}

Sentencia::~Sentencia() {
    if (h_ != SQL_NULL_HSTMT) SQLFreeHandle(SQL_HANDLE_STMT, h_);
}

Sentencia::Sentencia(Sentencia&& o) noexcept
    : c_(o.c_), h_(o.h_), params_(std::move(o.params_)), fila_(std::move(o.fila_)), numColumnas_(o.numColumnas_) {
    o.h_ = SQL_NULL_HSTMT;
}

Sentencia& Sentencia::agregar(std::unique_ptr<Param> p) {
    const SQLUSMALLINT indice = static_cast<SQLUSMALLINT>(params_.size() + 1);
    SQLPOINTER valor = nullptr;
    SQLLEN largoBuffer = 0;
    switch (p->tipoC) {
        case SQL_C_WCHAR:
            valor = &p->texto[0];
            largoBuffer = static_cast<SQLLEN>((p->texto.size() + 1) * sizeof(char16_t));
            break;
        case SQL_C_DOUBLE:
            valor = &p->decimal;
            break;
        case SQL_C_TYPE_TIMESTAMP:
            valor = &p->fecha;
            largoBuffer = sizeof(p->fecha);
            break;
        default:
            valor = &p->entero;
            break;
    }
    const SQLRETURN rc = SQLBindParameter(h_, indice, SQL_PARAM_INPUT, p->tipoC, p->tipoSql, p->tamano,
                                          p->decimales, valor, largoBuffer, &p->indicador);
    verificar(rc, SQL_HANDLE_STMT, h_, "Parámetro " + std::to_string(indice));
    params_.push_back(std::move(p));
    return *this;
}

Sentencia& Sentencia::pEntero(long v) {
    auto p = std::make_unique<Param>();
    p->tipoC = SQL_C_SLONG;
    p->tipoSql = SQL_INTEGER;
    p->entero = static_cast<SQLINTEGER>(v);
    return agregar(std::move(p));
}

Sentencia& Sentencia::pDecimal(double v) {
    auto p = std::make_unique<Param>();
    p->tipoC = SQL_C_DOUBLE;
    p->tipoSql = SQL_DOUBLE;
    p->decimal = v;
    p->tamano = 15;
    return agregar(std::move(p));
}

Sentencia& Sentencia::pDinero(Centavos v) { return pDecimal(aLempiras(v)); }

Sentencia& Sentencia::pTexto(const std::string& v) {
    auto p = std::make_unique<Param>();
    p->tipoC = SQL_C_WCHAR;
    p->tipoSql = SQL_WVARCHAR;
    p->texto = aUtf16(v);
    if (p->texto.size() > 255) p->texto.resize(255);
    p->tamano = p->texto.empty() ? 1 : p->texto.size();
    p->indicador = p->texto.empty() ? SQL_NULL_DATA : SQL_NTS;
    return agregar(std::move(p));
}

Sentencia& Sentencia::pFecha(const Fecha& v) {
    auto p = std::make_unique<Param>();
    p->tipoC = SQL_C_TYPE_TIMESTAMP;
    p->tipoSql = SQL_TYPE_TIMESTAMP;
    p->tamano = 19;
    p->fecha.year = static_cast<SQLSMALLINT>(v.anio);
    p->fecha.month = static_cast<SQLUSMALLINT>(v.mes);
    p->fecha.day = static_cast<SQLUSMALLINT>(v.dia);
    return agregar(std::move(p));
}

Sentencia& Sentencia::pFechaOpc(const std::optional<Fecha>& v) {
    if (v) return pFecha(*v);
    auto p = std::make_unique<Param>();
    p->tipoC = SQL_C_TYPE_TIMESTAMP;
    p->tipoSql = SQL_TYPE_TIMESTAMP;
    p->tamano = 19;
    p->indicador = SQL_NULL_DATA;
    return agregar(std::move(p));
}

Sentencia& Sentencia::pNulo() {
    auto p = std::make_unique<Param>();
    p->tipoC = SQL_C_SLONG;
    p->tipoSql = SQL_INTEGER;
    p->indicador = SQL_NULL_DATA;
    return agregar(std::move(p));
}

Sentencia& Sentencia::ejecutar() {
    const SQLRETURN rc = SQLExecute(h_);
    verificar(rc, SQL_HANDLE_STMT, h_, "Error al ejecutar la consulta");
    SQLSMALLINT n = 0;
    SQLNumResultCols(h_, &n);
    numColumnas_ = n;
    return *this;
}

bool Sentencia::siguiente() {
    fila_.clear();
    if (numColumnas_ <= 0) return false;
    SQLRETURN rc = SQLFetch(h_);
    if (rc == SQL_NO_DATA) return false;
    verificar(rc, SQL_HANDLE_STMT, h_, "Error al leer resultados");
    // Se leen todas las columnas como texto, en orden (requisito del controlador de Access).
    fila_.resize(static_cast<size_t>(numColumnas_));
    for (int col = 1; col <= numColumnas_; ++col) {
        std::u16string acumulado;
        bool nulo = false;
        char16_t buffer[256];
        for (;;) {
            SQLLEN ind = 0;
            rc = SQLGetData(h_, static_cast<SQLUSMALLINT>(col), SQL_C_WCHAR, buffer, sizeof(buffer), &ind);
            if (rc == SQL_NO_DATA) break;
            verificar(rc, SQL_HANDLE_STMT, h_, "Error al leer columna " + std::to_string(col));
            if (ind == SQL_NULL_DATA) {
                nulo = true;
                break;
            }
            size_t caracteres = 0;
            const size_t capacidad = sizeof(buffer) / sizeof(char16_t) - 1;
            if (ind == SQL_NO_TOTAL || ind > static_cast<SQLLEN>(capacidad * sizeof(char16_t)))
                caracteres = capacidad;
            else
                caracteres = static_cast<size_t>(ind) / sizeof(char16_t);
            acumulado.append(buffer, caracteres);
            if (rc == SQL_SUCCESS) break;
        }
        if (!nulo) fila_[static_cast<size_t>(col - 1)] = std::move(acumulado);
    }
    return true;
}

long Sentencia::filasAfectadas() const {
    SQLLEN n = 0;
    SQLRowCount(h_, &n);
    return static_cast<long>(n);
}

const std::optional<std::u16string>& Sentencia::celda(int col) const {
    if (col < 1 || col > static_cast<int>(fila_.size())) throw ErrorBD("Columna fuera de rango: " + std::to_string(col));
    return fila_[static_cast<size_t>(col - 1)];
}

bool Sentencia::esNulo(int col) const { return !celda(col).has_value(); }

std::string Sentencia::texto(int col) const {
    const auto& c = celda(col);
    return c ? desdeUtf16(*c) : std::string();
}

long Sentencia::entero(int col) const {
    const std::string t = texto(col);
    if (t.empty()) return 0;
    return static_cast<long>(std::llround(std::strtod(t.c_str(), nullptr)));
}

double Sentencia::decimal(int col) const {
    const std::string t = texto(col);
    if (t.empty()) return 0.0;
    return std::strtod(t.c_str(), nullptr);
}

Centavos Sentencia::dinero(int col) const { return desdeLempiras(decimal(col)); }

std::optional<Fecha> Sentencia::fecha(int col) const {
    if (esNulo(col)) return std::nullopt;
    return Fecha::desdeIso(texto(col));
}

}  // namespace bu::db
