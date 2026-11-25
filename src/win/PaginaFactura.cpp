// Pestaña "Comprobante de compra": registra la compra de café a un productor.
#include <cstdio>

#include "core/Documentos.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Excel.h"
#include "win/Impresion.h"
#include "win/Paginas.h"
#include "win/resource.h"

namespace bu::win {

namespace {


class PaginaFactura : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Comprobante de compra"; }

    void alIniciar() override {
        cliente_.configurar(h_, IDC_F_DNI, IDC_F_NOMBRE, IDC_F_TELEFONO, IDC_F_INFO, CLIENTE_PRODUCTOR);
        configurarFecha(h_, IDC_F_FECHA);
        configurarFecha(h_, IDC_F_VENCE);
        for (int t = 1; t <= NUM_TIPOS_CAFE; ++t) comboAgregar(h_, IDC_F_TIPO, nombreTipoCafe(t), t);
        comboSeleccionar(h_, IDC_F_TIPO, CAFE_SECO);
        comboAgregar(h_, IDC_F_UNIDAD, "Libras", static_cast<LPARAM>(Unidad::Libras));
        comboAgregar(h_, IDC_F_UNIDAD, "Quintales", static_cast<LPARAM>(Unidad::Quintales));
        comboSeleccionar(h_, IDC_F_UNIDAD, static_cast<LPARAM>(Unidad::Libras));
        lvConfigurar(h_, IDC_F_LINEAS,
                     {{L"Tipo de café", 100},
                      {L"Libras", 66, true},
                      {L"Quintales", 56, true},
                      {L"Galones", 52, true},
                      {L"Precio por libra", 70, true},
                      {L"Precio por quintal", 74, true},
                      {L"Subtotal", 84, true}});
        // Etiquetas del cliente grandes y en café; los campos con letra más grande.
        for (int id : {IDC_F_LBL_DNI, IDC_F_LBL_NOMBRE, IDC_F_LBL_TEL}) {
            ponerFuente(h_, id, fuenteEtiqueta());
            colorTexto(id, COLOR_CAFE);
        }
        for (int id : {IDC_F_DNI, IDC_F_NOMBRE, IDC_F_TELEFONO}) ponerFuente(h_, id, fuente(tamanoLetra() * 11.0 / 9.0, false));
        textoGuia(h_, IDC_F_DNI, L"DNI o nombre + Enter");
        textoGuia(h_, IDC_F_NOMBRE, L"Nombre completo (o Enter para buscar)");
        ponerFuente(h_, IDC_F_TOTAL, fuenteGrande());
        ponerFuente(h_, IDC_F_NUMERO, fuenteNegrita());
        ponerFuente(h_, IDC_F_SUBTOTAL, fuenteNegrita());
        ponerFuente(h_, IDC_F_SALDO, fuenteNegrita());
        colorTexto(IDC_F_TOTAL, COLOR_CAFE);
        colorTexto(IDC_F_NUMERO, COLOR_ROJO);
        colorTexto(IDC_F_PESO_INFO, COLOR_GRIS);
        limpiar();
    }

    void alMostrar() override {
        actualizarNumero();
        cargarPrecios(false);
    }

    // El logo del beneficio, tenue, al centro de la tabla de líneas (marca de agua de la pestaña).
    void alRedimensionar(int ancho, int alto) override {
        Pagina::alRedimensionar(ancho, alto);
        HWND lista = GetDlgItem(h_, IDC_F_LINEAS);
        RECT r;
        GetClientRect(lista, &r);
        if (r.right < 50 || r.bottom < 30 || (r.right == tamLogo_.cx && r.bottom == tamLogo_.cy)) return;
        tamLogo_ = {r.right, r.bottom};
        LVBKIMAGEW bk{};
        bk.hbm = App::instancia().logoTenue(r.right * 3 / 4, r.bottom * 9 / 10);
        if (!bk.hbm) return;
        bk.ulFlags = LVBKIF_SOURCE_HBITMAP | LVBKIF_STYLE_NORMAL;
        bk.xOffsetPercent = 50;
        bk.yOffsetPercent = 50;
        SendMessageW(lista, LVM_SETBKIMAGEW, 0, reinterpret_cast<LPARAM>(&bk));
        ListView_SetTextBkColor(lista, CLR_NONE);
    }

    bool alComando(int id, int codigo) override {
        if (cliente_.comando(id, codigo)) return true;
        switch (id) {
            case IDC_F_BUSCAR:
                cliente_.elegir();
                return true;
            case IDC_F_TIPO:
                if (codigo == CBN_SELCHANGE) {
                    ponerPrecioSugerido();
                    actualizarSubtotal();
                    actualizarNumero();
                }
                return true;
            case IDC_F_INFORME:
                verInformeDia(h_, Fecha::hoy());
                return true;
            case IDC_F_UNIDAD:
                if (codigo == CBN_SELCHANGE) actualizarSubtotal();
                return true;
            case IDC_F_CANTIDAD:
            case IDC_F_PRECIO:
                if (codigo == EN_CHANGE) actualizarSubtotal();
                return true;
            case IDC_F_PAGADO:
                if (codigo == EN_CHANGE && !actualizando_) {
                    pagadoEditado_ = true;
                    actualizarTotales();
                }
                return true;
            case IDC_F_AGREGAR:
                agregarLinea(true);
                return true;
            case IDC_F_QUITAR:
                quitarLinea();
                return true;
            case IDC_F_GUARDAR_IMP:
                guardar(true);
                return true;
            case IDC_F_GUARDAR:
                guardar(false);
                return true;
            case IDC_F_LIMPIAR:
                if (lineas_.empty() || confirmar(h_, "¿Borrar los datos del comprobante sin guardarlo?")) limpiar();
                return true;
            default:
                return false;
        }
    }

    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if (nm->idFrom == IDC_F_FECHA && nm->code == DTN_DATETIMECHANGE) {
            cargarPrecios(true);
            r = 0;
            return true;
        }
        return false;
    }

    void alAceptar() override {
        HWND foco = GetFocus();
        if (cliente_.alPresionarEnter()) {
            if (cliente_.hayCliente()) enfocar(h_, IDC_F_CANTIDAD);
        } else if (foco == GetDlgItem(h_, IDC_F_CANTIDAD) || foco == GetDlgItem(h_, IDC_F_GALONES) ||
                   foco == GetDlgItem(h_, IDC_F_PRECIO)) {
            agregarLinea(true);
        } else if (foco == GetDlgItem(h_, IDC_F_NOMBRE)) {
            enfocar(h_, IDC_F_CANTIDAD);
        }
    }

private:
    void limpiar() {
        cliente_.limpiar();
        lineas_.clear();
        ponerFecha(h_, IDC_F_FECHA, Fecha::hoy());
        ponerFechaOpc(h_, IDC_F_VENCE, std::nullopt);
        ponerTexto(h_, IDC_F_OBS, "");
        pagadoEditado_ = false;
        ponerTexto(h_, IDC_F_CANTIDAD, "");
        ponerTexto(h_, IDC_F_GALONES, "");
        refrescarLineas();
        alMostrar();
        enfocar(h_, IDC_F_DNI);
    }

    void cargarPrecios(bool cambiarPrecio) {
        const Fecha f = leerFecha(h_, IDC_F_FECHA);
        precios_ = repo().preciosDelDia(f);
        std::string t;
        if (precios_.vacio()) {
            precios_ = repo().ultimosPrecios(f);
            if (precios_.vacio())
                t = "¡Atención! No hay precios registrados. Ingréselos en la pestaña \"Precio del día\".";
            else
                t = "¡Atención! Sin precio del " + f.texto() + "; se sugieren los del " + precios_.fecha.texto() + ".";
            colorTexto(IDC_F_PRECIOS, COLOR_ROJO);
        } else {
            t = "Precio del día por libra:";
            colorTexto(IDC_F_PRECIOS, COLOR_VERDE);
        }
        if (!precios_.vacio())
            for (int tipo = 1; tipo <= NUM_TIPOS_CAFE; ++tipo)
                if (precios_.tiene(tipo))
                    t += std::string("  ") + nombreTipoCafe(tipo) + " L " + formatoPrecio(precios_.precioLibra[tipo]) + ";";
        ponerTexto(h_, IDC_F_PRECIOS, t);
        if (cambiarPrecio || vacio(h_, IDC_F_PRECIO)) ponerPrecioSugerido();
    }

    int tipoActual() const { return static_cast<int>(comboDato(h_, IDC_F_TIPO, CAFE_SECO)); }
    Unidad unidadActual() const { return static_cast<Unidad>(comboDato(h_, IDC_F_UNIDAD, 0)); }

    void ponerPrecioSugerido() {
        const int t = tipoActual();
        ponerTexto(h_, IDC_F_PRECIO, precios_.tiene(t) ? formatoPrecio(precios_.precioLibra[t]) : std::string());
    }

    void actualizarSubtotal() {
        const auto cantidad = leerDecimal(h_, IDC_F_CANTIDAD);
        const auto precio = leerDecimal(h_, IDC_F_PRECIO);
        std::string sub, info;
        if (cantidad && *cantidad > 0) {
            const double libras = aLibras(*cantidad, unidadActual());
            info = "= " + formatoDecimal(libras, 2) + " lb = " + formatoDecimal(aQuintales(libras), 2) + " qq";
            if (precio && *precio > 0) sub = formatoLempiras(calcularSubtotal(libras, *precio));
        }
        if (precio && *precio > 0) {
            if (!info.empty()) info += "    ·    ";
            info += "Precio por quintal: L " + formatoPrecio(*precio * LIBRAS_POR_QUINTAL);
        }
        ponerTexto(h_, IDC_F_SUBTOTAL, sub);
        ponerTexto(h_, IDC_F_PESO_INFO, info);
    }

    // Agrega la línea escrita. "avisar" = mostrar errores (si es false, ignora una línea vacía).
    bool agregarLinea(bool avisar) {
        const bool hayPeso = !vacio(h_, IDC_F_CANTIDAD);
        if (!hayPeso && !avisar) return true;
        const auto cantidad = leerDecimal(h_, IDC_F_CANTIDAD);
        const auto precio = leerDecimal(h_, IDC_F_PRECIO);
        const auto galones = vacio(h_, IDC_F_GALONES) ? std::optional<double>(0.0) : leerDecimal(h_, IDC_F_GALONES);
        if (!cantidad || *cantidad <= 0) {
            mostrarAviso(h_, "Escriba el peso del café (en libras o quintales).");
            enfocar(h_, IDC_F_CANTIDAD);
            return false;
        }
        if (!precio || *precio <= 0) {
            mostrarAviso(h_, "Escriba el precio por libra.");
            enfocar(h_, IDC_F_PRECIO);
            return false;
        }
        if (!galones || *galones < 0) {
            mostrarAviso(h_, "Los galones deben ser un número (o déjelos vacíos).");
            enfocar(h_, IDC_F_GALONES);
            return false;
        }
        if (static_cast<int>(lineas_.size()) >= MAX_LINEAS_FACTURA) {
            mostrarAviso(h_, "Un comprobante admite como máximo " + std::to_string(MAX_LINEAS_FACTURA) +
                                 " líneas. Guarde este y haga otro.");
            return false;
        }
        if (!lineas_.empty() && categoriaDe(tipoActual()) != categoriaDe(lineas_.front().tipoCafe)) {
            mostrarAviso(h_, std::string("Esta factura es de ") +
                                 (categoriaDe(lineas_.front().tipoCafe) == CATEGORIA_UVA ? "UVA" : "PERGAMINO") +
                                 ". Las facturas de uva y de pergamino van aparte: guarde esta y haga otra para " +
                                 (categoriaDe(tipoActual()) == CATEGORIA_UVA ? "la uva." : "el pergamino."));
            return false;
        }
        LineaFactura l;
        l.tipoCafe = tipoActual();
        l.libras = redondear(aLibras(*cantidad, unidadActual()), 2);
        l.galones = redondear(*galones, 2);
        l.precioLibra = redondear(*precio, 4);
        l.subtotal = calcularSubtotal(l.libras, l.precioLibra);
        lineas_.push_back(l);
        ponerTexto(h_, IDC_F_CANTIDAD, "");
        ponerTexto(h_, IDC_F_GALONES, "");
        refrescarLineas();
        enfocar(h_, IDC_F_CANTIDAD);
        return true;
    }

    void quitarLinea() {
        const int fila = lvSeleccion(h_, IDC_F_LINEAS);
        if (fila < 0 || fila >= static_cast<int>(lineas_.size())) {
            mostrarAviso(h_, "Seleccione en la tabla la línea que desea quitar.");
            return;
        }
        lineas_.erase(lineas_.begin() + fila);
        refrescarLineas();
    }

    void refrescarLineas() {
        actualizarNumero();
        lvLimpiar(h_, IDC_F_LINEAS);
        for (size_t i = 0; i < lineas_.size(); ++i) {
            const auto& l = lineas_[i];
            lvAgregar(h_, IDC_F_LINEAS,
                      {nombreTipoCafe(l.tipoCafe), formatoDecimal(l.libras, 2), formatoDecimal(l.quintales(), 2),
                       l.galones > 0 ? formatoDecimal(l.galones, 2) : std::string(), "L " + formatoPrecio(l.precioLibra),
                       "L " + formatoPrecio(l.precioLibra * LIBRAS_POR_QUINTAL), formatoLempiras(l.subtotal)},
                      static_cast<LPARAM>(i));
        }
        if (!pagadoEditado_) {
            actualizando_ = true;
            ponerTexto(h_, IDC_F_PAGADO, lineas_.empty() ? std::string() : formatoMonto(total()));
            actualizando_ = false;
        }
        actualizarTotales();
    }

    Centavos total() const {
        Centavos t = 0;
        for (const auto& l : lineas_) t += l.subtotal;
        return t;
    }

    void actualizarTotales() {
        const Centavos t = total();
        double libras = 0;
        for (const auto& l : lineas_) libras += l.libras;
        ponerTexto(h_, IDC_F_TOTAL, formatoLempiras(t) + "   (" + formatoDecimal(libras, 2) + " lb = " +
                                        formatoDecimal(aQuintales(libras), 2) + " qq)");
        const auto pagado = vacio(h_, IDC_F_PAGADO) ? std::optional<Centavos>(0) : leerMonto(h_, IDC_F_PAGADO);
        if (!pagado) {
            ponerTexto(h_, IDC_F_SALDO, "monto inválido");
            colorTexto(IDC_F_SALDO, COLOR_ROJO);
            return;
        }
        const Centavos saldo = t - *pagado;
        ponerTexto(h_, IDC_F_SALDO, formatoLempiras(saldo));
        colorTexto(IDC_F_SALDO, saldo > 0 ? COLOR_ROJO : (saldo < 0 ? COLOR_ROJO : COLOR_VERDE));
        habilitar(h_, IDC_F_VENCE, saldo > 0);
    }

    void guardar(bool imprimir) {
        if (!agregarLinea(false)) return;  // agrega la línea escrita si quedó sin agregar
        if (lineas_.empty()) {
            mostrarAviso(h_, "Agregue al menos una línea de café pesado (tipo, peso y precio) con el botón "
                             "\"Agregar línea\" o con Enter.");
            enfocar(h_, IDC_F_CANTIDAD);
            return;
        }
        const auto pagado = vacio(h_, IDC_F_PAGADO) ? std::optional<Centavos>(0) : leerMonto(h_, IDC_F_PAGADO);
        if (!pagado) {
            mostrarAviso(h_, "El monto pagado no es válido.");
            enfocar(h_, IDC_F_PAGADO);
            return;
        }
        Factura f;
        Cliente c;
        if (!intentar(h_, [&] { c = cliente_.obtenerORegistrar(); })) return;
        f.clienteId = c.id;
        f.fecha = leerFecha(h_, IDC_F_FECHA);
        f.lineas = lineas_;
        f.pagado = *pagado;
        f.vencimientoSaldo = leerFechaOpc(h_, IDC_F_VENCE);
        f.observaciones = texto(h_, IDC_F_OBS);
        recalcularFactura(f);
        std::string pregunta = "¿Guardar el comprobante de compra?\n\nProductor: " + c.nombre + " (" + c.dni +
                               ")\nTotal: " + formatoLempiras(f.total) + "\nPagado ahora: " + formatoLempiras(f.pagado);
        if (f.saldo > 0)
            pregunta += "\nSaldo que queda debiendo el beneficio: " + formatoLempiras(f.saldo) +
                        (f.vencimientoSaldo ? " (pagar el " + f.vencimientoSaldo->texto() + ")" : std::string(" (sin fecha)"));
        if (!confirmar(h_, pregunta)) return;
        if (!intentar(h_, [&] { repo().registrarFactura(f); })) return;
        const std::string numero = f.numeroTexto();
        const long id = f.id;
        limpiar();
        if (f.pagado > 0) App::instancia().avisarCajaBaja(h_);
        // Copia en PDF en la carpeta del cliente (Documentos\Facturas\<cliente>).
        std::string avisoPdf;
        if (repo().config("GuardarPdfComprobante", "1") == "1") {
            try {
                CursorEspera espera;
                guardarPdfComprobante(id);
            } catch (const std::exception& e) {
                avisoPdf = std::string("\n\n(No se pudo guardar el PDF: ") + e.what() + ")";
            }
        }
        if (imprimir) {
            // Directo a la impresora, sin cuadros de diálogo.
            try {
                CursorEspera espera;
                const std::wstring impresora = imprimirComprobante(id);
                mostrarInfo(h_, "Factura " + numero + " guardada e impresa en \"" + U(impresora) +
                                    "\"." + avisoPdf);
            } catch (const std::exception& e) {
                if (confirmar(h_, "La factura " + numero + " se guardó, pero no se pudo imprimir:\n" +
                                      e.what() + "\n\n¿Desea verlo en pantalla?"))
                    verComprobante(h_, id);
            }
        } else {
            if (!avisoPdf.empty()) mostrarAviso(h_, "Comprobante guardado." + avisoPdf);
            verComprobante(h_, id);  // se muestra dentro del programa
        }
    }

    // Número que tendrá la factura (U- o P- según el tipo de café) y su tipo, bien visible.
    void actualizarNumero() {
        const int categoria = categoriaDe(lineas_.empty() ? tipoActual() : lineas_.front().tipoCafe);
        ponerTexto(h_, IDC_F_NUMERO, numeroFactura(categoria, repo().siguienteNumeroFactura(categoria)) + "  ·  " +
                                         (categoria == CATEGORIA_UVA ? "UVA" : "PERGAMINO"));
        colorTexto(IDC_F_NUMERO, categoria == CATEGORIA_UVA ? RGB(155, 27, 48) : RGB(138, 90, 60));
    }

    SIZE tamLogo_{0, 0};
    EntradaCliente cliente_;
    std::vector<LineaFactura> lineas_;
    PreciosDia precios_;
    bool pagadoEditado_ = false;
    bool actualizando_ = false;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaFactura() { return std::make_unique<PaginaFactura>(); }

}  // namespace bu::win
