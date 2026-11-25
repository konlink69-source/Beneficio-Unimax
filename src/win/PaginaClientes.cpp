// Pestaña "Clientes": productores y compradores (nombre, DNI, teléfono, dirección).
#include "core/Identificacion.h"
#include "core/Texto.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Excel.h"
#include "win/Paginas.h"
#include "win/Sesion.h"
#include "win/resource.h"

namespace bu::win {

namespace {

class PaginaClientes : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Clientes"; }

    void alIniciar() override {
        controlEstirable_ = IDC_C_LISTA;
        for (int t = CLIENTE_PRODUCTOR; t <= CLIENTE_AMBOS; ++t) comboAgregar(h_, IDC_C_TIPO, nombreTipoCliente(t), t);
        SendDlgItemMessageW(h_, IDC_C_NOMBRE, EM_LIMITTEXT, 150, 0);
        SendDlgItemMessageW(h_, IDC_C_DNI, EM_LIMITTEXT, 20, 0);
        SendDlgItemMessageW(h_, IDC_C_TELEFONO, EM_LIMITTEXT, 30, 0);
        SendDlgItemMessageW(h_, IDC_C_DIRECCION, EM_LIMITTEXT, 255, 0);
        SendDlgItemMessageW(h_, IDC_C_CARNET, EM_LIMITTEXT, 30, 0);
        // Barra de búsqueda grande y visible.
        ponerFuente(h_, IDC_C_BUSCAR_LBL, fuenteEtiqueta());
        colorTexto(IDC_C_BUSCAR_LBL, COLOR_CAFE);
        ponerFuente(h_, IDC_C_BUSCAR, fuente(tamanoLetra() * 12.0 / 9.0, false));
        textoGuia(h_, IDC_C_BUSCAR, L"Escriba nombre, apellido, DNI, RTN o carnet");
        lvConfigurar(h_, IDC_C_LISTA,
                     {{L"Nombre", 140},
                      {L"DNI / RTN", 76},
                      {L"Teléfono", 56},
                      {L"Dirección", 110},
                      {L"Tipo", 70},
                      {L"RTN", 76},
                      {L"Carnet IHCAFE", 60},
                      {L"El beneficio le debe", 74, true},
                      {L"Le debe al beneficio", 74, true}});
        ponerFuente(h_, IDC_C_MODO, fuenteNegrita());
        colorTexto(IDC_C_MODO, COLOR_CAFE);
        nuevo();
    }

    void alMostrar() override { cargar(); }

    bool alComando(int id, int codigo) override {
        switch (id) {
            case IDC_C_NUEVO:
                nuevo();
                return true;
            case IDC_C_GUARDAR:
                guardar();
                return true;
            case IDC_C_ELIMINAR:
                eliminar();
                return true;
            case IDC_C_ESTADO: {
                const long id2 = editId_ ? editId_ : static_cast<long>(lvDatoSeleccionado(h_, IDC_C_LISTA, 0));
                if (!id2)
                    mostrarAviso(h_, "Seleccione un cliente de la lista.");
                else
                    generarEstadoCuenta(h_, id2);
                return true;
            }
            case IDC_C_BUSCAR:
                if (codigo == EN_CHANGE) mostrarLista();
                return true;
            case IDC_C_DNI:
                if (codigo == EN_KILLFOCUS) {
                    const auto v = validarIdentificacion(texto(h_, IDC_C_DNI), Fecha::hoy().anio);
                    if (v.valido) ponerTexto(h_, IDC_C_DNI, v.formateado);
                }
                return true;
            default:
                return false;
        }
    }

    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if (nm->idFrom == IDC_C_LISTA && nm->code == LVN_ITEMCHANGED) {
            const auto* nv = reinterpret_cast<NMLISTVIEW*>(nm);
            if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED)) cargarEnFormulario(static_cast<long>(nv->lParam));
            r = 0;
            return true;
        }
        return false;
    }

    void alAceptar() override {
        if (GetFocus() == GetDlgItem(h_, IDC_C_BUSCAR)) return;
        guardar();
    }

private:
    void cargar() {
        todos_ = repo().clientes();
        deudas_ = repo().resumenDeudasPorCliente();
        mostrarLista();
    }

    void mostrarLista() {
        lvLimpiar(h_, IDC_C_LISTA);
        const std::string filtro = texto(h_, IDC_C_BUSCAR);
        int n = 0;
        for (const auto& c : todos_) {
            if (!coincideBusqueda(c.nombre + " " + c.dni + " " + c.telefono + " " + c.direccion + " " + c.rtn + " " + c.carnet,
                                  filtro))
                continue;
            const auto it = deudas_.find(c.id);
            const ResumenDeudas rd = it == deudas_.end() ? ResumenDeudas() : it->second;
            lvAgregar(h_, IDC_C_LISTA,
                      {c.nombre, c.dni, c.telefono, c.direccion, nombreTipoCliente(c.tipo), c.rtn, c.carnet,
                       rd.porPagar ? formatoLempiras(rd.porPagar) : std::string(),
                       rd.porCobrar ? formatoLempiras(rd.porCobrar) : std::string()},
                      c.id);
            ++n;
        }
        ponerTexto(h_, IDC_C_RESUMEN, std::to_string(n) + " clientes");
    }

    void nuevo() {
        editId_ = 0;
        for (int id : {IDC_C_NOMBRE, IDC_C_DNI, IDC_C_TELEFONO, IDC_C_DIRECCION, IDC_C_RTN, IDC_C_CARNET}) ponerTexto(h_, id, "");
        comboSeleccionar(h_, IDC_C_TIPO, CLIENTE_PRODUCTOR);
        ponerTexto(h_, IDC_C_MODO, "Nuevo cliente: llene los datos y presione Guardar.");
        habilitar(h_, IDC_C_ELIMINAR, false);
        enfocar(h_, IDC_C_NOMBRE);
    }

    void cargarEnFormulario(long id) {
        for (const auto& c : todos_) {
            if (c.id != id) continue;
            editId_ = c.id;
            ponerTexto(h_, IDC_C_NOMBRE, c.nombre);
            ponerTexto(h_, IDC_C_DNI, c.dni);
            ponerTexto(h_, IDC_C_TELEFONO, c.telefono);
            ponerTexto(h_, IDC_C_DIRECCION, c.direccion);
            ponerTexto(h_, IDC_C_RTN, c.rtn);
            ponerTexto(h_, IDC_C_CARNET, c.carnet);
            comboSeleccionar(h_, IDC_C_TIPO, c.tipo);
            ponerTexto(h_, IDC_C_MODO, "Editando: " + c.nombre);
            habilitar(h_, IDC_C_ELIMINAR, true);
            return;
        }
    }

    void guardar() {
        Cliente c;
        c.id = editId_;
        c.nombre = texto(h_, IDC_C_NOMBRE);
        c.dni = texto(h_, IDC_C_DNI);
        c.telefono = texto(h_, IDC_C_TELEFONO);
        c.direccion = texto(h_, IDC_C_DIRECCION);
        c.rtn = texto(h_, IDC_C_RTN);
        c.carnet = texto(h_, IDC_C_CARNET);
        c.tipo = static_cast<int>(comboDato(h_, IDC_C_TIPO, CLIENTE_PRODUCTOR));
        c.activo = true;
        // Cambiar los datos de un cliente ya registrado es un dato delicado.
        if (c.id != 0 && !autorizar(h_, "modificar los datos de un cliente")) return;
        if (!intentar(h_, [&] { repo().guardarCliente(c); })) return;
        cargar();
        cargarEnFormulario(c.id);
        mostrarInfo(h_, "Cliente guardado: " + c.nombre + " (" + c.dni + ").");
    }

    void eliminar() {
        if (!editId_) return;
        if (!confirmar(h_, "¿Eliminar al cliente " + texto(h_, IDC_C_NOMBRE) + "?")) return;
        if (!autorizar(h_, "eliminar un cliente")) return;
        ResultadoEliminar r = ResultadoEliminar::Eliminado;
        if (!intentar(h_, [&] { r = repo().eliminarCliente(editId_); })) return;
        if (r == ResultadoEliminar::Desactivado)
            mostrarInfo(h_, "El cliente tiene comprobantes, ventas o deudas registradas, por eso no se borra: "
                            "se ocultó de la lista. Si vuelve a registrar una operación con su DNI, se reactiva solo.");
        nuevo();
        cargar();
    }

    long editId_ = 0;
    std::vector<Cliente> todos_;
    std::map<long, ResumenDeudas> deudas_;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaClientes() { return std::make_unique<PaginaClientes>(); }

}  // namespace bu::win
