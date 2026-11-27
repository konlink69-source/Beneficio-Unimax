#include "win/Buscador.h"

#include <algorithm>

#include "core/Identificacion.h"
#include "core/Texto.h"
#include "win/App.h"
#include "win/Dialogos.h"
#include "win/Tema.h"
#include "win/resource.h"

namespace bu::win {

namespace {

const wchar_t* const CLASE_LISTA = L"BeneficioUnimaxSugerencias";
constexpr size_t MAX_SUGERENCIAS = 7;

int px(int v) {
    static const int ppp = [] {
        HDC dc = GetDC(nullptr);
        const int d = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(nullptr, dc);
        return d;
    }();
    return MulDiv(v, ppp, 96);
}

int altoFila() { return std::max(px(40), static_cast<int>(tamanoLetra() * 4.6)); }

LRESULT CALLBACK procLista(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* b = reinterpret_cast<BuscadorCliente*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;  // el foco se queda en el buscador
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE:
            if (b) {
                const int i = static_cast<short>(HIWORD(lp)) / altoFila();
                if (i != b->seleccion()) b->seleccionar(i);
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (b) b->elegir(static_cast<short>(HIWORD(lp)) / altoFila());
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT r;
            GetClientRect(h, &r);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, r.right, r.bottom);
            HGDIOBJ a = SelectObject(mem, bmp);
            FillRect(mem, &r, tema::pincel(tema::TARJETA));
            if (b) {
                const int alto = altoFila();
                HFONT fNombre = fuente(tamanoLetra() + 1, true), fDetalle = fuente(std::max(7, tamanoLetra() - 1), false);
                const auto& lista = b->sugerencias();
                for (size_t i = 0; i < lista.size(); ++i) {
                    RECT fila{px(4), static_cast<int>(i) * alto + px(2), r.right - px(4), static_cast<int>(i + 1) * alto - px(2)};
                    if (static_cast<int>(i) == b->seleccion())
                        tema::rectRedondeado(mem, fila, px(6), tema::SELECCION, tema::SELECCION);
                    const auto& s = lista[i];
                    const int mitad = (fila.top + fila.bottom) / 2;
                    if (s.id == 0) {
                        RECT rt{fila.left + px(12), fila.top, fila.right - px(8), fila.bottom};
                        tema::texto(mem, s.nombre, rt, fNombre, tema::CAFE, DT_LEFT | DT_VCENTER);
                        continue;
                    }
                    RECT r1{fila.left + px(12), fila.top + px(2), fila.right - px(8), mitad + px(2)};
                    tema::texto(mem, s.nombre, r1, fNombre, tema::TEXTO, DT_LEFT | DT_BOTTOM);
                    RECT r2{fila.left + px(12), mitad + px(2), fila.right - px(8), fila.bottom - px(2)};
                    tema::texto(mem, s.detalle, r2, fDetalle, tema::TEXTO_SUAVE, DT_LEFT | DT_TOP);
                    if (i + 1 < lista.size() && static_cast<int>(i) != b->seleccion() &&
                        static_cast<int>(i + 1) != b->seleccion()) {
                        RECT sep{fila.left + px(12), fila.bottom + px(1), fila.right - px(12), fila.bottom + px(2)};
                        FillRect(mem, &sep, tema::pincel(RGB(242, 236, 228)));
                    }
                }
            }
            // Borde fino.
            HBRUSH borde = tema::pincel(tema::BORDE);
            FrameRect(mem, &r, borde);
            BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, a);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(h, msg, wp, lp);
    }
}

void registrarClase() {
    static bool hecho = false;
    if (hecho) return;
    hecho = true;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = procLista;
    wc.hInstance = App::instancia().hInst;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32649));  // mano
    wc.lpszClassName = CLASE_LISTA;
    RegisterClassExW(&wc);
}

// Teclas del buscador: flechas para moverse en la lista, Enter para elegir, Esc para cerrarla.
LRESULT CALLBACK procBusca(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) {
    auto* b = reinterpret_cast<BuscadorCliente*>(ref);
    switch (msg) {
        case WM_GETDLGCODE:
            if (b->listaVisible() && lp) {
                const auto* m = reinterpret_cast<const MSG*>(lp);
                if (m->message == WM_KEYDOWN && (m->wParam == VK_RETURN || m->wParam == VK_ESCAPE))
                    return DLGC_WANTALLKEYS;
            }
            break;
        case WM_KEYDOWN:
            if (b->listaVisible()) {
                if (wp == VK_DOWN) {
                    b->mover(1);
                    return 0;
                }
                if (wp == VK_UP) {
                    b->mover(-1);
                    return 0;
                }
                if (wp == VK_RETURN) {
                    b->elegir(b->seleccion());
                    return 0;
                }
                if (wp == VK_ESCAPE) {
                    b->ocultar();
                    return 0;
                }
            }
            break;
        case WM_CHAR:
            if (wp == VK_RETURN || wp == VK_ESCAPE) return 0;  // sin pitido
            break;
        case WM_KILLFOCUS:
            b->ocultar();
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(h, procBusca, 10);
            break;
        default:
            break;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

std::string detalleCliente(const Cliente& c) {
    std::string d = "DNI " + c.dni;
    if (!c.telefono.empty()) d += "  ·  Tel. " + c.telefono;
    if (!c.rtn.empty()) d += "  ·  RTN " + c.rtn;
    if (!c.carnet.empty()) d += "  ·  Carnet " + c.carnet;
    return d;
}

}  // namespace

void BuscadorCliente::configurar(HWND dlg, int idBusca, int idNombre, int idDatos, int idDeuda, int idCambiar,
                                 Colorear colorear, std::function<void()> alElegir, int tipoNuevo) {
    dlg_ = dlg;
    tipoNuevo_ = tipoNuevo;
    idBusca_ = idBusca;
    idNombre_ = idNombre;
    idDatos_ = idDatos;
    idDeuda_ = idDeuda;
    idCambiar_ = idCambiar;
    colorear_ = std::move(colorear);
    alElegir_ = std::move(alElegir);
    registrarClase();
    HWND busca = GetDlgItem(dlg, idBusca);
    SetWindowSubclass(busca, procBusca, 10, reinterpret_cast<DWORD_PTR>(this));
    textoGuia(dlg, idBusca, L"Buscar cliente por nombre o DNI...");
    ponerFuente(dlg, idBusca, fuente(tamanoLetra() * 11.0 / 9.0, false));
    ponerFuente(dlg, idNombre, fuente(tamanoLetra() * 1.45, true));
    tema::campoBusqueda(busca);
    limpiar();
}

bool BuscadorCliente::tieneFoco() const { return GetFocus() == GetDlgItem(dlg_, idBusca_); }

bool BuscadorCliente::listaVisible() const { return lista_ && IsWindowVisible(lista_); }

void BuscadorCliente::recargar() {
    clientes_.clear();
    for (const auto& c : repo().clientes())
        if (c.activo) clientes_.push_back(c);
}

bool BuscadorCliente::comando(int id, int codigo) {
    if (id == idBusca_) {
        if (codigo == EN_CHANGE && !poniendo_) filtrar();
        return true;
    }
    if (id == idCambiar_) {
        limpiar();
        return true;
    }
    return false;
}

void BuscadorCliente::filtrar() {
    const std::string t = recortar(texto(dlg_, idBusca_));
    sugerencias_.clear();
    if (t.empty()) {
        ocultar();
        return;
    }
    if (clientes_.empty()) recargar();
    // Primero los que empiezan con lo escrito, luego los que lo contienen.
    std::vector<const Cliente*> primeros, otros;
    const std::string n = normalizarBusqueda(t);
    for (const auto& c : clientes_) {
        if (!coincideBusqueda(c.nombre + " " + c.dni + " " + c.telefono + " " + c.rtn, t)) continue;
        const std::string digitos = soloDigitos(t);
        if (normalizarBusqueda(c.nombre).rfind(n, 0) == 0 || (!digitos.empty() && soloDigitos(c.dni).rfind(digitos, 0) == 0))
            primeros.push_back(&c);
        else
            otros.push_back(&c);
    }
    for (const auto* lista : {&primeros, &otros})
        for (const Cliente* c : *lista) {
            if (sugerencias_.size() >= MAX_SUGERENCIAS) break;
            sugerencias_.push_back({c->id, c->nombre, detalleCliente(*c)});
        }
    sugerencias_.push_back({0, "+  Registrar cliente nuevo: \"" + t + "\"", ""});
    seleccion_ = 0;
    mostrarLista();
}

void BuscadorCliente::mostrarLista() {
    HWND busca = GetDlgItem(dlg_, idBusca_);
    RECT r;
    GetWindowRect(busca, &r);
    const int ancho = std::max(static_cast<int>(r.right - r.left), px(420));
    const int alto = altoFila() * static_cast<int>(sugerencias_.size()) + px(4);
    if (!lista_) {
        lista_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, CLASE_LISTA, L"", WS_POPUP, 0, 0, 10, 10,
                                 GetAncestor(dlg_, GA_ROOT), nullptr, App::instancia().hInst, nullptr);
        SetWindowLongPtrW(lista_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    SetWindowPos(lista_, HWND_TOP, r.left, r.bottom + px(4), ancho, alto, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(lista_, nullptr, FALSE);
}

void BuscadorCliente::ocultar() {
    if (lista_) ShowWindow(lista_, SW_HIDE);
}

void BuscadorCliente::seleccionar(int i) {
    if (i < 0 || i >= static_cast<int>(sugerencias_.size())) return;
    seleccion_ = i;
    if (lista_) InvalidateRect(lista_, nullptr, FALSE);
}

void BuscadorCliente::mover(int delta) {
    if (sugerencias_.empty()) return;
    const int n = static_cast<int>(sugerencias_.size());
    seleccionar((seleccion_ + delta + n) % n);
}

void BuscadorCliente::alPresionarEnter() {
    if (listaVisible()) {
        elegir(seleccion_);
        return;
    }
    filtrar();
    // Un solo cliente encontrado (más la opción de registrar): se elige directo.
    if (sugerencias_.size() == 2) elegir(0);
}

void BuscadorCliente::elegir(int indice) {
    if (indice < 0 || indice >= static_cast<int>(sugerencias_.size())) return;
    const Sugerencia s = sugerencias_[static_cast<size_t>(indice)];
    ocultar();
    if (s.id == 0) {
        const auto nuevo = dialogoClienteNuevo(dlg_, recortar(texto(dlg_, idBusca_)), tipoNuevo_);
        if (!nuevo) {
            enfocar(dlg_, idBusca_);
            return;
        }
        recargar();
        poner(*nuevo);
    } else {
        const auto it = std::find_if(clientes_.begin(), clientes_.end(), [&](const Cliente& c) { return c.id == s.id; });
        if (it == clientes_.end()) return;
        poner(*it);
    }
    if (alElegir_) alElegir_();
}

void BuscadorCliente::poner(const Cliente& c) {
    cliente_ = c;
    poniendo_ = true;
    ponerTexto(dlg_, idBusca_, "");
    poniendo_ = false;
    mostrarDatos();
}

void BuscadorCliente::limpiar() {
    cliente_.reset();
    poniendo_ = true;
    ponerTexto(dlg_, idBusca_, "");
    poniendo_ = false;
    ocultar();
    mostrarDatos();
}

void BuscadorCliente::mostrarDatos() {
    if (!cliente_) {
        ponerTexto(dlg_, idNombre_, "Ningún cliente elegido");
        ponerTexto(dlg_, idDatos_, "Escriba el nombre o el DNI en el buscador y elija el cliente de la lista.");
        ponerTexto(dlg_, idDeuda_, "");
        if (colorear_) {
            colorear_(idNombre_, RGB(170, 158, 148));
            colorear_(idDatos_, tema::TEXTO_SUAVE);
        }
        ShowWindow(GetDlgItem(dlg_, idCambiar_), SW_HIDE);
        tema::refrescar(dlg_);
        return;
    }
    const Cliente& c = *cliente_;
    ponerTexto(dlg_, idNombre_, c.nombre);
    ponerTexto(dlg_, idDatos_, detalleCliente(c));
    std::string deuda = "Sin deudas";
    COLORREF color = tema::TEXTO_SUAVE;
    try {
        const ResumenDeudas d = repo().resumenDeudas(c.id);
        if (d.porPagar > 0 && d.porCobrar > 0) {
            deuda = "Le debemos " + formatoLempiras(d.porPagar) + " · Nos debe " + formatoLempiras(d.porCobrar);
            color = tema::AMBAR;
        } else if (d.porPagar > 0) {
            deuda = "Le debemos " + formatoLempiras(d.porPagar);
            color = tema::ROJO;
        } else if (d.porCobrar > 0) {
            deuda = "Nos debe " + formatoLempiras(d.porCobrar);
            color = tema::VERDE;
        }
    } catch (...) {
    }
    ponerTexto(dlg_, idDeuda_, deuda);
    if (colorear_) {
        colorear_(idNombre_, tema::CAFE);
        colorear_(idDatos_, tema::TEXTO_SUAVE);
        colorear_(idDeuda_, color);
    }
    ShowWindow(GetDlgItem(dlg_, idCambiar_), SW_SHOW);
    tema::refrescar(dlg_);
}

// ================================================================ Cliente nuevo
namespace {

class DialogoClienteNuevo : public DialogoModal {
public:
    DialogoClienteNuevo(std::string texto, int tipo) : texto_(std::move(texto)), tipo_(tipo) {}
    std::optional<Cliente> resultado;

protected:
    void alIniciar() override {
        // Lo escrito en el buscador: si son números es el DNI; si no, el nombre.
        if (!texto_.empty() && !tieneLetras(texto_)) {
            ponerTexto(h_, IDC_CN_DNI, texto_);
            enfocar(h_, IDC_CN_NOMBRE);
        } else {
            ponerTexto(h_, IDC_CN_NOMBRE, texto_);
            enfocar(h_, IDC_CN_DNI);
        }
        textoGuia(h_, IDC_CN_DNI, L"0801-1990-12345");
        textoGuia(h_, IDC_CN_TEL, L"9999-0000");
        if (tipo_ == CLIENTE_COMPRADOR) SetWindowTextW(h_, L"Registrar comprador nuevo");
        ponerTexto(h_, IDC_CN_INFO, tipo_ == CLIENTE_COMPRADOR
                                        ? "Queda registrado como comprador y se elige para esta venta. Puede usar DNI o RTN."
                                        : "Queda registrado como productor y se elige para este documento.");
    }

    bool alAceptar() override {
        Cliente c;
        c.nombre = recortar(texto(h_, IDC_CN_NOMBRE));
        c.dni = recortar(texto(h_, IDC_CN_DNI));
        c.telefono = recortar(texto(h_, IDC_CN_TEL));
        c.rtn = recortar(texto(h_, IDC_CN_RTN));
        c.carnet = recortar(texto(h_, IDC_CN_CARNET));
        c.direccion = recortar(texto(h_, IDC_CN_DIR));
        c.tipo = tipo_;
        if (c.nombre.empty()) {
            mostrarAviso(h_, "Escriba el nombre completo del cliente.");
            enfocar(h_, IDC_CN_NOMBRE);
            return false;
        }
        if (!intentar(h_, [&] { repo().guardarCliente(c); })) return false;
        resultado = c;
        return true;
    }

private:
    std::string texto_;
    int tipo_ = CLIENTE_PRODUCTOR;
};

}  // namespace

std::optional<Cliente> dialogoClienteNuevo(HWND padre, const std::string& texto, int tipo) {
    DialogoClienteNuevo d(texto, tipo);
    if (d.mostrar(padre, IDD_CLIENTE_NUEVO) != IDOK) return std::nullopt;
    return d.resultado;
}

}  // namespace bu::win
