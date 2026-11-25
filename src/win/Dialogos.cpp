#include "win/Dialogos.h"

#include "core/Identificacion.h"
#include "core/Texto.h"
#include "win/App.h"
#include "win/Excel.h"
#include "win/Sesion.h"
#include "win/resource.h"

namespace bu::win {

// ================================================================ DialogoModal
INT_PTR DialogoModal::mostrar(HWND padre, int plantilla) {
    const std::vector<BYTE> t = plantillaDialogo(plantilla, tamanoLetra());
    if (t.empty()) return IDCANCEL;
    return DialogBoxIndirectParamW(App::instancia().hInst, reinterpret_cast<LPCDLGTEMPLATEW>(t.data()), padre, proc,
                                   reinterpret_cast<LPARAM>(this));
}

INT_PTR CALLBACK DialogoModal::proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<DialogoModal*>(GetWindowLongPtrW(h, DWLP_USER));
    switch (msg) {
        case WM_INITDIALOG:
            self = reinterpret_cast<DialogoModal*>(lp);
            SetWindowLongPtrW(h, DWLP_USER, lp);
            self->h_ = h;
            self->alIniciar();
            {
                // Si alIniciar ya puso el foco en un campo, se respeta (FALSE); si no, Windows elige.
                HWND foco = GetFocus();
                return (foco && IsChild(h, foco)) ? FALSE : TRUE;
            }
        case WM_COMMAND: {
            if (!self) return FALSE;
            const int id = LOWORD(wp);
            if (id == IDOK) {
                if (self->alAceptar()) EndDialog(h, IDOK);
                return TRUE;
            }
            if (id == IDCANCEL) {
                EndDialog(h, IDCANCEL);
                return TRUE;
            }
            return self->alComando(id, HIWORD(wp)) ? TRUE : FALSE;
        }
        case WM_NOTIFY: {
            if (!self) return FALSE;
            LRESULT r = 0;
            if (self->alNotificar(reinterpret_cast<NMHDR*>(lp), r)) {
                SetWindowLongPtrW(h, DWLP_MSGRESULT, r);
                return TRUE;
            }
            return FALSE;
        }
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            return self ? self->alColor(msg, reinterpret_cast<HDC>(wp), reinterpret_cast<HWND>(lp)) : FALSE;
        default:
            return FALSE;
    }
}

// ================================================================ Colores
COLORREF colorDeuda(const Deuda& d, const Fecha& hoy, int dias) {
    if (d.anulada) return COLOR_GRIS;
    if (d.saldo <= 0) return COLOR_VERDE;
    switch (urgenciaDeuda(d, hoy, dias)) {
        case -1: return COLOR_ROJO;
        case 0: return COLOR_NARANJA;
        case 1: return COLOR_CAFE;
        default: return GetSysColor(COLOR_WINDOWTEXT);
    }
}

bool colorearFilas(NMHDR* nm, const std::map<LPARAM, COLORREF>& colores, LRESULT& resultado) {
    if (nm->code != NM_CUSTOMDRAW) return false;
    auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(nm);
    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) {
        resultado = CDRF_NOTIFYITEMDRAW;
        return true;
    }
    if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        const auto it = colores.find(static_cast<LPARAM>(cd->nmcd.lItemlParam));
        if (it != colores.end()) cd->clrText = it->second;
        resultado = CDRF_DODEFAULT;
        return true;
    }
    return false;
}

// ================================================================ EntradaCliente
void EntradaCliente::configurar(HWND dlg, int idDni, int idNombre, int idTelefono, int idInfo, int tipoCliente) {
    dlg_ = dlg;
    idDni_ = idDni;
    idNombre_ = idNombre;
    idTelefono_ = idTelefono;
    idInfo_ = idInfo;
    tipo_ = tipoCliente;
    SendDlgItemMessageW(dlg_, idDni_, EM_LIMITTEXT, 20, 0);
    SendDlgItemMessageW(dlg_, idNombre_, EM_LIMITTEXT, 150, 0);
}

void EntradaCliente::info(const std::string& texto, bool advertencia) {
    if (idInfo_) ponerTexto(dlg_, idInfo_, advertencia ? "¡Atención! " + texto : texto);
}

bool EntradaCliente::tieneFoco() const { return GetFocus() == GetDlgItem(dlg_, idDni_); }

bool EntradaCliente::comando(int id, int codigo) {
    if (id == idDni_) {
        if (codigo == EN_CHANGE && !poniendo_ && cliente_) {
            // Se cambió el DNI de un cliente ya cargado: se vuelve a empezar.
            cliente_.reset();
            poniendo_ = true;
            ponerTexto(dlg_, idNombre_, "");
            if (idTelefono_) ponerTexto(dlg_, idTelefono_, "");
            poniendo_ = false;
            soloLectura(dlg_, idNombre_, false);
            info("");
        } else if (codigo == EN_KILLFOCUS) {
            if (tieneLetras(texto(dlg_, idDni_)))
                info("Presione Enter para buscar el cliente por nombre.");
            else
                buscar();
        }
        return true;
    }
    if (id == idNombre_ || id == idTelefono_) return true;
    return false;
}

void EntradaCliente::buscar() {
    const std::string dni = recortar(texto(dlg_, idDni_));
    if (dni.empty()) {
        info("");
        return;
    }
    if (cliente_ && soloDigitos(cliente_->dni) == soloDigitos(dni)) return;
    const auto v = validarIdentificacion(dni, Fecha::hoy().anio);
    if (!v.valido) {
        info(v.error, true);
        return;
    }
    intentar(dlg_, [&] {
        if (auto c = repo().clientePorDni(v.formateado)) {
            poner(*c);
        } else {
            poniendo_ = true;
            ponerTexto(dlg_, idDni_, v.formateado);
            poniendo_ = false;
            soloLectura(dlg_, idNombre_, false);
            info("Cliente nuevo: escriba el nombre completo. Se registrará al guardar.");
            if (vacio(dlg_, idNombre_)) enfocar(dlg_, idNombre_);
        }
    });
}

void EntradaCliente::elegir(const std::string& filtro) {
    if (auto c = seleccionarCliente(dlg_, filtro)) poner(*c);
}

bool EntradaCliente::alPresionarEnter() {
    HWND foco = GetFocus();
    if (foco == GetDlgItem(dlg_, idDni_)) {
        const std::string t = recortar(texto(dlg_, idDni_));
        if (tieneLetras(t)) {
            // Se escribió un nombre en el campo DNI: se busca por nombre.
            elegir(t);
            if (!cliente_) {
                poniendo_ = true;
                ponerTexto(dlg_, idDni_, "");
                poniendo_ = false;
                if (vacio(dlg_, idNombre_)) ponerTexto(dlg_, idNombre_, t);
                info("No se eligió un cliente registrado. Escriba el DNI del cliente nuevo.", true);
                enfocar(dlg_, idDni_);
            }
        } else {
            buscar();
        }
        return true;
    }
    if (foco == GetDlgItem(dlg_, idNombre_) && !cliente_) {
        const std::string nombre = recortar(texto(dlg_, idNombre_));
        if (nombre.size() < 3) return false;
        bool hayParecidos = false;
        intentar(dlg_, [&] {
            for (const auto& c : repo().clientes())
                if (coincideBusqueda(c.nombre, nombre)) hayParecidos = true;
        });
        if (!hayParecidos) return false;
        elegir(nombre);
        return cliente_.has_value();
    }
    return false;
}

void EntradaCliente::poner(const Cliente& c) {
    poniendo_ = true;
    ponerTexto(dlg_, idDni_, c.dni);
    ponerTexto(dlg_, idNombre_, c.nombre);
    if (idTelefono_) ponerTexto(dlg_, idTelefono_, c.telefono);
    poniendo_ = false;
    soloLectura(dlg_, idNombre_, true);
    cliente_ = c;
    std::string t = "Cliente registrado.";
    try {
        const ResumenDeudas rd = repo().resumenDeudas(c.id);
        if (rd.porPagar > 0) t += "  El beneficio le debe " + formatoLempiras(rd.porPagar) + ".";
        if (rd.porCobrar > 0) t += "  Le debe al beneficio " + formatoLempiras(rd.porCobrar) + ".";
        if (rd.porPagar == 0 && rd.porCobrar == 0) t += "  Sin deudas pendientes.";
    } catch (...) {
    }
    info(t);
}

void EntradaCliente::limpiar() {
    poniendo_ = true;
    ponerTexto(dlg_, idDni_, "");
    ponerTexto(dlg_, idNombre_, "");
    if (idTelefono_) ponerTexto(dlg_, idTelefono_, "");
    poniendo_ = false;
    soloLectura(dlg_, idNombre_, false);
    cliente_.reset();
    info("Escriba el DNI y presione Enter, o use Buscar... para buscarlo por nombre.");
}

Cliente EntradaCliente::obtenerORegistrar() {
    const std::string dni = recortar(texto(dlg_, idDni_));
    if (cliente_ && soloDigitos(cliente_->dni) == soloDigitos(dni)) return *cliente_;
    if (dni.empty()) throw ErrorNegocio("Escriba el DNI del cliente (o búsquelo con el botón Buscar...).");
    const std::string telefono = idTelefono_ ? texto(dlg_, idTelefono_) : std::string();
    Cliente c = repo().obtenerORegistrarCliente(dni, texto(dlg_, idNombre_), telefono, tipo_);
    poner(c);
    return c;
}

// ================================================================ Seleccionar cliente
namespace {

class DlgSeleccionarCliente : public DialogoModal {
public:
    explicit DlgSeleccionarCliente(std::string filtro) : filtroInicial_(std::move(filtro)) {}
    std::optional<Cliente> elegido;

protected:
    void alIniciar() override {
        lvConfigurar(h_, IDC_S_LISTA,
                     {{L"Nombre", 160}, {L"DNI / RTN", 80}, {L"Teléfono", 64}, {L"Dirección", 110}, {L"Tipo", 66}});
        ponerFuente(h_, IDC_S_BUSCAR, fuente(tamanoLetra() * 11.0 / 9.0, false));
        textoGuia(h_, IDC_S_BUSCAR, L"Nombre, apellido o DNI");
        intentar(h_, [&] { todos_ = repo().clientes(); });
        ponerTexto(h_, IDC_S_BUSCAR, filtroInicial_);
        filtrar();
        enfocar(h_, IDC_S_BUSCAR);
        SendDlgItemMessageW(h_, IDC_S_BUSCAR, EM_SETSEL, static_cast<WPARAM>(filtroInicial_.size()),
                            static_cast<LPARAM>(filtroInicial_.size()));
    }
    bool alComando(int id, int codigo) override {
        if (id == IDC_S_BUSCAR && codigo == EN_CHANGE) {
            filtrar();
            return true;
        }
        return false;
    }
    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if (nm->idFrom == IDC_S_LISTA && nm->code == NM_DBLCLK) {
            if (alAceptar()) EndDialog(h_, IDOK);
            r = 0;
            return true;
        }
        return false;
    }
    bool alAceptar() override {
        const LPARAM id = lvDatoSeleccionado(h_, IDC_S_LISTA, 0);
        for (const auto& c : todos_)
            if (c.id == id) {
                elegido = c;
                return true;
            }
        mostrarAviso(h_, "Seleccione un cliente de la lista.");
        return false;
    }

private:
    void filtrar() {
        lvLimpiar(h_, IDC_S_LISTA);
        const std::string f = texto(h_, IDC_S_BUSCAR);
        for (const auto& c : todos_)
            if (coincideBusqueda(c.nombre + " " + c.dni + " " + c.telefono, f))
                lvAgregar(h_, IDC_S_LISTA, {c.nombre, c.dni, c.telefono, c.direccion, nombreTipoCliente(c.tipo)}, c.id);
        if (ListView_GetItemCount(GetDlgItem(h_, IDC_S_LISTA)) > 0) lvSeleccionar(h_, IDC_S_LISTA, 0);
    }
    std::vector<Cliente> todos_;
    std::string filtroInicial_;
};

// ================================================================ Nueva deuda / préstamo
class DlgNuevaDeuda : public DialogoModal {
protected:
    void alIniciar() override {
        cliente_.configurar(h_, IDC_N_DNI, IDC_N_NOMBRE, 0, IDC_N_INFO, CLIENTE_PRODUCTOR);
        cliente_.limpiar();
        CheckRadioButton(h_, IDC_N_PORCOBRAR, IDC_N_PORPAGAR, IDC_N_PORCOBRAR);
        for (const char* o : {"Préstamo", "Adelanto", "Deuda anterior", "Otro"}) comboAgregar(h_, IDC_N_ORIGEN, o, 0);
        SendDlgItemMessageW(h_, IDC_N_ORIGEN, CB_SETCURSEL, 0, 0);
        configurarFecha(h_, IDC_N_FECHA);
        configurarFecha(h_, IDC_N_VENCE);
        ponerFecha(h_, IDC_N_FECHA, Fecha::hoy());
        ponerFechaOpc(h_, IDC_N_VENCE, std::nullopt);
        CheckDlgButton(h_, IDC_N_CAJA, BST_CHECKED);
        actualizarInfoCaja();
        enfocar(h_, IDC_N_DNI);
    }
    bool alComando(int id, int codigo) override {
        if (cliente_.comando(id, codigo)) return true;
        switch (id) {
            case IDC_N_BUSCAR:
                cliente_.elegir();
                return true;
            case IDC_N_PORCOBRAR:
            case IDC_N_PORPAGAR:
            case IDC_N_CAJA:
                actualizarInfoCaja();
                return true;
            case IDC_N_ORIGEN:
                if (codigo == CBN_SELCHANGE) {
                    // Una deuda anterior al programa normalmente no mueve la caja de hoy.
                    CheckDlgButton(h_, IDC_N_CAJA, origen() == "Deuda anterior" ? BST_UNCHECKED : BST_CHECKED);
                    actualizarInfoCaja();
                }
                return true;
            default:
                return false;
        }
    }
    bool alAceptar() override {
        if (cliente_.alPresionarEnter()) return false;
        bool cancelado = false;
        const bool ok = intentar(h_, [&] {
            const auto monto = leerMonto(h_, IDC_N_MONTO);
            if (!monto || *monto <= 0) throw ErrorNegocio("Escriba un monto válido mayor que cero.");
            Cliente c = cliente_.obtenerORegistrar();
            Deuda d;
            d.clienteId = c.id;
            d.tipo = porCobrar() ? TipoDeuda::PorCobrar : TipoDeuda::PorPagar;
            d.origen = origen();
            d.monto = *monto;
            d.fecha = leerFecha(h_, IDC_N_FECHA);
            d.vencimiento = leerFechaOpc(h_, IDC_N_VENCE);
            d.concepto = texto(h_, IDC_N_CONCEPTO);
            const bool caja = IsDlgButtonChecked(h_, IDC_N_CAJA) == BST_CHECKED;
            std::string pregunta = "¿Registrar " + std::string(porCobrar() ? "que " + c.nombre + " le debe al beneficio "
                                                                           : "que el beneficio le debe a " + c.nombre + " ") +
                                   formatoLempiras(d.monto) + "?";
            if (caja) pregunta += porCobrar() ? "\n\nSe registrará una SALIDA de caja." : "\n\nSe registrará una ENTRADA de caja.";
            if (!confirmar(h_, pregunta)) {
                cancelado = true;
                return;
            }
            repo().registrarDeuda(d, caja);
        });
        return ok && !cancelado;
    }

private:
    bool porCobrar() const { return IsDlgButtonChecked(h_, IDC_N_PORCOBRAR) == BST_CHECKED; }
    std::string origen() const {
        const LRESULT i = SendDlgItemMessageW(h_, IDC_N_ORIGEN, CB_GETCURSEL, 0, 0);
        wchar_t buf[64] = {0};
        if (i != CB_ERR) SendDlgItemMessageW(h_, IDC_N_ORIGEN, CB_GETLBTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(buf));
        return U(buf);
    }
    void actualizarInfoCaja() {
        const bool caja = IsDlgButtonChecked(h_, IDC_N_CAJA) == BST_CHECKED;
        std::string t;
        if (porCobrar())
            t = caja ? "Se registrará una SALIDA de dinero de la caja por el monto (el dinero se le entrega al cliente)."
                     : "No mueve la caja (por ejemplo, una deuda que ya existía antes de usar el programa).";
        else
            t = caja ? "Se registrará una ENTRADA de dinero a la caja por el monto (el beneficio recibe el dinero)."
                     : "No mueve la caja (por ejemplo, algo que el beneficio ya debía antes de usar el programa).";
        ponerTexto(h_, IDC_N_CAJA_INFO, t);
    }
    EntradaCliente cliente_;
};

// ================================================================ Abono
class DlgAbono : public DialogoModal {
public:
    explicit DlgAbono(long deudaId) : deudaId_(deudaId) {}

protected:
    void alIniciar() override {
        configurarFecha(h_, IDC_A_FECHA);
        ponerFecha(h_, IDC_A_FECHA, Fecha::hoy());
        intentar(h_, [&] {
            const auto d = repo().deudaPorId(deudaId_);
            if (!d) throw ErrorNegocio("La deuda no existe.");
            deuda_ = *d;
        });
        ponerTexto(h_, IDC_A_INFO, "Deuda #" + std::to_string(deuda_.id) + " de " + deuda_.clienteNombre + " (" +
                                       deuda_.clienteDni + ")\n" + descripcionTipoDeuda(deuda_.tipo) +
                                       "\nSaldo pendiente: " + formatoLempiras(deuda_.saldo));
        ponerTexto(h_, IDC_A_CAJA, deuda_.tipo == TipoDeuda::PorCobrar
                                       ? "El abono se registrará como ENTRADA de dinero a la caja."
                                       : "El pago se registrará como SALIDA de dinero de la caja.");
        enfocar(h_, IDC_A_MONTO);
    }
    bool alComando(int id, int) override {
        if (id == IDC_A_TODO) {
            ponerTexto(h_, IDC_A_MONTO, formatoMonto(deuda_.saldo));
            return true;
        }
        return false;
    }
    bool alAceptar() override {
        return intentar(h_, [&] {
            const auto monto = leerMonto(h_, IDC_A_MONTO);
            if (!monto) throw ErrorNegocio("Escriba el monto del abono.");
            repo().registrarAbono(deudaId_, leerFecha(h_, IDC_A_FECHA), *monto, texto(h_, IDC_A_NOTA));
        });
    }

private:
    long deudaId_;
    Deuda deuda_;
};

// ================================================================ Detalle de deuda
class DlgDetalleDeuda : public DialogoModal {
public:
    explicit DlgDetalleDeuda(long deudaId) : deudaId_(deudaId) {}

protected:
    void alIniciar() override {
        lvConfigurar(h_, IDC_DD_LISTA,
                     {{L"No.", 34}, {L"Fecha", 54}, {L"Hora", 40}, {L"Monto", 70, true}, {L"Nota", 110}, {L"Estado", 50}});
        cargar();
    }
    bool alComando(int id, int) override {
        if (id == IDC_DD_ABONAR) {
            if (dialogoAbono(h_, deudaId_)) cargar();
            return true;
        }
        if (id == IDC_DD_ANULAR) {
            const long abonoId = static_cast<long>(lvDatoSeleccionado(h_, IDC_DD_LISTA, 0));
            if (!abonoId) {
                mostrarAviso(h_, "Seleccione el abono que desea anular.");
                return true;
            }
            if (confirmar(h_, "¿Anular el abono seleccionado? El saldo de la deuda aumentará y se registrará el "
                              "movimiento contrario en la caja de hoy.") &&
                autorizar(h_, "anular un abono"))
                if (intentar(h_, [&] { repo().anularAbono(abonoId, Fecha::hoy()); })) cargar();
            return true;
        }
        return false;
    }

private:
    void cargar() {
        intentar(h_, [&] {
            const auto d = repo().deudaPorId(deudaId_);
            if (!d) throw ErrorNegocio("La deuda no existe.");
            const auto abonos = repo().abonos(deudaId_);
            Centavos abonado = 0;
            lvLimpiar(h_, IDC_DD_LISTA);
            for (const auto& a : abonos) {
                if (!a.anulado) abonado += a.monto;
                lvAgregar(h_, IDC_DD_LISTA,
                          {std::to_string(a.id), a.fecha.texto(), a.hora.substr(0, 5), formatoLempiras(a.monto), a.nota,
                           a.anulado ? "Anulado" : "Válido"},
                          a.id);
            }
            const Fecha hoy = Fecha::hoy();
            ponerTexto(h_, IDC_DD_INFO,
                       "Deuda #" + std::to_string(d->id) + "  ·  " + d->clienteNombre + " (" + d->clienteDni + ")\n" +
                           descripcionTipoDeuda(d->tipo) + "\nOrigen: " + d->origen + "  ·  " + d->concepto +
                           "\nFecha: " + d->fecha.texto() + "  ·  Fecha de pago: " +
                           (d->vencimiento ? d->vencimiento->texto() : std::string("sin fecha")) + "  ·  " +
                           estadoDeuda(*d, hoy) + "\nMonto: " + formatoLempiras(d->monto) +
                           "   Abonado: " + formatoLempiras(abonado) + "   Saldo: " + formatoLempiras(d->saldo));
            habilitar(h_, IDC_DD_ABONAR, d->pendiente());
        });
    }
    long deudaId_;
};

// ================================================================ Movimiento manual
class DlgMovimiento : public DialogoModal {
public:
    explicit DlgMovimiento(const Fecha& f) : fecha_(f) {}

protected:
    void alIniciar() override {
        ponerTexto(h_, IDC_M_INFO, "Caja del " + fecha_.texto());
        CheckRadioButton(h_, IDC_M_INGRESO, IDC_M_EGRESO, IDC_M_INGRESO);
        enfocar(h_, IDC_M_MONTO);
    }
    bool alAceptar() override {
        return intentar(h_, [&] {
            const auto monto = leerMonto(h_, IDC_M_MONTO);
            if (!monto) throw ErrorNegocio("Escriba el monto.");
            const bool entrada = IsDlgButtonChecked(h_, IDC_M_INGRESO) == BST_CHECKED;
            repo().registrarMovimientoManual(fecha_, entrada ? TipoMovimiento::Ingreso : TipoMovimiento::Egreso, *monto,
                                             texto(h_, IDC_M_CONCEPTO));
        });
    }

private:
    Fecha fecha_;
};

// ================================================================ Pedir texto
class DlgTexto : public DialogoModal {
public:
    DlgTexto(std::string titulo, std::string pregunta, std::string inicial)
        : valor(std::move(inicial)), titulo_(std::move(titulo)), pregunta_(std::move(pregunta)) {}
    std::string valor;

protected:
    void alIniciar() override {
        SetWindowTextW(h_, W(titulo_).c_str());
        ponerTexto(h_, IDC_T_PROMPT, pregunta_);
        ponerTexto(h_, IDC_T_TEXTO, valor);
        enfocar(h_, IDC_T_TEXTO);
    }
    bool alAceptar() override {
        valor = recortar(texto(h_, IDC_T_TEXTO));
        if (valor.empty()) {
            mostrarAviso(h_, "Escriba un texto.");
            return false;
        }
        return true;
    }

private:
    std::string titulo_, pregunta_;
};

// ================================================================ Recordatorio
class DlgRecordatorio : public DialogoModal {
public:
    explicit DlgRecordatorio(const Recordatorio& r) : r_(r) {}

protected:
    void alIniciar() override {
        lvConfigurar(h_, IDC_RC_LISTA,
                     {{L"Situación", 80}, {L"Cliente", 110}, {L"Tipo", 62}, {L"Concepto", 110}, {L"Fecha de pago", 54},
                      {L"Saldo", 64, true}});
        std::string t = "Hoy es " + r_.hoy.texto() + ".\n";
        if (!r_.hayDeudasUrgentes()) t += "No hay deudas vencidas ni por vencer en los próximos " +
                                          std::to_string(r_.diasAnticipacion) + " días.\n";
        if (!r_.vencidas.empty()) t += "• " + std::to_string(r_.vencidas.size()) + " deuda(s) VENCIDA(S).   ";
        if (!r_.venceHoy.empty()) t += "• " + std::to_string(r_.venceHoy.size()) + " vence(n) HOY.   ";
        if (!r_.proximas.empty())
            t += "• " + std::to_string(r_.proximas.size()) + " vence(n) en los próximos " +
                 std::to_string(r_.diasAnticipacion) + " días.";
        if (r_.hayDeudasUrgentes()) t += "\n";
        t += "Total que el beneficio debe: " + formatoLempiras(r_.totales.porPagar) +
             "   ·   Total que le deben al beneficio: " + formatoLempiras(r_.totales.porCobrar);
        if (r_.pendientesSinFecha > 0)
            t += "   (" + std::to_string(r_.pendientesSinFecha) + " pendiente(s) sin fecha de pago)";
        if (r_.faltaPrecioHoy) t += "\n¡Atención! Falta ingresar el precio del café de hoy (pestaña Precio del día).";
        if (r_.diaSinCerrar) t += "\n¡Atención! La caja del " + r_.diaSinCerrar->texto() + " no se cerró (pestaña Caja).";
        ponerTexto(h_, IDC_RC_RESUMEN, t);

        auto agregar = [&](const std::vector<Deuda>& lista) {
            for (const auto& d : lista) {
                lvAgregar(h_, IDC_RC_LISTA,
                          {estadoDeuda(d, r_.hoy), d.clienteNombre, nombreTipoDeuda(d.tipo), d.concepto,
                           d.vencimiento ? d.vencimiento->texto() : std::string(), formatoLempiras(d.saldo)},
                          d.id);
                colores_[d.id] = colorDeuda(d, r_.hoy, r_.diasAnticipacion);
            }
        };
        agregar(r_.vencidas);
        agregar(r_.venceHoy);
        agregar(r_.proximas);
    }
    bool alComando(int id, int) override {
        if (id == IDC_RC_IR) {
            App::instancia().irAPagina(PAG_DEUDAS);
            EndDialog(h_, IDCANCEL);
            return true;
        }
        if (id == IDC_RC_EXCEL) {
            Documento doc;
            if (intentar(h_, [&] { doc = docDeudasPendientes(repo().empresa(), repo().deudas(FiltroDeudas{}), r_.hoy); }))
                generarYMostrar(h_, doc);
            return true;
        }
        return false;
    }
    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if (nm->idFrom == IDC_RC_LISTA) return colorearFilas(nm, colores_, r);
        return false;
    }

private:
    Recordatorio r_;
    std::map<LPARAM, COLORREF> colores_;
};

}  // namespace

// ================================================================ Funciones públicas
std::optional<Cliente> seleccionarCliente(HWND padre, const std::string& filtroInicial) {
    DlgSeleccionarCliente d(filtroInicial);
    if (d.mostrar(padre, IDD_SEL_CLIENTE) == IDOK) return d.elegido;
    return std::nullopt;
}

bool dialogoNuevaDeuda(HWND padre) {
    DlgNuevaDeuda d;
    return d.mostrar(padre, IDD_NUEVA_DEUDA) == IDOK;
}

void dialogoDetalleDeuda(HWND padre, long deudaId) {
    DlgDetalleDeuda d(deudaId);
    d.mostrar(padre, IDD_DETALLE_DEUDA);
}

bool dialogoAbono(HWND padre, long deudaId) {
    DlgAbono d(deudaId);
    return d.mostrar(padre, IDD_ABONO) == IDOK;
}

bool dialogoMovimiento(HWND padre, const Fecha& fecha) {
    DlgMovimiento d(fecha);
    return d.mostrar(padre, IDD_MOVIMIENTO) == IDOK;
}

std::optional<std::string> pedirTexto(HWND padre, const std::string& titulo, const std::string& pregunta,
                                      const std::string& inicial) {
    DlgTexto d(titulo, pregunta, inicial);
    if (d.mostrar(padre, IDD_TEXTO) == IDOK) return d.valor;
    return std::nullopt;
}

void dialogoRecordatorio(HWND padre, const Recordatorio& r) {
    DlgRecordatorio d(r);
    d.mostrar(padre, IDD_RECORDATORIO);
}

}  // namespace bu::win
