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
        ponerFecha(h_, IDC_H_DESDE, Fecha(Fecha::hoy().anio, 1, 1));
        ponerFecha(h_, IDC_H_HASTA, Fecha::hoy());
        textoGuia(h_, IDC_H_FILTRO, L"Nombre o DNI");
        lvConfigurar(h_, IDC_H_CLIENTES, {{L"Cliente (carpeta)", 150}, {L"Facturas", 34, true}});
        lvConfigurar(h_, IDC_H_LISTA,
                     {{L"No.", 46},
                      {L"Tipo", 50},
                      {L"Fecha", 50},
                      {L"Cliente", 120},
                      {L"Libras", 56, true},
                      {L"Quintales", 46, true},
                      {L"Total", 66, true},
                      {L"Pagado", 66, true},
                      {L"Saldo", 60, true},
                      {L"Estado", 46}});
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

    void alMostrar() override { buscar(); }

    void alRedimensionar(int ancho, int alto) override {
        estirar(IDC_H_CLIENTES, ancho, alto, false, true);
        estirar(IDC_H_LISTA, ancho, alto, true, true);
    }

    bool alComando(int id, int codigo) override {
        switch (id) {
            case IDC_H_BUSCAR:
                buscar();
                return true;
            case IDC_H_FILTRO:
                if (codigo == EN_CHANGE) mostrarCarpetas();
                return true;
            case IDC_H_HOY:
                ponerFecha(h_, IDC_H_DESDE, Fecha::hoy());
                ponerFecha(h_, IDC_H_HASTA, Fecha::hoy());
                buscar();
                return true;
            case IDC_H_MES:
                ponerFecha(h_, IDC_H_DESDE, Fecha::hoy().primerDiaDelMes());
                ponerFecha(h_, IDC_H_HASTA, Fecha::hoy());
                buscar();
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
    void buscar() {
        facturas_ = repo().facturas(leerFecha(h_, IDC_H_DESDE), leerFecha(h_, IDC_H_HASTA));
        mostrarCarpetas();
    }

    // Una carpeta por cliente (aunque tenga muchos comprobantes).
    void mostrarCarpetas() {
        std::map<long, Carpeta> carpetas;
        for (const auto& f : facturas_) {
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
            if (coincideBusqueda(par.second.nombre + " " + par.second.dni, filtro)) lista.push_back(par.second);
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
                      {f.numeroTexto(), f.categoria == CATEGORIA_UVA ? "UVA" : "PERGAMINO", f.fecha.texto(), f.clienteNombre, formatoDecimal(f.totalLibras, 2),
                       formatoDecimal(aQuintales(f.totalLibras), 2), formatoLempiras(f.total), formatoLempiras(f.pagado),
                       formatoLempiras(f.saldo), f.anulada ? "ANULADO" : "Válido"},
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
