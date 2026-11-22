# Beneficio Unimax: contabilidad de compra de café

Programa de escritorio para Windows (C++) que lleva la contabilidad de un beneficio de café:
compra de café **uva, pergamino húmedo y pergamino seco**, comprobantes de compra, precio del día,
deudas con clientes y productores, caja diaria y ventas. Usa una base de datos **Microsoft Access**
(`.accdb`) y genera los comprobantes y reportes con **plantillas de Excel**.

---

## 1. Qué hace el programa (una pestaña por tarea)

| Pestaña | Para qué sirve |
|---|---|
| **Comprobante de compra** | Se escribe el DNI del productor (si ya existe, aparece su nombre; si no, se registra al guardar). Se agregan líneas de café pesado: tipo, peso en **libras o quintales**, **galones** (como dato) y precio por libra. El programa calcula los subtotales, el total, lo **pagado en efectivo** y el **saldo** que queda debiendo el beneficio, con una fecha de pago opcional. Al guardar, se imprime el comprobante en Excel. |
| **Historial de compras** | Lista de comprobantes por fecha y productor, con totales. Permite reimprimir o **anular** un comprobante (devuelve el pago a la caja y anula el saldo pendiente). |
| **Clientes** | Productores y compradores: nombre, DNI/RTN, teléfono, dirección. Muestra cuánto les debe el beneficio y cuánto le deben. Genera el **estado de cuenta** en Excel. |
| **Precio del día** | Precio por libra de cada tipo de café para cada fecha. Se puede escribir por libra o por quintal (el otro se calcula solo). Se pone solo en los comprobantes del día y se puede cambiar en un comprobante en particular. |
| **Deudas** | Lo que **el beneficio debe** (saldos de comprobantes, dinero recibido) y lo que **le deben al beneficio** (préstamos, adelantos, saldos de ventas). Registra abonos, muestra vencidas en rojo y permite anular. |
| **Caja** | **Restante del día anterior** + entradas − salidas = **debe haber en caja**. Se escribe el **dinero contado** y se **cierra la caja**; lo contado pasa a ser el saldo inicial del día siguiente. Todas las compras, abonos, préstamos y ventas se anotan solos en la caja. |
| **Ventas** | Venta de café a exportadoras u otros compradores (con cobro total o parcial). |
| **Reportes** | Compras del período, ventas del período, caja por día, cierre de un día, deudas pendientes y estado de cuenta de un cliente (todo en Excel). |
| **Configuración** | Datos de la empresa para los comprobantes, saldo inicial de caja, numeración, recordatorio diario, respaldo de la base de datos. |

**Recordatorio diario de deudas** de dos formas:
1. **Al abrir el programa** (una vez al día) aparece una ventana con las deudas vencidas, las que vencen hoy y las que vencen pronto. También avisa si falta el precio del día o si quedó una caja sin cerrar.
2. **Aviso de Windows** a la hora que usted elija, aunque el programa esté cerrado. Se activa en *Configuración → Activar aviso de Windows* y usa el Programador de tareas de Windows.

---

## 2. Reglas del negocio

Estas reglas salen de sus respuestas. Si alguna no es correcta, se puede cambiar.

- **País:** Honduras. Moneda: Lempiras (L).
- **DNI:** 13 dígitos (`0801-1990-12345`). El programa revisa el formato: departamento 01–18 y año válido. También acepta **RTN de 14 dígitos** para empresas como las exportadoras.
- **Peso:** el pago **siempre se calcula por libra**. Si se escribe en quintales, se convierte (**1 quintal = 100 libras**). Los **galones** se guardan solo como dato de referencia, porque primero se pesa y luego se calcula el precio.
- **Sin descuentos** de peso (ni tara ni humedad): se paga el peso de la báscula.
- **Precio del día:** uno por tipo de café; se puede cambiar en un comprobante en particular.
- **Comprobante interno**, con número correlativo propio. No es una factura fiscal del SAR.
- **Deudas:** el beneficio le debe al productor (saldo que no se pagó en la compra) y el productor le debe al beneficio (préstamo o adelanto). Las deudas **no se descuentan solas** de una compra: si un productor paga con café, se registra la compra y luego un abono a su deuda.
- **Caja:** registra compras, ventas, abonos y préstamos. También permite **entradas o salidas manuales** para casos como el fondo inicial, un retiro del dueño o un gasto.
- Un día con **caja cerrada** queda bloqueado. Para corregirlo, se reabre (solo el último cierre). Las anulaciones se registran en la caja **del día en que se anula**.

---

## 3. Requisitos

- Windows 10 u 11 de **64 bits**.
- Microsoft Office de **64 bits** con **Excel** y **Access**. El programa usa el controlador de Access que viene con Office.
- Para compilar: **Visual Studio 2022** (o más reciente) con la carga de trabajo **"Desarrollo para el escritorio con C++"**. Ya incluye CMake; no hace falta instalar nada más.

---

## 4. Compilar en Visual Studio

1. Descargue el proyecto: en GitHub, botón **Code → Download ZIP**, y descomprímalo. También puede clonarlo con Git.
2. Abra Visual Studio y elija **Archivo → Abrir → Carpeta...** y seleccione la carpeta del proyecto (la que tiene `CMakeLists.txt`).
3. Espere a que termine "Generación de CMake" (abajo, en la ventana *Salida*).
4. En la lista de configuraciones de la barra superior elija **x64 Release (64 bits, para usar)**.
5. Menú **Compilar → Compilar todo**.
6. En el selector de elemento de inicio (botón ▶) elija **BeneficioUnimax.exe** y presione **Ctrl+F5**.

El programa queda en `out\build\x64-release\BeneficioUnimax.exe`, junto con la carpeta `plantillas`.

**Por línea de comandos** (desde *Developer PowerShell for VS*):

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
# Resultado: build\Release\BeneficioUnimax.exe
```

**Sin compilar (descarga directa):** cada vez que se suben cambios a GitHub, el programa se compila
con Visual Studio y se publica en *Releases*. Este enlace siempre descarga la última versión:

<https://github.com/konlink69-source/Beneficio-Unimax/releases/latest/download/BeneficioUnimax-windows-x64.zip>

Descomprima el .zip y abra `BeneficioUnimax.exe`. Si Windows muestra *"Windows protegió su PC"*, haga clic en
*Más información → Ejecutar de todas formas*: el programa no tiene firma digital comercial.
Mientras el repositorio sea privado, el enlace pide iniciar sesión en GitHub con una cuenta que tenga acceso.

### Instalarlo para el uso diario

Copie `BeneficioUnimax.exe` **y la carpeta `plantillas`** a una carpeta fija, por ejemplo
`C:\BeneficioUnimax\`. Luego haga clic derecho en el `.exe` → *Enviar a → Escritorio (crear acceso directo)*.

---

## 5. Dónde quedan sus datos

La primera vez que se abre, el programa crea esta carpeta:

```
Documentos\BeneficioUnimax\
├── BeneficioUnimax.accdb      ← base de datos de Access (todos los registros)
├── Plantillas\                ← plantillas de Excel (puede personalizarlas)
├── Documentos\
│   ├── Comprobantes\          ← comprobantes de compra generados
│   ├── Caja\                  ← cierres de caja
│   ├── Estados de cuenta\
│   └── Reportes\
└── Respaldos\                 ← copias de seguridad
```

- La base de datos se puede **abrir con Access** para consultas propias, pero no cambie la estructura de las tablas.
- **Respaldo:** *Archivo → Respaldar base de datos...* Hágalo al menos una vez por semana y guarde una copia fuera de la computadora (USB o nube).
- Para usar otra carpeta, por ejemplo una carpeta compartida, abra el programa con: `BeneficioUnimax.exe /datos "D:\Contabilidad"`.

---

## 6. Rutina diaria recomendada

1. Abrir el programa y revisar el **recordatorio** del día.
2. **Precio del día:** escribir el precio de cada tipo de café y guardar.
3. Por cada productor: **Comprobante de compra** → DNI → líneas de café → monto pagado → **Guardar e imprimir**.
4. Préstamos, adelantos y abonos en **Deudas**.
5. Ventas en **Ventas**.
6. Al final del día: contar el dinero → pestaña **Caja** → escribir el **dinero contado** → **Cerrar caja del día** → imprimir.

El primer día de uso, escriba en *Configuración* el **saldo inicial de la caja** (el dinero que hay en caja
ese día). Si ya tiene recibos en papel, escriba también el número que sigue al último.

---

## 7. Plantillas de Excel

Las plantillas están en `Documentos\BeneficioUnimax\Plantillas`. Puede abrirlas en Excel y cambiar colores,
letras, columnas, agregar el logotipo, etc. Para que el programa siga sabiendo dónde escribir:

- **No borre los nombres de celda.** Están en *Fórmulas → Administrador de nombres*, por ejemplo `CLIENTE`, `DNI`, `TOTAL` o `DETALLE`. Si mueve una celda, el nombre se mueve con ella.
- Las tablas (`DETALLE`, `MOVIMIENTOS`, `DEUDAS`, ...) empiezan en la celda con ese nombre y **crecen hacia abajo**. Deje libre el espacio debajo de ellas.
- Si una plantilla se daña, bórrela de esa carpeta. Al abrir el programa se copia de nuevo la original desde la carpeta `plantillas` junto al `.exe`.

| Plantilla | Documento |
|---|---|
| `Factura.xlsx` | Comprobante de compra (con el total en letras y espacio para firmas) |
| `CierreCaja.xlsx` | Cierre de caja diario |
| `EstadoCuenta.xlsx` | Estado de cuenta de un cliente (deudas y abonos) |
| `ReporteDeudas.xlsx` | Deudas pendientes |
| `ReporteCompras.xlsx` | Compras del período, con resumen por tipo de café |
| `ReporteVentas.xlsx` | Ventas del período |
| `ReporteCaja.xlsx` | Resumen de caja por día |

Las plantillas originales se generan con `python tools/generar_plantillas.py` (requiere `openpyxl`).

---

## 8. Problemas frecuentes

**"No se pudo crear/abrir la base de datos" o un mensaje sobre el controlador (driver).**
El programa usa el controlador ODBC *Microsoft Access Driver (\*.mdb, \*.accdb)* de 64 bits. Para revisarlo:
abra *Orígenes de datos ODBC (64 bits)* desde el menú Inicio → pestaña *Controladores*. Si no aparece,
instale **Microsoft Access Database Engine 2016 Redistributable** (`accessdatabaseengine_X64.exe`) desde el
sitio de Microsoft. Si el instalador se queja porque ya tiene Office, ejecútelo así:
`accessdatabaseengine_X64.exe /quiet`.

**Excel no se abre al imprimir.** Revise que Excel esté instalado y que no tenga un cuadro de diálogo abierto
(por ejemplo, activación o recuperación de archivos). El archivo queda guardado de todos modos en
`Documentos\BeneficioUnimax\Documentos`.

**"La caja del ... ya está cerrada".** Vaya a *Caja*, elija esa fecha y presione *Reabrir caja* (solo se puede
reabrir el último cierre). Luego registre lo que faltaba y vuelva a cerrar.

**No aparece el aviso de Windows.** Use *Configuración → Probar aviso ahora*. Revise que las notificaciones de
Windows estén activas y que el modo *No molestar* (Asistente de concentración) esté apagado. La computadora debe
estar encendida y con la sesión iniciada a la hora del aviso.

---

## 9. Para programadores

```
src/core/   Lógica del negocio sin Windows: fechas, dinero (en centavos), DNI, montos en letras,
            modelos y contenido de los documentos de Excel.
src/db/     ODBC (Access en Windows, SQLite en las pruebas), creación de tablas y Repositorio
            (todas las operaciones del negocio, con transacciones).
src/win/    Interfaz Win32: ventana con pestañas, diálogos (app.rc), Excel por COM, avisos.
tests/      Pruebas automáticas (189 comprobaciones).
plantillas/ Plantillas de Excel.
tools/      Generadores de plantillas e icono, verificación de plantillas.
```

**Tablas de Access:** `Clientes`, `PreciosDia`, `Facturas` + `FacturaDetalle` (comprobantes de compra),
`Ventas`, `Deudas` + `Abonos`, `MovimientosCaja`, `CierresCaja`, `TiposCafe`, `Configuracion`.
El programa crea las tablas solo. Los montos se guardan como `CURRENCY` y los pesos en libras como `DOUBLE`.

**Pruebas:**
- En Windows, `pruebas.exe` crea una base de datos de Access temporal y prueba todas las operaciones. Sirve también para comprobar que el controlador de Access funciona.
- En Linux, las mismas pruebas usan SQLite: `sudo apt install unixodbc-dev libsqliteodbc`, luego `cmake -S . -B build && cmake --build build && ./build/pruebas`.
- `./build/pruebas --nombres | python tools/verificar_plantillas.py` revisa que las plantillas tengan todas las celdas con nombre.
