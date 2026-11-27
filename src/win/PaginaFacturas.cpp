// Pestaña "Facturas": comprobantes de compra agrupados en una carpeta por cliente.
// Se ven e imprimen dentro del programa (sin abrir Excel).
#include <algorithm>
#include <cstdio>
#include <map>

#include "core/Texto.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Impresion.h"
#include "win/Paginas.h"
#include "win/Sesion.h"
#include "win/Tema.h"
#include "win/resource.h"

#include <shellapi.h>

namespace bu::win {

namespace {

constexpr LPARAM TODOS = -1;

struct Carpeta {
    long clienteId = 0;
    std::string nombre;
    std::string dni;
    int cantidad = 0;
    Centavos total = 0;
    Centavos saldo = 0;
};

class PaginaFacturas : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Facturas"; }

    void alIniciar() override {
        controlEstirable_ = IDC_H_LISTA;
        configurarFecha(h_, IDC_H_DESDE);
        configurarFecha(h_, IDC_H_HASTA);
        ponerFecha(h_, IDC_H_DESDE, Fecha::hoy().primerDiaDelMes());
        ponerFecha(h_, IDC_H_HASTA, Fecha::hoy());
        textoGuia(h_, IDC_H_FILTRO, L"Buscar cliente, DNI o número de factura...");
        tema::campoBusqueda(GetDlgItem(h_, IDC_H_FILTRO));
        const char* periodos[] = {"Hoy", "Esta semana", "Este mes", "Mes anterior", "Esta cosecha", "Este año", "Elegir fechas..."};
        for (int i = 0; i < 7; ++i) comboAgregar(h_, IDC_H_PERIODO, periodos[i], i);
        comboSeleccionar(h_, IDC_H_PERIODO, P_MES);
        mostrarFechas(false);
        lvConfigurar(h_, IDC_H_CLIENTES, {{L"Cliente", 150}, {L"Facturas", 34, true}});
        lvConfigurar(h_, IDC_H_LISTA,
                     {{L"No.", 60}, {L"Tipo", 70}, {L"Fecha", 66}, {L"Total", 90, true}, {L"Saldo", 80, true}});
        // Ícono de carpeta en la lista de clientes.
        HIMAGELIST iconos = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                             ILC_COLOR32 | ILC_MASK, 1, 1);
        SHSTOCKICONINFO sii{};
        sii.cbSize = sizeof sii;
        if (SUCCEEDED(SHGetStockIconInfo(SIID_FOLDER, SHGSI_ICON | SHGSI_SMALLICON, &sii))) {
            ImageList_AddIcon(iconos, sii.hIcon);
            DestroyIcon(sii.hIcon);
        }
        ListView_SetImageList(GetDlgItem(h_, IDC_H_CLIENTES), iconos, LVSIL_SMALL);
        ponerFuente(h_, IDC_H_RESUMEN, fuenteNegrita());
        colorTexto(IDC_H_RESUMEN, COLOR_CAFE);
    }

    void alMostrar() override {
        if (comboDato(h_, IDC_H_PERIODO, P_MES) != P_ELEGIR) aplicarPeriodo();
        buscar();
    }

    void alRedimensionar(int ancho, int alto) override {
        estirar(IDC_H_CLIENTES, ancho, alto, false, true);
        estirar(IDC_H_LISTA, ancho, alto, true, true);
    }

    bool alComando(int id, int codigo) override {
        switch (id) {
            case IDC_H_FILTRO:
                if (codigo == EN_CHANGE) mostrarCarpetas();
                return true;
            case IDC_H_PERIODO:
                if (codigo == CBN_SELCHANGE) {
                    aplicarPeriodo();
                    buscar();
                }
                return true;
            case IDC_H_VER:
                if (const long f = seleccionada()) verComprobante(h_, f);
                return true;
            case IDC_H_IMPRIMIR:
                if (const long f = seleccionada())
                    intentar(h_, [&] {
                        CursorEspera espera;
                        mostrarInfo(h_, "Comprobante enviado a la impresora \"" + U(imprimirComprobante(f)) + "\".");
                    });
                return true;
            case IDC_H_CARPETA:
                abrirCarpetaCliente();
                return true;
            case IDC_H_EXCEL:
                if (const long f = seleccionada())
                    intentar(h_, [&] {
                        CursorEspera espera;
                        // Se abre el PDF ya guardado; solo se crea si todavía no existe.
                        std::wstring ruta = rutaPdfComprobante(f);
                        if (!existeArchivo(ruta)) ruta = guardarPdfComprobante(f);
                        ShellExecuteW(nullptr, L"open", ruta.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    });
                return true;
            case IDC_H_ANULAR:
                anular();
                return true;
            default:
                return false;
        }
    }

    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if ((nm->idFrom == IDC_H_DESDE || nm->idFrom == IDC_H_HASTA) && nm->code == DTN_DATETIMECHANGE) {
            buscar();
            r = 0;
            return true;
        }
        if (nm->idFrom == IDC_H_CLIENTES && nm->code == LVN_ITEMCHANGED) {
            const auto* nv = reinterpret_cast<NMLISTVIEW*>(nm);
            if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED)) {
                carpeta_ = nv->lParam;
                mostrarFacturas();
            }
            r = 0;
            return true;
        }
        if (nm->idFrom == IDC_H_CLIENTES && nm->code == NM_DBLCLK) {
            abrirCarpetaCliente();
            r = 0;
            return true;
        }
        if (nm->idFrom == IDC_H_LISTA && nm->code == NM_DBLCLK) {
            if (const long f = static_cast<long>(lvDatoSeleccionado(h_, IDC_H_LISTA, 0))) verComprobante(h_, f);
            r = 0;
            return true;
        }
        if (nm->idFrom == IDC_H_LISTA) return colorearFilas(nm, colores_, r);
        return false;
    }

    void alAceptar() override { buscar(); }

private:
    enum Periodo { P_HOY, P_SEMANA, P_MES, P_MES_ANTERIOR, P_COSECHA, P_ANIO, P_ELEGIR };

    void mostrarFechas(bool si) {
        for (int id : {IDC_H_DESDE, IDC_H_LBL_A, IDC_H_HASTA}) ShowWindow(GetDlgItem(h_, id), si ? SW_SHOW : SW_HIDE);
        tema::refrescar(h_);
    }

    // Pone las fechas según el período elegido (las fechas solo se ven con "Elegir fechas...").
    void aplicarPeriodo() {
        const Fecha hoy = Fecha::hoy();
        const int p = static_cast<int>(comboDato(h_, IDC_H_PERIODO, P_MES));
        Fecha desde = hoy, hasta = hoy;
        switch (p) {
            case P_HOY: break;
            case P_SEMANA: {
                const long n = hoy.numeroDia();
                const int diaSemana = static_cast<int>(((n % 7) + 7 + 3) % 7);  // 0 = lunes
                desde = hoy.sumarDias(-diaSemana);
                break;
            }
            case P_MES: desde = hoy.primerDiaDelMes(); break;
            case P_MES_ANTERIOR:
                hasta = hoy.primerDiaDelMes().sumarDias(-1);
                desde = hasta.primerDiaDelMes();
                break;
            case P_COSECHA: desde = inicioCosecha(hoy); break;
            case P_ANIO: desde = Fecha(hoy.anio, 1, 1); break;
            default: break;
        }
        if (p != P_ELEGIR) {
            ponerFecha(h_, IDC_H_DESDE, desde);
            ponerFecha(h_, IDC_H_HASTA, hasta);
        }
        mostrarFechas(p == P_ELEGIR);
    }

    void buscar() {
        facturas_ = repo().facturas(leerFecha(h_, IDC_H_DESDE), leerFecha(h_, IDC_H_HASTA));
        mostrarCarpetas();
    }

    // Una carpeta por cliente (aunque tenga muchos comprobantes).
    void mostrarCarpetas() {
        std::map<long, Carpeta> carpetas;
        std::map<long, std::string> numeros;  // números de factura de cada cliente (para buscar por número)
        for (const auto& f : facturas_) {
            numeros[f.clienteId] += " " + f.numeroTexto();
            Carpeta& c = carpetas[f.clienteId];
            c.clienteId = f.clienteId;
            c.nombre = f.clienteNombre;
            c.dni = f.clienteDni;
            ++c.cantidad;
            if (!f.anulada) {
                c.total += f.total;
                c.saldo += f.saldo;
            }
        }
        std::vector<Carpeta> lista;
        const std::string filtro = texto(h_, IDC_H_FILTRO);
        for (const auto& par : carpetas)
            if (coincideBusqueda(par.second.nombre + " " + par.second.dni + numeros[par.first], filtro))
                lista.push_back(par.second);
        std::sort(lista.begin(), lista.end(),
                  [](const Carpeta& a, const Carpeta& b) { return normalizarBusqueda(a.nombre) < normalizarBusqueda(b.nombre); });
        HWND lv = GetDlgItem(h_, IDC_H_CLIENTES);
        SendMessageW(lv, WM_SETREDRAW, FALSE, 0);
        lvLimpiar(h_, IDC_H_CLIENTES);
        if (filtro.empty()) agregarCarpeta("Todos los clientes", static_cast<int>(facturas_.size()), TODOS);
        int seleccion = -1;
        for (const auto& c : lista) {
            const int fila = agregarCarpeta(c.nombre, c.cantidad, c.clienteId);
            if (c.clienteId == carpeta_) seleccion = fila;
        }
        SendMessageW(lv, WM_SETREDRAW, TRUE, 0);
        const int total = ListView_GetItemCount(lv);
        if (total > 0) {
            if (seleccion < 0) seleccion = 0;
            carpeta_ = lvDato(h_, IDC_H_CLIENTES, seleccion);
            lvSeleccionar(h_, IDC_H_CLIENTES, seleccion);
        } else {
            carpeta_ = TODOS;
        }
        mostrarFacturas();
    }

    int agregarCarpeta(const std::string& nombre, int cantidad, LPARAM dato) {
        const int fila = lvAgregar(h_, IDC_H_CLIENTES, {nombre, std::to_string(cantidad)}, dato);
        LVITEMW it{};
        it.mask = LVIF_IMAGE;
        it.iItem = fila;
        it.iImage = 0;
        SendMessageW(GetDlgItem(h_, IDC_H_CLIENTES), LVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&it));
        return fila;
    }

    void mostrarFacturas() {
        lvLimpiar(h_, IDC_H_LISTA);
        colores_.clear();
        int cantidad = 0;
        double libras = 0;
        Centavos total = 0, pagado = 0, saldo = 0;
        for (const auto& f : facturas_) {
            if (carpeta_ != TODOS && f.clienteId != carpeta_) continue;

            lvAgregar(h_, IDC_H_LISTA,
                      {f.numeroTexto(), f.categoria == CATEGORIA_UVA ? "Uva" : "Pergamino", f.fecha.texto(),
                       formatoLempiras(f.total), f.anulada ? "Anulado" : (f.saldo > 0 ? formatoLempiras(f.saldo) : "Pagado")},
                      f.id);
            if (f.anulada) {
                colores_[f.id] = COLOR_GRIS;
                continue;
            }
            if (f.saldo > 0) colores_[f.id] = COLOR_ROJO;
            ++cantidad;
            libras += f.totalLibras;
            total += f.total;
            pagado += f.pagado;
            saldo += f.saldo;
        }
        ponerTexto(h_, IDC_H_RESUMEN,
                   std::to_string(cantidad) + " comprobantes  ·  " + formatoDecimal(aQuintales(libras), 2) +
                       " qq  ·  Total " + formatoLempiras(total) + "  ·  Pagado " + formatoLempiras(pagado) +
                       "  ·  Saldo " + formatoLempiras(saldo));
    }

    long seleccionada() {
        const long id = static_cast<long>(lvDatoSeleccionado(h_, IDC_H_LISTA, 0));
        if (!id) mostrarAviso(h_, "Seleccione un comprobante de la lista de la derecha.");
        return id;
    }

    void abrirCarpetaCliente() {
        long clienteId = carpeta_ == TODOS ? 0 : static_cast<long>(carpeta_);
        if (!clienteId) {
            const long f = static_cast<long>(lvDatoSeleccionado(h_, IDC_H_LISTA, 0));
            for (const auto& x : facturas_)
                if (x.id == f) clienteId = x.clienteId;
        }
        if (!clienteId) {
            abrirCarpeta(App::instancia().rutas().documentos + L"\\Facturas");
            return;
        }
        if (const auto c = repo().clientePorId(clienteId)) abrirCarpeta(carpetaFacturasCliente(*c));
    }

    void anular() {
        const long id = seleccionada();
        if (!id) return;
        const auto f = repo().facturaPorId(id);
        if (!f) return;
        if (f->anulada) {
            mostrarInfo(h_, "Ese comprobante ya está anulado.");
            return;
        }
        std::string aviso = "¿Anular la factura " + f->numeroTexto() + " de " + f->clienteNombre +
                            " por " + formatoLempiras(f->total) + "?";
        if (f->pagado > 0)
            aviso += "\n\nSe registrará en la caja de HOY una entrada de " + formatoLempiras(f->pagado) +
                     " (devolución de lo pagado).";
        if (f->saldo > 0) aviso += "\nTambién se anulará el saldo pendiente de " + formatoLempiras(f->saldo) + ".";
        if (!confirmar(h_, aviso)) return;
        if (!autorizar(h_, "anular un comprobante")) return;
        const auto motivo = pedirTexto(h_, "Anular comprobante", "Escriba el motivo de la anulación:");
        if (!motivo) return;
        if (intentar(h_, [&] { repo().anularFactura(id, *motivo, Fecha::hoy()); })) {
            mostrarInfo(h_, "Comprobante anulado.");
            buscar();
        }
    }

    std::vector<Factura> facturas_;
    std::map<LPARAM, COLORREF> colores_;
    LPARAM carpeta_ = TODOS;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaFacturas() { return std::make_unique<PaginaFacturas>(); }

}  // namespace bu::win
