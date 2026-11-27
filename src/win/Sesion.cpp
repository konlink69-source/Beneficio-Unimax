#include "win/Sesion.h"

#include <algorithm>

#include "core/Seguridad.h"
#include "core/Texto.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Tema.h"
#include "win/resource.h"

namespace bu::win {

namespace {

constexpr COLORREF CREMA = RGB(241, 234, 219);

HBRUSH brochaCrema() {
    static HBRUSH b = CreateSolidBrush(CREMA);
    return b;
}

// Logo del encabezado (fondo crema) escalado al tamaño de un control STATIC.
void ponerLogo(HWND dlg, int id) {
    HWND c = GetDlgItem(dlg, id);
    RECT r;
    GetClientRect(c, &r);
    const ImagenDib img = imagenRecurso(IDB_LOGO);
    if (!img.info || r.right <= 0 || r.bottom <= 0) return;
    const double factor = std::min(static_cast<double>(r.right) / img.ancho, static_cast<double>(r.bottom) / img.alto);
    const int w = static_cast<int>(img.ancho * factor), h = static_cast<int>(img.alto * factor);
    HDC pantalla = GetDC(nullptr);
    HBITMAP bmp = CreateCompatibleBitmap(pantalla, r.right, r.bottom);
    HDC mem = CreateCompatibleDC(pantalla);
    HGDIOBJ anterior = SelectObject(mem, bmp);
    FillRect(mem, &r, brochaCrema());
    SetStretchBltMode(mem, HALFTONE);
    SetBrushOrgEx(mem, 0, 0, nullptr);
    StretchDIBits(mem, (r.right - w) / 2, (r.bottom - h) / 2, w, h, 0, 0, img.ancho, img.alto, img.bits, img.info,
                  DIB_RGB_COLORS, SRCCOPY);
    SelectObject(mem, anterior);
    DeleteDC(mem);
    ReleaseDC(nullptr, pantalla);
    HBITMAP viejo = reinterpret_cast<HBITMAP>(SendMessageW(c, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(bmp)));
    if (viejo) DeleteObject(viejo);
}

void mostrarControl(HWND dlg, int id, bool si) { ShowWindow(GetDlgItem(dlg, id), si ? SW_SHOW : SW_HIDE); }

// ================================================================ Código de recuperación
class DlgCodigo : public DialogoModal {
public:
    explicit DlgCodigo(std::string codigo) : codigo_(std::move(codigo)) {}

protected:
    void alIniciar() override {
        ponerTexto(h_, IDC_C2_CODIGO, codigo_);
        ponerFuente(h_, IDC_C2_CODIGO, fuente(tamanoLetra() * 2.0, true));
        enfocar(h_, IDOK);
    }

private:
    std::string codigo_;
};

void crearYMostrarCodigoRecuperacion(HWND padre) {
    const std::string codigo = codigoRecuperacion(bytesAleatorios(12));
    repo().guardarCodigoRecuperacion(codigo, bytesAleatorios(16));
    DlgCodigo d(codigo);
    d.mostrar(padre, IDD_CODIGO);
}

// ================================================================ Cambiar contraseña
class DlgCambiarClave : public DialogoModal {
public:
    DlgCambiarClave(Usuario u, bool pedirActual, bool obligatorio, bool pedirCambioAlIngresar = false)
        : u_(std::move(u)), pedirActual_(pedirActual), obligatorio_(obligatorio), pedirCambio_(pedirCambioAlIngresar) {}

protected:
    void alIniciar() override {
        std::string t = "Usuario: " + u_.usuario + (u_.nombre.empty() ? std::string() : " (" + u_.nombre + ")");
        if (obligatorio_ && u_.usuario == USUARIO_MAESTRO)
            t += "\nPor seguridad, cambie la contraseña inicial por una contraseña maestra propia.";
        else if (obligatorio_)
            t += "\nDebe escribir una contraseña nueva para continuar.";
        else if (pedirCambio_)
            t += "\nEsta contraseña es temporal: el usuario deberá cambiarla al ingresar.";
        t += "\nMínimo 6 caracteres, con letras y números.";
        ponerTexto(h_, IDC_K2_INFO, t);
        mostrarControl(h_, IDC_K2_ACTUAL, pedirActual_);
        mostrarControl(h_, IDC_K2_ACTUAL_LBL, pedirActual_);
        enfocar(h_, pedirActual_ ? IDC_K2_ACTUAL : IDC_K2_NUEVA);
    }
    bool alAceptar() override {
        if (pedirActual_ && !repo().autenticar(u_.usuario, texto(h_, IDC_K2_ACTUAL))) {
            mostrarAviso(h_, "La contraseña actual no es correcta.");
            enfocar(h_, IDC_K2_ACTUAL);
            return false;
        }
        const std::string nueva = texto(h_, IDC_K2_NUEVA);
        if (nueva != texto(h_, IDC_K2_REPETIR)) {
            mostrarAviso(h_, "Las dos contraseñas nuevas no son iguales.");
            enfocar(h_, IDC_K2_REPETIR);
            return false;
        }
        return intentar(h_, [&] { repo().cambiarContrasena(u_.id, nueva, bytesAleatorios(16), pedirCambio_); });
    }

private:
    Usuario u_;
    bool pedirActual_, obligatorio_, pedirCambio_;
};

// ================================================================ Ingreso
class DlgIngreso : public DialogoModal {
public:
    Usuario usuario;

protected:
    void alIniciar() override {
        tema::usarFondo(h_, CREMA);
        ponerLogo(h_, IDC_L_LOGO);
        ponerFuente(h_, IDC_L_USUARIO, fuente(tamanoLetra() * 11.0 / 9.0, false));
        ponerFuente(h_, IDC_L_CLAVE, fuente(tamanoLetra() * 11.0 / 9.0, false));
        ponerFuente(h_, IDC_L_TITULO, fuenteGrande());
        // Mientras la contraseña inicial no se haya cambiado, se muestra cómo entrar la primera vez.
        bool primerIngreso = false;
        try {
            const auto lista = repo().usuarios();
            primerIngreso = lista.size() == 1 && lista[0].usuario == USUARIO_MAESTRO && lista[0].debeCambiar;
        } catch (...) {
        }
        if (primerIngreso) {
            ponerTexto(h_, IDC_L_AYUDA, std::string("Primer ingreso: usuario \"") + USUARIO_MAESTRO + "\", contraseña \"" +
                                            CONTRASENA_INICIAL + "\". Después se le pedirá una contraseña nueva.");
            ponerTexto(h_, IDC_L_USUARIO, USUARIO_MAESTRO);
            enfocar(h_, IDC_L_CLAVE);
        } else {
            enfocar(h_, IDC_L_USUARIO);
        }
    }
    INT_PTR alColor(UINT msg, HDC dc, HWND control) override {
        if (msg == WM_CTLCOLORDLG) return reinterpret_cast<INT_PTR>(brochaCrema());
        if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORBTN) {
            SetBkColor(dc, CREMA);
            const int id = GetDlgCtrlID(control);
            SetTextColor(dc, id == IDC_L_INFO ? RGB(192, 0, 0) : (id == IDC_L_AYUDA ? RGB(110, 110, 110) : COLOR_CAFE));
            return reinterpret_cast<INT_PTR>(brochaCrema());
        }
        return 0;
    }
    bool alComando(int id, int) override {
        if (id == IDC_L_OLVIDO) {
            recuperar();
            return true;
        }
        return false;
    }
    bool alAceptar() override {
        const std::string u = recortar(texto(h_, IDC_L_USUARIO));
        const std::string clave = texto(h_, IDC_L_CLAVE);
        if (u.empty() || clave.empty()) {
            ponerTexto(h_, IDC_L_INFO, "Escriba su usuario y su contraseña.");
            return false;
        }
        std::optional<Usuario> r;
        {
            CursorEspera espera;
            intentar(h_, [&] { r = repo().autenticar(u, clave); });
        }
        if (!r) {
            ++intentos_;
            ponerTexto(h_, IDC_L_INFO, "Usuario o contraseña incorrectos.");
            ponerTexto(h_, IDC_L_CLAVE, "");
            enfocar(h_, IDC_L_CLAVE);
            if (intentos_ >= 5) Sleep(static_cast<DWORD>(std::min(intentos_ - 4, 10)) * 2000);  // frena los intentos
            return false;
        }
        usuario = *r;
        return true;
    }

private:
    void recuperar() {
        if (repo().config("CodigoRecuperacion").empty()) {
            mostrarAviso(h_, "No hay un código de recuperación guardado. Pida a otro administrador que le asigne una "
                             "contraseña nueva en Configuración > Usuarios.");
            return;
        }
        const auto codigo = pedirTexto(h_, "Recuperar acceso",
                                       "Escriba el código de recuperación que anotó al crear la contraseña maestra:");
        if (!codigo) return;
        if (!repo().verificarCodigoRecuperacion(*codigo)) {
            mostrarAviso(h_, "El código de recuperación no es correcto.");
            return;
        }
        // Se restablece el administrador escrito en "Usuario" o, si no, el usuario maestro.
        const std::string escrito = recortar(texto(h_, IDC_L_USUARIO));
        std::optional<Usuario> admin;
        for (const auto& u : repo().usuarios()) {
            if (!u.esAdministrador()) continue;
            if (normalizarBusqueda(u.usuario) == normalizarBusqueda(escrito)) admin = u;
            if (!admin && u.usuario == USUARIO_MAESTRO) admin = u;
        }
        if (!admin) {
            for (const auto& u : repo().usuarios())
                if (u.esAdministrador()) admin = u;
        }
        if (!admin) return;
        // Se reactiva por si estaba desactivado.
        admin->activo = true;
        intentar(h_, [&] { repo().guardarUsuario(*admin); });
        DlgCambiarClave d(*admin, false, false);
        if (d.mostrar(h_, IDD_CAMBIAR_CLAVE) != IDOK) return;
        mostrarInfo(h_, "Contraseña cambiada. Ahora se generará un código de recuperación nuevo (el anterior ya no sirve).");
        crearYMostrarCodigoRecuperacion(h_);
        ponerTexto(h_, IDC_L_USUARIO, admin->usuario);
        ponerTexto(h_, IDC_L_INFO, "");
        enfocar(h_, IDC_L_CLAVE);
    }
    int intentos_ = 0;
};

// ================================================================ Autorización
class DlgAutorizar : public DialogoModal {
public:
    explicit DlgAutorizar(std::string accion) : accion_(std::move(accion)) {}
    Usuario autorizador;

protected:
    void alIniciar() override {
        ponerTexto(h_, IDC_A2_INFO, "Para " + accion_ + " se necesita la contraseña de un administrador.");
        const Usuario& actual = App::instancia().usuario();
        if (actual.esAdministrador()) {
            ponerTexto(h_, IDC_A2_USUARIO, actual.usuario);
            enfocar(h_, IDC_A2_CLAVE);
        } else {
            enfocar(h_, IDC_A2_USUARIO);
        }
    }
    bool alAceptar() override {
        std::optional<Usuario> r;
        {
            CursorEspera espera;
            r = repo().autenticar(texto(h_, IDC_A2_USUARIO), texto(h_, IDC_A2_CLAVE));
        }
        if (!r || !r->esAdministrador()) {
            mostrarAviso(h_, "Usuario o contraseña incorrectos, o el usuario no es administrador.");
            ponerTexto(h_, IDC_A2_CLAVE, "");
            enfocar(h_, IDC_A2_CLAVE);
            return false;
        }
        autorizador = *r;
        return true;
    }

private:
    std::string accion_;
};

// ================================================================ Usuarios
class DlgUsuario : public DialogoModal {
public:
    explicit DlgUsuario(std::optional<Usuario> u) : u_(std::move(u)) {}

protected:
    void alIniciar() override {
        comboAgregar(h_, IDC_U2_ROL, "Operador (registra compras, ventas, caja)", ROL_OPERADOR);
        comboAgregar(h_, IDC_U2_ROL, "Administrador (además autoriza y configura)", ROL_ADMINISTRADOR);
        if (u_) {
            SetWindowTextW(h_, L"Editar usuario");
            ponerTexto(h_, IDC_U2_USUARIO, u_->usuario);
            soloLectura(h_, IDC_U2_USUARIO, true);
            ponerTexto(h_, IDC_U2_NOMBRE, u_->nombre);
            comboSeleccionar(h_, IDC_U2_ROL, u_->rol);
            CheckDlgButton(h_, IDC_U2_ACTIVO, u_->activo ? BST_CHECKED : BST_UNCHECKED);
            for (int id : {IDC_U2_CLAVE, IDC_U2_REPETIR, IDC_U2_CLAVE_LBL, IDC_U2_REPETIR_LBL, IDC_U2_CAMBIAR})
                mostrarControl(h_, id, false);
            enfocar(h_, IDC_U2_NOMBRE);
        } else {
            comboSeleccionar(h_, IDC_U2_ROL, ROL_OPERADOR);
            CheckDlgButton(h_, IDC_U2_ACTIVO, BST_CHECKED);
            CheckDlgButton(h_, IDC_U2_CAMBIAR, BST_CHECKED);
            enfocar(h_, IDC_U2_USUARIO);
        }
    }
    bool alAceptar() override {
        return intentar(h_, [&] {
            Usuario u = u_ ? *u_ : Usuario();
            u.nombre = texto(h_, IDC_U2_NOMBRE);
            u.rol = static_cast<int>(comboDato(h_, IDC_U2_ROL, ROL_OPERADOR));
            u.activo = IsDlgButtonChecked(h_, IDC_U2_ACTIVO) == BST_CHECKED;
            if (u_) {
                repo().guardarUsuario(u);
                return;
            }
            u.usuario = texto(h_, IDC_U2_USUARIO);
            u.debeCambiar = IsDlgButtonChecked(h_, IDC_U2_CAMBIAR) == BST_CHECKED;
            const std::string clave = texto(h_, IDC_U2_CLAVE);
            if (clave != texto(h_, IDC_U2_REPETIR)) throw ErrorNegocio("Las dos contraseñas no son iguales.");
            repo().crearUsuario(u, clave, bytesAleatorios(16));
        });
    }

private:
    std::optional<Usuario> u_;
};

class DlgUsuarios : public DialogoModal {
protected:
    void alIniciar() override {
        lvConfigurar(h_, IDC_U_LISTA, {{L"Usuario", 70}, {L"Nombre", 120}, {L"Rol", 70}, {L"Estado", 110}});
        cargar();
    }
    bool alComando(int id, int) override {
        switch (id) {
            case IDC_U_NUEVO: {
                DlgUsuario d(std::nullopt);
                if (d.mostrar(h_, IDD_USUARIO) == IDOK) cargar();
                return true;
            }
            case IDC_U_EDITAR:
                if (auto u = seleccionado()) {
                    DlgUsuario d(*u);
                    if (d.mostrar(h_, IDD_USUARIO) == IDOK) cargar();
                }
                return true;
            case IDC_U_CLAVE:
                if (auto u = seleccionado()) {
                    DlgCambiarClave d(*u, false, false, u->id != App::instancia().usuario().id);
                    if (d.mostrar(h_, IDD_CAMBIAR_CLAVE) == IDOK) {
                        mostrarInfo(h_, "Contraseña cambiada para \"" + u->usuario + "\".");
                        cargar();
                    }
                }
                return true;
            case IDC_U_CODIGO:
                if (confirmar(h_, "¿Generar un código de recuperación nuevo? El anterior dejará de servir."))
                    intentar(h_, [&] { crearYMostrarCodigoRecuperacion(h_); });
                return true;
            default:
                return false;
        }
    }
    bool alNotificar(NMHDR* nm, LRESULT& r) override {
        if (nm->idFrom == IDC_U_LISTA && nm->code == NM_DBLCLK) {
            alComando(IDC_U_EDITAR, 0);
            r = 0;
            return true;
        }
        return false;
    }

private:
    void cargar() {
        intentar(h_, [&] {
            lista_ = repo().usuarios();
            lvLimpiar(h_, IDC_U_LISTA);
            for (const auto& u : lista_)
                lvAgregar(h_, IDC_U_LISTA,
                          {u.usuario, u.nombre, nombreRol(u.rol),
                           !u.activo ? "Desactivado" : (u.debeCambiar ? "Debe cambiar contraseña" : "Activo")},
                          u.id);
        });
    }
    std::optional<Usuario> seleccionado() {
        const long id = static_cast<long>(lvDatoSeleccionado(h_, IDC_U_LISTA, 0));
        for (const auto& u : lista_)
            if (u.id == id) return u;
        mostrarAviso(h_, "Seleccione un usuario de la lista.");
        return std::nullopt;
    }
    std::vector<Usuario> lista_;
};

}  // namespace

// ================================================================ Funciones públicas
bool iniciarSesion(HWND padre) {
    DlgIngreso d;
    if (d.mostrar(padre, IDD_INGRESO) != IDOK) return false;
    Usuario u = d.usuario;
    if (u.debeCambiar) {
        DlgCambiarClave c(u, false, true);
        if (c.mostrar(padre, IDD_CAMBIAR_CLAVE) != IDOK) return false;
        u.debeCambiar = false;
        // La primera vez que se cambia la contraseña maestra se entrega el código de recuperación.
        if (u.esAdministrador() && repo().config("CodigoRecuperacion").empty())
            intentar(padre, [&] { crearYMostrarCodigoRecuperacion(padre); });
    }
    App::instancia().establecerUsuario(u);
    return true;
}

bool autorizar(HWND padre, const std::string& accion) {
    // Un administrador que ya confirmó su contraseña no la vuelve a escribir durante 5 minutos.
    static ULONGLONG ultima = 0;
    static long autorizadoId = 0;
    const Usuario& actual = App::instancia().usuario();
    if (actual.esAdministrador() && autorizadoId == actual.id && GetTickCount64() - ultima < 5 * 60 * 1000) return true;
    DlgAutorizar d(accion);
    if (d.mostrar(padre, IDD_AUTORIZAR) != IDOK) return false;
    if (d.autorizador.id == actual.id) {
        autorizadoId = actual.id;
        ultima = GetTickCount64();
    }
    return true;
}

void dialogoCambiarMiContrasena(HWND padre) {
    DlgCambiarClave d(App::instancia().usuario(), true, false);
    if (d.mostrar(padre, IDD_CAMBIAR_CLAVE) == IDOK) mostrarInfo(padre, "Su contraseña se cambió.");
}

void dialogoUsuarios(HWND padre) {
    if (!App::instancia().usuario().esAdministrador() && !autorizar(padre, "administrar los usuarios")) return;
    DlgUsuarios d;
    d.mostrar(padre, IDD_USUARIOS);
}

}  // namespace bu::win
