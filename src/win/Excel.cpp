#include "win/Excel.h"

#include <oleauto.h>

#include <cstdio>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "win/App.h"

namespace bu::win {

namespace {

// ---------------------------------------------------------------- Variantes
VARIANT vTexto(const std::wstring& s) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_BSTR;
    v.bstrVal = SysAllocString(s.c_str());
    return v;
}

VARIANT vEntero(long n) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I4;
    v.lVal = n;
    return v;
}

VARIANT vNumero(double d) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_R8;
    v.dblVal = d;
    return v;
}

double aNumero(VARIANT v) {
    VARIANT r;
    VariantInit(&r);
    double n = 0;
    if (SUCCEEDED(VariantChangeType(&r, &v, 0, VT_R8))) n = r.dblVal;
    VariantClear(&r);
    VariantClear(&v);
    return n;
}

VARIANT vBool(bool b) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_BOOL;
    v.boolVal = b ? VARIANT_TRUE : VARIANT_FALSE;
    return v;
}

VARIANT aVariante(const Valor& valor) {
    VARIANT v;
    VariantInit(&v);
    if (const auto* s = std::get_if<std::string>(&valor)) {
        v.vt = VT_BSTR;
        v.bstrVal = SysAllocString(W(*s).c_str());
    } else if (const auto* d = std::get_if<double>(&valor)) {
        v.vt = VT_R8;
        v.dblVal = *d;
    } else if (const auto* f = std::get_if<Fecha>(&valor)) {
        v.vt = VT_DATE;
        v.date = f->fechaOle();
    }
    return v;
}

// ---------------------------------------------------------------- IDispatch (enlace tardío)
class Disp {
public:
    Disp() = default;
    explicit Disp(IDispatch* p) : p_(p) {}
    ~Disp() {
        if (p_) p_->Release();
    }
    Disp(Disp&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Disp& operator=(Disp&& o) noexcept {
        if (this != &o) {
            if (p_) p_->Release();
            p_ = o.p_;
            o.p_ = nullptr;
        }
        return *this;
    }
    Disp(const Disp&) = delete;
    Disp& operator=(const Disp&) = delete;
    explicit operator bool() const { return p_ != nullptr; }

    // Los argumentos se pasan en orden natural y se liberan después de la llamada.
    VARIANT invocar(WORD tipo, const wchar_t* nombre, std::vector<VARIANT> args) {
        struct Liberar {
            std::vector<VARIANT>& a;
            ~Liberar() {
                for (auto& x : a) VariantClear(&x);
            }
        } liberar{args};
        if (!p_) throw std::runtime_error("Objeto de Excel no disponible.");
        DISPID id = 0;
        LPOLESTR n = const_cast<LPOLESTR>(nombre);
        HRESULT hr = p_->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
        if (FAILED(hr)) throw std::runtime_error("Excel no reconoce \"" + U(nombre) + "\".");
        std::vector<VARIANT> invertidos(args.rbegin(), args.rend());
        DISPPARAMS dp{};
        dp.cArgs = static_cast<UINT>(invertidos.size());
        dp.rgvarg = invertidos.empty() ? nullptr : invertidos.data();
        DISPID put = DISPID_PROPERTYPUT;
        if (tipo & DISPATCH_PROPERTYPUT) {
            dp.cNamedArgs = 1;
            dp.rgdispidNamedArgs = &put;
        }
        VARIANT resultado;
        VariantInit(&resultado);
        EXCEPINFO ei{};
        UINT argError = 0;
        hr = p_->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, tipo, &dp, &resultado, &ei, &argError);
        if (FAILED(hr)) {
            std::string detalle;
            if (ei.bstrDescription) detalle = U(ei.bstrDescription);
            if (ei.bstrSource) SysFreeString(ei.bstrSource);
            if (ei.bstrDescription) SysFreeString(ei.bstrDescription);
            if (ei.bstrHelpFile) SysFreeString(ei.bstrHelpFile);
            char codigo[16];
            std::snprintf(codigo, sizeof codigo, "0x%08lX", static_cast<unsigned long>(hr));
            throw std::runtime_error("Excel devolvió un error en \"" + U(nombre) + "\" (" + codigo + ")" +
                                     (detalle.empty() ? std::string(".") : ": " + detalle));
        }
        return resultado;
    }

    Disp obtener(const wchar_t* nombre, std::vector<VARIANT> args = {}) {
        VARIANT v = invocar(DISPATCH_PROPERTYGET | DISPATCH_METHOD, nombre, std::move(args));
        if (v.vt != VT_DISPATCH || !v.pdispVal) {
            VariantClear(&v);
            throw std::runtime_error("Excel no devolvió un objeto para \"" + U(nombre) + "\".");
        }
        return Disp(v.pdispVal);  // se transfiere la referencia
    }

    VARIANT valor(const wchar_t* nombre) { return invocar(DISPATCH_PROPERTYGET, nombre, {}); }

    void poner(const wchar_t* nombre, VARIANT valor) {
        VARIANT r = invocar(DISPATCH_PROPERTYPUT, nombre, {valor});
        VariantClear(&r);
    }

    void llamar(const wchar_t* nombre, std::vector<VARIANT> args = {}) {
        VARIANT r = invocar(DISPATCH_METHOD, nombre, std::move(args));
        VariantClear(&r);
    }

private:
    IDispatch* p_ = nullptr;
};

// El orden de los índices de SafeArrayPutElement no está claro en la documentación;
// se comprueba una sola vez con un arreglo de 2 x 3.
bool indicesNaturales() {
    static const bool natural = [] {
        SAFEARRAYBOUND b[2];
        b[0].lLbound = 1;
        b[0].cElements = 2;  // filas
        b[1].lLbound = 1;
        b[1].cElements = 3;  // columnas
        SAFEARRAY* sa = SafeArrayCreate(VT_VARIANT, 2, b);
        LONG idx[2] = {1, 3};  // fila 1, columna 3
        VARIANT v;
        VariantInit(&v);
        v.vt = VT_I4;
        v.lVal = 1;
        const HRESULT hr = SafeArrayPutElement(sa, idx, &v);
        SafeArrayDestroy(sa);
        return SUCCEEDED(hr);
    }();
    return natural;
}

VARIANT arregloTabla(const std::vector<std::vector<Valor>>& filas, size_t columnas) {
    SAFEARRAYBOUND b[2];
    b[0].lLbound = 1;
    b[0].cElements = static_cast<ULONG>(filas.size());
    b[1].lLbound = 1;
    b[1].cElements = static_cast<ULONG>(columnas);
    SAFEARRAY* sa = SafeArrayCreate(VT_VARIANT, 2, b);
    if (!sa) throw std::runtime_error("No hay memoria para preparar la tabla de Excel.");
    const bool natural = indicesNaturales();
    for (size_t f = 0; f < filas.size(); ++f) {
        for (size_t c = 0; c < columnas; ++c) {
            VARIANT v = c < filas[f].size() ? aVariante(filas[f][c]) : VARIANT{};
            if (c >= filas[f].size()) VariantInit(&v);
            LONG idx[2];
            idx[0] = natural ? static_cast<LONG>(f + 1) : static_cast<LONG>(c + 1);
            idx[1] = natural ? static_cast<LONG>(c + 1) : static_cast<LONG>(f + 1);
            SafeArrayPutElement(sa, idx, &v);
            VariantClear(&v);
        }
    }
    VARIANT r;
    VariantInit(&r);
    r.vt = VT_ARRAY | VT_VARIANT;
    r.parray = sa;
    return r;
}

std::wstring destinoLibre(const std::wstring& carpeta, const std::wstring& nombre, const std::wstring& plantilla) {
    std::wstring destino = carpeta + L"\\" + nombre + L".xlsx";
    for (int i = 2; i < 100; ++i) {
        if (CopyFileW(plantilla.c_str(), destino.c_str(), FALSE)) {
            SetFileAttributesW(destino.c_str(), FILE_ATTRIBUTE_NORMAL);
            return destino;
        }
        // Puede estar abierto en Excel: se usa otro nombre.
        destino = carpeta + L"\\" + nombre + L" (" + std::to_wstring(i) + L").xlsx";
    }
    throw std::runtime_error("No se pudo crear el archivo en " + U(carpeta) + ".");
}

}  // namespace

std::wstring generarDocumento(const Documento& doc, bool tambienPdf, bool mostrar) {
    App& app = App::instancia();
    const std::wstring plantilla = app.rutaPlantilla(doc.plantilla);
    if (!existeArchivo(plantilla))
        throw std::runtime_error("No se encontró la plantilla de Excel \"" + doc.plantilla + "\".\nBúsquela en:\n" +
                                 U(app.rutas().plantillas));
    const std::wstring carpeta = app.rutas().documentos + L"\\" + W(doc.subcarpeta);
    crearCarpetas(carpeta);
    const std::wstring destino = destinoLibre(carpeta, W(doc.nombreArchivo), plantilla);

    CLSID clsid;
    if (FAILED(CLSIDFromProgID(L"Excel.Application", &clsid)))
        throw std::runtime_error("No se encontró Microsoft Excel en esta computadora.\nEl archivo quedó sin llenar en:\n" +
                                 U(destino));
    IDispatch* p = nullptr;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&p))) || !p)
        throw std::runtime_error("No se pudo iniciar Microsoft Excel.");
    Disp excel(p);
    Disp libro;
    std::vector<std::string> faltantes;
    try {
        excel.poner(L"DisplayAlerts", vBool(false));
        Disp libros = excel.obtener(L"Workbooks");
        libro = libros.obtener(L"Open", {vTexto(destino)});
        Disp nombres = libro.obtener(L"Names");

        auto rangoDe = [&](const std::string& nombre) -> Disp {
            try {
                return nombres.obtener(L"Item", {vTexto(W(nombre))}).obtener(L"RefersToRange");
            } catch (const std::exception&) {
                faltantes.push_back(nombre);
                return Disp();
            }
        };

        for (const auto& celda : doc.celdas) {
            Disp r = rangoDe(celda.first);
            if (r) r.poner(L"Value", aVariante(celda.second));
        }
        for (const auto& tabla : doc.tablas) {
            Disp inicio = rangoDe(tabla.nombre);
            if (!inicio || tabla.filas.empty()) continue;
            const size_t columnas = tabla.formatos.size();
            Disp destinoTabla = inicio.obtener(L"Resize", {vEntero(static_cast<long>(tabla.filas.size())),
                                                           vEntero(static_cast<long>(columnas))});
            // Formatos antes de escribir, para que los textos (DNI, horas) no se conviertan en números.
            for (size_t c = 0; c < columnas; ++c)
                destinoTabla.obtener(L"Columns")
                    .obtener(L"Item", {vEntero(static_cast<long>(c + 1))})
                    .poner(L"NumberFormat", vTexto(W(tabla.formatos[c])));
            destinoTabla.poner(L"Value", arregloTabla(tabla.filas, columnas));
            destinoTabla.obtener(L"Borders").poner(L"LineStyle", vEntero(1));  // xlContinuous
        }
        // Marca de agua (imagen PNG casi transparente) centrada sobre el contenido de la primera hoja.
        const int marca = app.marcaAgua();
        const std::wstring png = app.rutaMarcaAguaPng(marca);
        if (marca != MARCA_NINGUNA && existeArchivo(png)) {
            try {
                Disp hoja = libro.obtener(L"Worksheets").obtener(L"Item", {vEntero(1)});
                Disp usado = hoja.obtener(L"UsedRange");
                const double izq = aNumero(usado.valor(L"Left")), arriba = aNumero(usado.valor(L"Top"));
                const double ancho = aNumero(usado.valor(L"Width")), alto = aNumero(usado.valor(L"Height"));
                double lado = std::min({ancho, alto, 430.0}) * 0.9;
                if (lado < 120) lado = std::min(ancho, 300.0);
                hoja.obtener(L"Shapes").obtener(
                    L"AddPicture", {vTexto(png), vEntero(0), vEntero(-1), vNumero(izq + (ancho - lado) / 2),
                                    vNumero(arriba + std::max(0.0, (alto - lado) / 2)), vNumero(lado), vNumero(lado)});
            } catch (...) {
                // La marca de agua es opcional: si Excel no la acepta, el documento sale igual.
            }
        }
        libro.llamar(L"Save");
        if (tambienPdf) {
            std::wstring pdf = destino.substr(0, destino.size() - 5) + L".pdf";
            libro.llamar(L"ExportAsFixedFormat", {vEntero(0), vTexto(pdf)});  // 0 = xlTypePDF
        }
        if (mostrar) {
            excel.poner(L"Visible", vBool(true));
            excel.poner(L"UserControl", vBool(true));
            VARIANT hwnd = excel.valor(L"Hwnd");
            if (hwnd.vt == VT_I4) {
                HWND ventanaExcel = reinterpret_cast<HWND>(static_cast<LONG_PTR>(hwnd.lVal));
                ShowWindow(ventanaExcel, SW_SHOWMAXIMIZED);
                SetForegroundWindow(ventanaExcel);
            }
            VariantClear(&hwnd);
        } else {
            libro.llamar(L"Close", {vBool(false)});
            excel.llamar(L"Quit");
        }
    } catch (...) {
        try {
            if (libro) libro.llamar(L"Close", {vBool(false)});
            excel.llamar(L"Quit");
        } catch (...) {
        }
        throw;
    }
    if (!faltantes.empty()) {
        std::string lista;
        for (const auto& n : faltantes) lista += "\n  • " + n;
        throw std::runtime_error("El documento se generó, pero la plantilla \"" + doc.plantilla +
                                 "\" no tiene estas celdas con nombre (Fórmulas > Administrador de nombres):" + lista);
    }
    return destino;
}

bool generarYMostrar(HWND padre, const Documento& doc) {
    return intentar(padre, [&] {
        CursorEspera espera;
        const bool pdf = repo().config("GuardarPdf", "0") == "1";
        generarDocumento(doc, pdf, true);
    });
}

bool generarEstadoCuenta(HWND padre, long clienteId) {
    Documento doc;
    const bool ok = intentar(padre, [&] {
        const auto c = repo().clientePorId(clienteId);
        if (!c) throw ErrorNegocio("Seleccione un cliente.");
        FiltroDeudas f;
        f.soloPendientes = false;
        f.clienteId = clienteId;
        const auto deudas = repo().deudas(f);
        std::vector<std::vector<Abono>> abonos;
        for (const auto& d : deudas) abonos.push_back(repo().abonos(d.id));
        doc = docEstadoCuenta(repo().empresa(), *c, deudas, abonos, Fecha::hoy());
    });
    return ok && generarYMostrar(padre, doc);
}

}  // namespace bu::win
