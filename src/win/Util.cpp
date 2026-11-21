#include "win/Util.h"

#include <shellapi.h>
#include <shlobj.h>

#include "db/Odbc.h"
#include "db/Repositorio.h"

namespace bu::win {

// ================================================================ Texto
std::wstring W(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring r(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &r[0], n);
    return r;
}

std::string U(const std::wstring& s) {
    if (s.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string r(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &r[0], n, nullptr, nullptr);
    return r;
}

std::string formatoPrecio(double precio) {
    if (redondear(precio, 2) == redondear(precio, 4)) return formatoDecimal(precio, 2);
    std::string s = formatoDecimal(precio, 4);
    while (!s.empty() && s.back() == '0') s.pop_back();
    return s;
}

std::string texto(HWND dlg, int id) {
    HWND c = GetDlgItem(dlg, id);
    const int n = GetWindowTextLengthW(c);
    std::wstring w(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(c, &w[0], n + 1);
    w.resize(static_cast<size_t>(n));
    return U(w);
}

void ponerTexto(HWND dlg, int id, const std::string& s) { SetDlgItemTextW(dlg, id, W(s).c_str()); }

std::optional<Centavos> leerMonto(HWND dlg, int id) { return parsearMonto(texto(dlg, id)); }

std::optional<double> leerDecimal(HWND dlg, int id) { return parsearDecimal(texto(dlg, id)); }

bool vacio(HWND dlg, int id) {
    for (char ch : texto(dlg, id))
        if (ch != ' ' && ch != '\t') return false;
    return true;
}

void habilitar(HWND dlg, int id, bool si) { EnableWindow(GetDlgItem(dlg, id), si ? TRUE : FALSE); }

void soloLectura(HWND dlg, int id, bool si) { SendDlgItemMessageW(dlg, id, EM_SETREADONLY, si ? TRUE : FALSE, 0); }

void enfocar(HWND dlg, int id) { SendMessageW(dlg, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(GetDlgItem(dlg, id)), TRUE); }

// ================================================================ Fechas
void configurarFecha(HWND dlg, int id) { DateTime_SetFormat(GetDlgItem(dlg, id), L"dd/MM/yyyy"); }

Fecha leerFecha(HWND dlg, int id) {
    SYSTEMTIME st{};
    if (DateTime_GetSystemtime(GetDlgItem(dlg, id), &st) != GDT_VALID) return Fecha::hoy();
    return Fecha(st.wYear, st.wMonth, st.wDay);
}

std::optional<Fecha> leerFechaOpc(HWND dlg, int id) {
    SYSTEMTIME st{};
    if (DateTime_GetSystemtime(GetDlgItem(dlg, id), &st) != GDT_VALID) return std::nullopt;
    return Fecha(st.wYear, st.wMonth, st.wDay);
}

void ponerFecha(HWND dlg, int id, const Fecha& f) {
    SYSTEMTIME st{};
    st.wYear = static_cast<WORD>(f.anio);
    st.wMonth = static_cast<WORD>(f.mes);
    st.wDay = static_cast<WORD>(f.dia);
    DateTime_SetSystemtime(GetDlgItem(dlg, id), GDT_VALID, &st);
}

void ponerFechaOpc(HWND dlg, int id, const std::optional<Fecha>& f) {
    if (f) {
        ponerFecha(dlg, id, *f);
    } else {
        // Primero se pone una fecha válida para que, al marcar la casilla, aparezca hoy.
        ponerFecha(dlg, id, Fecha::hoy());
        DateTime_SetSystemtime(GetDlgItem(dlg, id), GDT_NONE, nullptr);
    }
}

// ================================================================ Listas desplegables
void comboAgregar(HWND dlg, int id, const std::string& t, LPARAM dato) {
    const LRESULT i = SendDlgItemMessageW(dlg, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(W(t).c_str()));
    SendDlgItemMessageW(dlg, id, CB_SETITEMDATA, static_cast<WPARAM>(i), dato);
}

LPARAM comboDato(HWND dlg, int id, LPARAM porDefecto) {
    const LRESULT i = SendDlgItemMessageW(dlg, id, CB_GETCURSEL, 0, 0);
    if (i == CB_ERR) return porDefecto;
    return static_cast<LPARAM>(SendDlgItemMessageW(dlg, id, CB_GETITEMDATA, static_cast<WPARAM>(i), 0));
}

void comboSeleccionar(HWND dlg, int id, LPARAM dato) {
    const LRESULT n = SendDlgItemMessageW(dlg, id, CB_GETCOUNT, 0, 0);
    for (LRESULT i = 0; i < n; ++i) {
        if (static_cast<LPARAM>(SendDlgItemMessageW(dlg, id, CB_GETITEMDATA, static_cast<WPARAM>(i), 0)) == dato) {
            SendDlgItemMessageW(dlg, id, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
            return;
        }
    }
}

// ================================================================ ListView
void lvConfigurar(HWND dlg, int id, std::initializer_list<Columna> columnas) {
    HWND lv = GetDlgItem(dlg, id);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    int i = 0;
    for (const auto& c : columnas) {
        RECT r{0, 0, c.ancho, 0};
        MapDialogRect(dlg, &r);
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.fmt = c.derecha ? LVCFMT_RIGHT : LVCFMT_LEFT;
        col.cx = r.right;
        col.pszText = const_cast<LPWSTR>(c.titulo);
        SendMessageW(lv, LVM_INSERTCOLUMNW, static_cast<WPARAM>(i++), reinterpret_cast<LPARAM>(&col));
    }
}

void lvLimpiar(HWND dlg, int id) { ListView_DeleteAllItems(GetDlgItem(dlg, id)); }

int lvAgregar(HWND dlg, int id, const std::vector<std::string>& celdas, LPARAM dato) {
    HWND lv = GetDlgItem(dlg, id);
    std::wstring t0 = celdas.empty() ? std::wstring() : W(celdas[0]);
    LVITEMW it{};
    it.mask = LVIF_TEXT | LVIF_PARAM;
    it.iItem = ListView_GetItemCount(lv);
    it.pszText = &t0[0];
    it.lParam = dato;
    const int fila = static_cast<int>(SendMessageW(lv, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&it)));
    for (size_t c = 1; c < celdas.size(); ++c) {
        std::wstring t = W(celdas[c]);
        LVITEMW sub{};
        sub.iSubItem = static_cast<int>(c);
        sub.pszText = &t[0];
        SendMessageW(lv, LVM_SETITEMTEXTW, static_cast<WPARAM>(fila), reinterpret_cast<LPARAM>(&sub));
    }
    return fila;
}

int lvSeleccion(HWND dlg, int id) { return ListView_GetNextItem(GetDlgItem(dlg, id), -1, LVNI_SELECTED); }

LPARAM lvDato(HWND dlg, int id, int fila) {
    LVITEMW it{};
    it.mask = LVIF_PARAM;
    it.iItem = fila;
    SendMessageW(GetDlgItem(dlg, id), LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&it));
    return it.lParam;
}

LPARAM lvDatoSeleccionado(HWND dlg, int id, LPARAM porDefecto) {
    const int f = lvSeleccion(dlg, id);
    return f < 0 ? porDefecto : lvDato(dlg, id, f);
}

void lvSeleccionar(HWND dlg, int id, int fila) {
    HWND lv = GetDlgItem(dlg, id);
    ListView_SetItemState(lv, fila, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(lv, fila, FALSE);
}

// ================================================================ Mensajes
static const wchar_t* const TITULO = L"Beneficio Unimax";

void mostrarError(HWND padre, const std::string& msg) { MessageBoxW(padre, W(msg).c_str(), TITULO, MB_OK | MB_ICONERROR); }

void mostrarAviso(HWND padre, const std::string& msg) {
    MessageBoxW(padre, W(msg).c_str(), TITULO, MB_OK | MB_ICONWARNING);
}

void mostrarInfo(HWND padre, const std::string& msg) {
    MessageBoxW(padre, W(msg).c_str(), TITULO, MB_OK | MB_ICONINFORMATION);
}

bool confirmar(HWND padre, const std::string& msg) {
    return MessageBoxW(padre, W(msg).c_str(), TITULO, MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void mostrarExcepcion(HWND padre, const std::exception& e) {
    if (dynamic_cast<const ErrorNegocio*>(&e)) {
        mostrarAviso(padre, e.what());
    } else if (dynamic_cast<const db::ErrorBD*>(&e)) {
        mostrarError(padre, std::string("Error de la base de datos:\n\n") + e.what());
    } else {
        mostrarError(padre, e.what());
    }
}

// ================================================================ Fuentes
namespace {
HFONT crearFuente(int puntos, int peso) {
    HDC dc = GetDC(nullptr);
    const int alto = -MulDiv(puntos, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, dc);
    return CreateFontW(alto, 0, 0, 0, peso, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}
}  // namespace

HFONT fuenteNormal() {
    static HFONT f = crearFuente(9, FW_NORMAL);
    return f;
}

HFONT fuenteNegrita() {
    static HFONT f = crearFuente(9, FW_BOLD);
    return f;
}

HFONT fuenteGrande() {
    static HFONT f = crearFuente(13, FW_BOLD);
    return f;
}

void ponerFuente(HWND dlg, int id, HFONT f) { SendDlgItemMessageW(dlg, id, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE); }

// ================================================================ Archivos
bool existeArchivo(const std::wstring& ruta) { return GetFileAttributesW(ruta.c_str()) != INVALID_FILE_ATTRIBUTES; }

void crearCarpetas(const std::wstring& ruta) { SHCreateDirectoryExW(nullptr, ruta.c_str(), nullptr); }

void abrirCarpeta(const std::wstring& ruta) {
    crearCarpetas(ruta);
    ShellExecuteW(nullptr, L"open", ruta.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

std::wstring carpetaDe(const std::wstring& ruta) {
    const auto p = ruta.find_last_of(L"\\/");
    return p == std::wstring::npos ? std::wstring() : ruta.substr(0, p);
}

}  // namespace bu::win
