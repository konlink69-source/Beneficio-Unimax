#include "db/Esquema.h"

#include <utility>
#include <vector>

#ifdef _WIN32
#include <odbcinst.h>
#endif

namespace bu::db {

namespace {

std::string reemplazar(std::string s, const std::string& de, const std::string& a) {
    size_t pos = 0;
    while ((pos = s.find(de, pos)) != std::string::npos) {
        s.replace(pos, de.size(), a);
        pos += a.size();
    }
    return s;
}

struct Tabla {
    const char* nombre;
    const char* ddl;
    std::vector<const char*> indices;
};

// %PK% se reemplaza por la clave autonumérica según el motor.
const std::vector<Tabla>& tablas() {
    static const std::vector<Tabla> t = {
        {"Configuracion",
         "CREATE TABLE Configuracion (Clave TEXT(50) NOT NULL PRIMARY KEY, Valor TEXT(255))",
         {}},
        {"TiposCafe",
         "CREATE TABLE TiposCafe (Id LONG NOT NULL PRIMARY KEY, Nombre TEXT(50) NOT NULL)",
         {}},
        {"Clientes",
         "CREATE TABLE Clientes (Id %PK%, Nombre TEXT(150) NOT NULL, DNI TEXT(20) NOT NULL, "
         "Telefono TEXT(30), Direccion TEXT(255), Tipo LONG NOT NULL, Activo LONG NOT NULL, "
         "FechaRegistro DATETIME, CONSTRAINT UQ_Clientes_DNI UNIQUE (DNI))",
         {"CREATE INDEX IX_Clientes_Nombre ON Clientes (Nombre)"}},
        {"PreciosDia",
         "CREATE TABLE PreciosDia (Fecha DATETIME NOT NULL, TipoCafeId LONG NOT NULL, "
         "PrecioLibra CURRENCY NOT NULL, CONSTRAINT PK_PreciosDia PRIMARY KEY (Fecha, TipoCafeId), "
         "CONSTRAINT FK_PreciosDia_Tipo FOREIGN KEY (TipoCafeId) REFERENCES TiposCafe (Id))",
         {}},
        {"Facturas",
         "CREATE TABLE Facturas (Id %PK%, Numero LONG NOT NULL, Fecha DATETIME NOT NULL, "
         "HoraRegistro TEXT(8), ClienteId LONG NOT NULL, Total CURRENCY NOT NULL, Pagado CURRENCY NOT NULL, "
         "Saldo CURRENCY NOT NULL, Observaciones TEXT(255), Anulada LONG NOT NULL, FechaAnulacion DATETIME, "
         "MotivoAnulacion TEXT(255), CONSTRAINT UQ_Facturas_Numero UNIQUE (Numero), "
         "CONSTRAINT FK_Facturas_Cliente FOREIGN KEY (ClienteId) REFERENCES Clientes (Id))",
         {"CREATE INDEX IX_Facturas_Fecha ON Facturas (Fecha)"}},
        {"FacturaDetalle",
         "CREATE TABLE FacturaDetalle (Id %PK%, FacturaId LONG NOT NULL, TipoCafeId LONG NOT NULL, "
         "Libras DOUBLE NOT NULL, Galones DOUBLE NOT NULL, PrecioLibra CURRENCY NOT NULL, "
         "Subtotal CURRENCY NOT NULL, "
         "CONSTRAINT FK_Detalle_Factura FOREIGN KEY (FacturaId) REFERENCES Facturas (Id), "
         "CONSTRAINT FK_Detalle_Tipo FOREIGN KEY (TipoCafeId) REFERENCES TiposCafe (Id))",
         {"CREATE INDEX IX_Detalle_Factura ON FacturaDetalle (FacturaId)"}},
        {"Ventas",
         "CREATE TABLE Ventas (Id %PK%, Numero LONG NOT NULL, Fecha DATETIME NOT NULL, "
         "ClienteId LONG NOT NULL, TipoCafeId LONG NOT NULL, Libras DOUBLE NOT NULL, "
         "PrecioLibra CURRENCY NOT NULL, Total CURRENCY NOT NULL, Recibido CURRENCY NOT NULL, "
         "Saldo CURRENCY NOT NULL, Observaciones TEXT(255), Anulada LONG NOT NULL, "
         "FechaAnulacion DATETIME, MotivoAnulacion TEXT(255), "
         "CONSTRAINT UQ_Ventas_Numero UNIQUE (Numero), "
         "CONSTRAINT FK_Ventas_Cliente FOREIGN KEY (ClienteId) REFERENCES Clientes (Id), "
         "CONSTRAINT FK_Ventas_Tipo FOREIGN KEY (TipoCafeId) REFERENCES TiposCafe (Id))",
         {"CREATE INDEX IX_Ventas_Fecha ON Ventas (Fecha)"}},
        {"Deudas",
         "CREATE TABLE Deudas (Id %PK%, ClienteId LONG NOT NULL, Tipo LONG NOT NULL, "
         "Origen TEXT(30) NOT NULL, FacturaId LONG, VentaId LONG, Fecha DATETIME NOT NULL, "
         "Vencimiento DATETIME, Monto CURRENCY NOT NULL, Saldo CURRENCY NOT NULL, Concepto TEXT(255), "
         "AfectoCaja LONG NOT NULL, Anulada LONG NOT NULL, "
         "CONSTRAINT FK_Deudas_Cliente FOREIGN KEY (ClienteId) REFERENCES Clientes (Id))",
         {"CREATE INDEX IX_Deudas_Cliente ON Deudas (ClienteId)",
          "CREATE INDEX IX_Deudas_Factura ON Deudas (FacturaId)",
          "CREATE INDEX IX_Deudas_Venta ON Deudas (VentaId)"}},
        {"Abonos",
         "CREATE TABLE Abonos (Id %PK%, DeudaId LONG NOT NULL, Fecha DATETIME NOT NULL, "
         "HoraRegistro TEXT(8), Monto CURRENCY NOT NULL, Nota TEXT(255), Anulado LONG NOT NULL, "
         "CONSTRAINT FK_Abonos_Deuda FOREIGN KEY (DeudaId) REFERENCES Deudas (Id))",
         {"CREATE INDEX IX_Abonos_Deuda ON Abonos (DeudaId)"}},
        {"MovimientosCaja",
         "CREATE TABLE MovimientosCaja (Id %PK%, Fecha DATETIME NOT NULL, Hora TEXT(8) NOT NULL, "
         "Tipo LONG NOT NULL, Origen TEXT(20) NOT NULL, Concepto TEXT(255) NOT NULL, "
         "Referencia TEXT(50), Monto CURRENCY NOT NULL)",
         {"CREATE INDEX IX_Movimientos_Fecha ON MovimientosCaja (Fecha)"}},
        {"Usuarios",
         "CREATE TABLE Usuarios (Id %PK%, Usuario TEXT(50) NOT NULL, Nombre TEXT(150), Hash TEXT(200) NOT NULL, "
         "Rol LONG NOT NULL, Activo LONG NOT NULL, DebeCambiar LONG NOT NULL, FechaRegistro DATETIME, "
         "UltimoIngreso DATETIME, CONSTRAINT UQ_Usuarios_Usuario UNIQUE (Usuario))",
         {}},
        {"CierresCaja",
         "CREATE TABLE CierresCaja (Fecha DATETIME NOT NULL PRIMARY KEY, Hora TEXT(8), "
         "SaldoInicial CURRENCY NOT NULL, Ingresos CURRENCY NOT NULL, Egresos CURRENCY NOT NULL, "
         "SaldoEsperado CURRENCY NOT NULL, EfectivoContado CURRENCY NOT NULL, "
         "Diferencia CURRENCY NOT NULL, Notas TEXT(255))",
         {}},
    };
    return t;
}

void insertarConfigSiFalta(Conexion& c, const std::string& clave, const std::string& valor) {
    auto s = c.preparar("SELECT COUNT(*) FROM Configuracion WHERE Clave = ?");
    s.pTexto(clave).ejecutar();
    if (s.siguiente() && s.entero(1) > 0) return;
    c.preparar("INSERT INTO Configuracion (Clave, Valor) VALUES (?, ?)").pTexto(clave).pTexto(valor).ejecutar();
}

}  // namespace

void crearEsquema(Conexion& c, Dialecto d) {
    const std::string pk = d == Dialecto::Access ? "COUNTER PRIMARY KEY" : "INTEGER PRIMARY KEY AUTOINCREMENT";
    for (const auto& t : tablas()) {
        if (c.existeTabla(t.nombre)) continue;
        c.ejecutar(reemplazar(t.ddl, "%PK%", pk));
        for (const char* idx : t.indices) c.ejecutar(idx);
    }

    // Versión 2: columna "Usuario" (quién registró cada operación). Se agrega si falta,
    // así funciona igual con bases de datos nuevas y con las creadas por versiones anteriores.
    for (const char* tabla : {"Facturas", "Ventas", "Deudas", "Abonos", "MovimientosCaja", "CierresCaja"})
        if (!c.existeColumna(tabla, "Usuario"))
            c.ejecutar(std::string("ALTER TABLE ") + tabla + " ADD COLUMN Usuario TEXT(50)");

    // Tipos de café fijos.
    const std::pair<int, const char*> tipos[] = {{1, "Uva"}, {2, "Pergamino húmedo"}, {3, "Pergamino seco"}};
    for (const auto& tp : tipos) {
        auto s = c.preparar("SELECT COUNT(*) FROM TiposCafe WHERE Id = ?");
        s.pEntero(tp.first).ejecutar();
        if (s.siguiente() && s.entero(1) == 0)
            c.preparar("INSERT INTO TiposCafe (Id, Nombre) VALUES (?, ?)").pEntero(tp.first).pTexto(tp.second).ejecutar();
    }

    // Configuración inicial.
    insertarConfigSiFalta(c, "VersionEsquema", std::to_string(VERSION_ESQUEMA));
    c.preparar("UPDATE Configuracion SET Valor = ? WHERE Clave = ?")
        .pTexto(std::to_string(VERSION_ESQUEMA))
        .pTexto("VersionEsquema")
        .ejecutar();
    insertarConfigSiFalta(c, "EmpresaNombre", "Beneficio Unimax");
    insertarConfigSiFalta(c, "EmpresaRTN", "");
    insertarConfigSiFalta(c, "EmpresaDireccion", "");
    insertarConfigSiFalta(c, "EmpresaTelefono", "");
    insertarConfigSiFalta(c, "SaldoAperturaCaja", "0.00");
    insertarConfigSiFalta(c, "NumeroInicialFactura", "1");
    insertarConfigSiFalta(c, "NumeroInicialVenta", "1");
    insertarConfigSiFalta(c, "DiasAnticipacion", "3");
    insertarConfigSiFalta(c, "RecordatorioAlAbrir", "1");
    insertarConfigSiFalta(c, "HoraAviso", "07:30");
    insertarConfigSiFalta(c, "GuardarPdf", "0");
    insertarConfigSiFalta(c, "UltimoRecordatorio", "");
    insertarConfigSiFalta(c, "MarcaAgua", "1");
    insertarConfigSiFalta(c, "Impresora", "");
    insertarConfigSiFalta(c, "CopiasComprobante", "1");
    insertarConfigSiFalta(c, "GuardarPdfComprobante", "1");
    insertarConfigSiFalta(c, "CodigoRecuperacion", "");
}

std::string cadenaConexionAccess(const std::string& rutaArchivo) {
    return "Driver={Microsoft Access Driver (*.mdb, *.accdb)};DBQ=" + rutaArchivo + ";";
}

#ifdef _WIN32
std::string crearArchivoAccess(const std::wstring& ruta) {
    // Lista de atributos terminada en doble cero.
    std::wstring atributos = L"CREATE_DBV12=\"" + ruta + L"\" General";
    atributos.push_back(L'\0');
    atributos.push_back(L'\0');
    if (SQLConfigDataSourceW(nullptr, ODBC_ADD_DSN, L"Microsoft Access Driver (*.mdb, *.accdb)", atributos.c_str()))
        return std::string();
    std::string detalle;
    for (WORD i = 1; i <= 8; ++i) {
        DWORD codigo = 0;
        wchar_t msg[512] = {0};
        WORD largo = 0;
        if (SQLInstallerErrorW(i, &codigo, msg, 511, &largo) != SQL_SUCCESS) break;
        if (!detalle.empty()) detalle += " ";
        detalle += desdeUtf16(std::u16string(reinterpret_cast<const char16_t*>(msg)));
    }
    return detalle.empty() ? std::string("No se encontró el controlador ODBC de Microsoft Access.") : detalle;
}
#endif

}  // namespace bu::db
