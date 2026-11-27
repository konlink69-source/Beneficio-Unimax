#include "win/Impresion.h"

#include <winspool.h>

#include <algorithm>
#include <cwctype>
#include <functional>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <stdexcept>

#include "core/Documentos.h"
#include "core/Texto.h"
#include "win/App.h"
#include "win/Excel.h"
#include "win/resource.h"

namespace bu::win {

namespace {

COLORREF cr(uint32_t c) { return RGB((c >> 16) & 255, (c >> 8) & 255, c & 255); }
int L(double v) { return static_cast<int>(std::lround(v)); }

int idImagen(const std::string& nombre) {
    if (nombre == "logo") return IDB_LOGO_DOC;
    if (nombre == "marca1") return IDB_MARCA1;
    if (nombre == "marca2") return IDB_MARCA2;
    return 0;
}


}  // namespace

// ================================================================ Dibujo
void dibujarLienzo(HDC dc, const Lienzo& l, const RECT& destino) {
    const int guardado = SaveDC(dc);
    // Unidades lógicas = unidades del lienzo (1/100 de pulgada), ajustadas al rectángulo destino.
    SetMapMode(dc, MM_ANISOTROPIC);
    SetWindowOrgEx(dc, 0, 0, nullptr);
    SetWindowExtEx(dc, L(l.ancho), L(l.alto), nullptr);
    SetViewportOrgEx(dc, destino.left, destino.top, nullptr);
    SetViewportExtEx(dc, destino.right - destino.left, destino.bottom - destino.top, nullptr);
    SetBkMode(dc, TRANSPARENT);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);

    for (const auto& e : l.elementos) {
        switch (e.tipo) {
            case TipoElemento::Rectangulo: {
                const RECT r{L(e.x), L(e.y), L(e.x + e.ancho), L(e.y + e.alto)};
                if (e.relleno != SIN_COLOR) {
                    HBRUSH b = CreateSolidBrush(cr(e.relleno));
                    FillRect(dc, &r, b);
                    DeleteObject(b);
                }
                if (e.color != SIN_COLOR) {
                    HPEN p = CreatePen(PS_SOLID, std::max(1, L(e.grosor)), cr(e.color));
                    HGDIOBJ pa = SelectObject(dc, p);
                    HGDIOBJ ba = SelectObject(dc, GetStockObject(NULL_BRUSH));
                    Rectangle(dc, r.left, r.top, r.right, r.bottom);
                    SelectObject(dc, ba);
                    SelectObject(dc, pa);
                    DeleteObject(p);
                }
                break;
            }
            case TipoElemento::Linea: {
                HPEN p = CreatePen(PS_SOLID, std::max(1, L(e.grosor)), cr(e.color));
                HGDIOBJ pa = SelectObject(dc, p);
                MoveToEx(dc, L(e.x), L(e.y), nullptr);
                LineTo(dc, L(e.x + e.ancho), L(e.y + e.alto));
                SelectObject(dc, pa);
                DeleteObject(p);
                break;
            }
            case TipoElemento::Imagen: {
                const ImagenDib img = imagenRecurso(idImagen(e.imagen));
                if (!img.info || img.ancho <= 0 || img.alto <= 0) break;
                // Se ajusta al cuadro sin deformarse, centrada.
                const double factor = std::min(e.ancho / img.ancho, e.alto / img.alto);
                const double w = img.ancho * factor, h = img.alto * factor;
                StretchDIBits(dc, L(e.x + (e.ancho - w) / 2), L(e.y + (e.alto - h) / 2), L(w), L(h), 0, 0, img.ancho,
                              img.alto, img.bits, img.info, DIB_RGB_COLORS, SRCCOPY);
                break;
            }
            case TipoElemento::Texto: {
                HFONT f = CreateFontW(-L(e.puntos * 100.0 / 72.0), 0, 0, 0, e.negrita ? FW_BOLD : FW_NORMAL, FALSE, FALSE,
                                      FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HGDIOBJ fa = SelectObject(dc, f);
                SetTextColor(dc, cr(e.color));
                RECT r{L(e.x), L(e.y), L(e.x + e.ancho), L(e.y + e.alto)};
                UINT flags = DT_NOPREFIX | DT_TOP;
                flags |= e.variasLineas ? DT_WORDBREAK : (DT_SINGLELINE | DT_END_ELLIPSIS);
                flags |= e.alineacion == ALINEAR_CENTRO ? DT_CENTER : (e.alineacion == ALINEAR_DERECHA ? DT_RIGHT : DT_LEFT);
                std::wstring t = W(e.texto);
                DrawTextW(dc, t.c_str(), static_cast<int>(t.size()), &r, flags);
                SelectObject(dc, fa);
                DeleteObject(f);
                break;
            }
        }
    }
    RestoreDC(dc, guardado);
}

// ================================================================ Impresoras
std::vector<std::wstring> impresoras() {
    std::vector<std::wstring> r;
    DWORD necesario = 0, cantidad = 0;
    const DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    EnumPrintersW(flags, nullptr, 4, nullptr, 0, &necesario, &cantidad);
    if (necesario == 0) return r;
    std::vector<BYTE> buffer(necesario);
    if (!EnumPrintersW(flags, nullptr, 4, buffer.data(), necesario, &necesario, &cantidad)) return r;
    const auto* info = reinterpret_cast<const PRINTER_INFO_4W*>(buffer.data());
    for (DWORD i = 0; i < cantidad; ++i)
        if (info[i].pPrinterName) r.emplace_back(info[i].pPrinterName);
    return r;
}

std::wstring impresoraPredeterminada() {
    DWORD n = 0;
    GetDefaultPrinterW(nullptr, &n);
    if (n == 0) return std::wstring();
    std::wstring r(n, L'\0');
    if (!GetDefaultPrinterW(&r[0], &n)) return std::wstring();
    r.resize(wcslen(r.c_str()));
    return r;
}

bool esImpresoraVirtual(const std::wstring& nombre) {
    // Estas "impresoras" no imprimen en papel: piden guardar un archivo.
    std::wstring n = nombre;
    for (auto& ch : n) ch = static_cast<wchar_t>(towlower(ch));
    for (const wchar_t* v : {L"pdf", L"xps", L"onenote", L"fax", L"enviar a", L"send to"})
        if (n.find(v) != std::wstring::npos) return true;
    return false;
}

std::wstring impresoraConfigurada() {
    const auto lista = impresoras();
    // La impresora es de cada computadora (no se toma la de otra computadora por Google Drive).
    const std::wstring elegida = W(App::instancia().configEquipo("Impresora", repo().config("Impresora")));
    if (!elegida.empty() && !esImpresoraVirtual(elegida))
        for (const auto& p : lista)
            if (p == elegida) return elegida;
    const std::wstring predeterminada = impresoraPredeterminada();
    if (!predeterminada.empty() && !esImpresoraVirtual(predeterminada)) return predeterminada;
    for (const auto& p : lista)
        if (!esImpresoraVirtual(p)) return p;
    return std::wstring();  // no hay impresora física
}

bool imprimirLienzo(const Lienzo& l, const std::wstring& impresora, int copias, const std::wstring& documento,
                    const std::wstring& archivoSalida, std::string& error) {
    if (impresora.empty()) {
        error = "No hay ninguna impresora instalada en Windows.";
        return false;
    }
    HDC dc = CreateDCW(L"WINSPOOL", impresora.c_str(), nullptr, nullptr);
    if (!dc) {
        error = "No se pudo usar la impresora \"" + U(impresora) + "\". Revise que esté encendida e instalada.";
        return false;
    }
    DOCINFOW di{};
    di.cbSize = sizeof di;
    di.lpszDocName = documento.c_str();
    di.lpszOutput = archivoSalida.empty() ? nullptr : archivoSalida.c_str();
    if (StartDocW(dc, &di) <= 0) {
        error = "Windows no pudo iniciar la impresión en \"" + U(impresora) + "\" (código " +
                std::to_string(GetLastError()) + ").";
        DeleteDC(dc);
        return false;
    }
    // Hoja carta (8.5 x 11 pulgadas). Si el papel es más pequeño (por ejemplo A4), se reduce un poco.
    const int ppx = GetDeviceCaps(dc, LOGPIXELSX), ppy = GetDeviceCaps(dc, LOGPIXELSY);
    const int anchoFisico = GetDeviceCaps(dc, PHYSICALWIDTH), altoFisico = GetDeviceCaps(dc, PHYSICALHEIGHT);
    const int margenX = GetDeviceCaps(dc, PHYSICALOFFSETX), margenY = GetDeviceCaps(dc, PHYSICALOFFSETY);
    double escala = 1.0;
    if (anchoFisico > 0 && altoFisico > 0)
        escala = std::min({1.0, anchoFisico / (8.5 * ppx), altoFisico / (11.0 * ppy)});
    const int w = L(8.5 * ppx * escala), h = L(11.0 * ppy * escala);
    const int x0 = (anchoFisico > 0 ? (anchoFisico - w) / 2 : 0) - margenX;
    const RECT destino{x0, -margenY, x0 + w, -margenY + h};
    bool ok = true;
    for (int c = 0; c < std::max(1, copias) && ok; ++c) {
        ok = StartPage(dc) > 0;
        if (ok) {
            dibujarLienzo(dc, l, destino);
            ok = EndPage(dc) > 0;
        }
    }
    if (ok) {
        EndDoc(dc);
    } else {
        AbortDoc(dc);
        error = "La impresora \"" + U(impresora) + "\" no aceptó el documento.";
    }
    DeleteDC(dc);
    return ok;
}

// ================================================================ Comprobantes
std::wstring carpetaFacturasCliente(const Cliente& c) {
    const std::wstring carpeta =
        App::instancia().rutas().documentos + L"\\Facturas\\" + W(nombreCarpetaCliente(c.nombre, c.dni));
    crearCarpetas(carpeta);
    return carpeta;
}

Lienzo lienzoDeFactura(long facturaId, Factura* factura, Cliente* cliente) {
    const auto f = repo().facturaPorId(facturaId);
    if (!f) throw ErrorNegocio("El comprobante no existe.");
    const auto c = repo().clientePorId(f->clienteId);
    if (!c) throw ErrorNegocio("El cliente del comprobante no existe.");
    if (factura) *factura = *f;
    if (cliente) *cliente = *c;
    // Las facturas llevan siempre el sello (opción 2) como marca de agua.
    return lienzoComprobante(repo().empresa(), *f, *c, MARCA_SELLO,
                             static_cast<int>(repo().configEntero("DisenoComprobante", DISENO_COMPROBANTE_ACTUAL)));
}

std::wstring imprimirComprobante(long facturaId) {
    Factura f;
    const Lienzo l = lienzoDeFactura(facturaId, &f);
    const std::wstring impresora = impresoraConfigurada();
    if (impresora.empty())
        throw ErrorNegocio("No hay una impresora conectada. La factura " + f.numeroTexto() +
                           " ya está guardada en PDF en la carpeta del cliente; se imprimirá cuando conecte una impresora.");
    const int copias = static_cast<int>(std::clamp(repo().configEntero("CopiasComprobante", 1), 1L, 5L));
    std::string error;
    if (!imprimirLienzo(l, impresora, copias, W("Comprobante " + f.numeroTexto()), std::wstring(), error))
        throw std::runtime_error(error);
    return impresora;
}

std::wstring rutaPdfComprobante(long facturaId) {
    const auto f = repo().facturaPorId(facturaId);
    if (!f) throw ErrorNegocio("El comprobante no existe.");
    const auto c = repo().clientePorId(f->clienteId);
    if (!c) throw ErrorNegocio("El cliente del comprobante no existe.");
    return carpetaFacturasCliente(*c) + L"\\Comprobante_" + W(f->numeroTexto()) + L".pdf";
}

std::wstring guardarPdfComprobante(long facturaId) {
    Factura f;
    Cliente c;
    const Lienzo l = lienzoDeFactura(facturaId, &f, &c);
    std::wstring pdf;
    for (const auto& p : impresoras())
        if (p == L"Microsoft Print to PDF") pdf = p;
    if (pdf.empty())
        throw std::runtime_error(
            "No se encontró la impresora \"Microsoft Print to PDF\" de Windows. Actívela en Panel de control > "
            "Programas > Activar o desactivar las características de Windows.");
    const std::wstring carpeta = carpetaFacturasCliente(c);
    const std::wstring base = carpeta + L"\\Comprobante_" + W(f.numeroTexto());
    std::wstring ruta = base + L".pdf";
    // Si ya existe (por ejemplo, abierto en un visor de PDF) se usa otro nombre.
    for (int i = 2; existeArchivo(ruta) && !DeleteFileW(ruta.c_str()) && i < 50; ++i)
        ruta = base + L" (" + std::to_wstring(i) + L").pdf";
    std::string error;
    if (!imprimirLienzo(l, pdf, 1, W("Comprobante " + f.numeroTexto()), ruta, error)) throw std::runtime_error(error);
    return ruta;
}

// ================================================================ Visor dentro del programa
namespace {

const wchar_t* const CLASE_VISOR = L"BeneficioUnimaxVisor";
constexpr int ID_PRIMER_BOTON = 101;

struct BotonVisor {
    std::wstring texto;
    int ancho;  // a 96 ppp
    std::function<void(HWND)> accion;  // vacía = cerrar
};

struct Visor {
    Lienzo lienzo;
    std::vector<BotonVisor> botones;
    std::vector<HWND> controles;
    HWND duenio = nullptr;
    bool cerrado = false;
};

int px(int valor) {
    HDC dc = GetDC(nullptr);
    const int r = MulDiv(valor, GetDeviceCaps(dc, LOGPIXELSY), 96);
    ReleaseDC(nullptr, dc);
    return r;
}

int altoBarra() { return px(56); }

void cerrarVisor(HWND h, Visor* v) {
    if (v && v->duenio) EnableWindow(v->duenio, TRUE);  // antes de destruir, para que el foco vuelva al programa
    DestroyWindow(h);
}

void pintarVisor(HWND h, HDC dc, Visor* v) {
    RECT cliente;
    GetClientRect(h, &cliente);
    const int w = cliente.right, alto = cliente.bottom;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, std::max(1, w), std::max(1, alto));
    HGDIOBJ anterior = SelectObject(mem, bmp);
    HBRUSH gris = CreateSolidBrush(RGB(96, 96, 96));
    FillRect(mem, &cliente, gris);
    DeleteObject(gris);
    RECT barra{0, 0, w, altoBarra()};
    HBRUSH crema = CreateSolidBrush(RGB(241, 234, 219));
    FillRect(mem, &barra, crema);
    DeleteObject(crema);
    if (v) {
        const int margen = px(14);
        const int disponibleW = w - 2 * margen, disponibleH = alto - altoBarra() - 2 * margen;
        if (disponibleW > 50 && disponibleH > 50) {
            const double escala = std::min(disponibleW / v->lienzo.ancho, disponibleH / v->lienzo.alto);
            const int pw = L(v->lienzo.ancho * escala), ph = L(v->lienzo.alto * escala);
            const int x = (w - pw) / 2, y = altoBarra() + margen;
            RECT sombra{x + px(4), y + px(4), x + pw + px(4), y + ph + px(4)};
            HBRUSH negro = CreateSolidBrush(RGB(50, 50, 50));
            FillRect(mem, &sombra, negro);
            DeleteObject(negro);
            RECT hoja{x, y, x + pw, y + ph};
            FillRect(mem, &hoja, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            dibujarLienzo(mem, v->lienzo, hoja);
        }
    }
    BitBlt(dc, 0, 0, w, alto, mem, 0, 0, SRCCOPY);
    SelectObject(mem, anterior);
    DeleteObject(bmp);
    DeleteDC(mem);
}

LRESULT CALLBACK procVisor(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* v = reinterpret_cast<Visor*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            v = static_cast<Visor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(v));
            for (size_t i = 0; i < v->botones.size(); ++i) {
                HWND b = CreateWindowExW(0, L"BUTTON", v->botones[i].texto.c_str(),
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10, h,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PRIMER_BOTON + i)),
                                         App::instancia().hInst, nullptr);
                SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(fuente(tamanoLetra() + 1, true)), TRUE);
                v->controles.push_back(b);
            }
            return 0;
        }
        case WM_SIZE: {
            if (!v) return 0;
            int x = px(12);
            for (size_t i = 0; i < v->controles.size(); ++i) {
                const int ancho = px(v->botones[i].ancho);
                SetWindowPos(v->controles[i], nullptr, x, px(9), ancho, altoBarra() - px(18), SWP_NOZORDER);
                x += ancho + px(8);
            }
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            pintarVisor(h, dc, v);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
            mm->ptMinTrackSize.x = px(700);
            mm->ptMinTrackSize.y = px(500);
            return 0;
        }
        case WM_COMMAND: {
            if (!v) return 0;
            const int id = LOWORD(wp);
            if (id == IDCANCEL) {
                cerrarVisor(h, v);
                return 0;
            }
            const int i = id - ID_PRIMER_BOTON;
            if (i >= 0 && i < static_cast<int>(v->botones.size())) {
                if (v->botones[static_cast<size_t>(i)].accion)
                    intentar(h, [&] { v->botones[static_cast<size_t>(i)].accion(h); });
                else
                    cerrarVisor(h, v);
            }
            return 0;
        }
        case WM_CLOSE:
            cerrarVisor(h, v);
            return 0;
        case WM_DESTROY:
            if (v) v->cerrado = true;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// Muestra un lienzo en una ventana del programa, con los botones indicados (más "Cerrar").
void mostrarLienzo(HWND padre, const Lienzo& lienzo, const std::wstring& titulo, std::vector<BotonVisor> botones) {
    Visor v;
    v.lienzo = lienzo;
    v.botones = std::move(botones);
    v.botones.push_back({L"Cerrar", 100, nullptr});
    static bool registrada = false;
    if (!registrada) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = procVisor;
        wc.hInstance = App::instancia().hInst;
        wc.hIcon = LoadIconW(App::instancia().hInst, MAKEINTRESOURCEW(IDI_APP));
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.lpszClassName = CLASE_VISOR;
        RegisterClassExW(&wc);
        registrada = true;
    }
    v.duenio = padre ? GetAncestor(padre, GA_ROOT) : nullptr;
    RECT trabajo;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &trabajo, 0);
    const int altoT = trabajo.bottom - trabajo.top, anchoT = trabajo.right - trabajo.left;
    const int alto = altoT * 96 / 100;
    int anchoBotones = px(40);
    for (const auto& b : v.botones) anchoBotones += px(b.ancho + 8);
    const int ancho = std::min(anchoT * 95 / 100, std::max(anchoBotones, static_cast<int>((alto - altoBarra()) * 0.80)));
    HWND h = CreateWindowExW(0, CLASE_VISOR, titulo.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             trabajo.left + (anchoT - ancho) / 2, trabajo.top + (altoT - alto) / 2, ancho, alto, v.duenio,
                             nullptr, App::instancia().hInst, &v);
    if (!h) return;
    if (v.duenio) EnableWindow(v.duenio, FALSE);
    ShowWindow(h, SW_SHOW);
    if (!v.controles.empty()) SetFocus(v.controles[0]);
    MSG m;
    bool salir = false;
    while (!v.cerrado) {
        const BOOL r = GetMessageW(&m, nullptr, 0, 0);
        if (r <= 0) {
            salir = true;
            break;
        }
        if (!IsDialogMessageW(h, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    if (v.duenio) {
        EnableWindow(v.duenio, TRUE);
        SetForegroundWindow(v.duenio);
    }
    if (salir) PostQuitMessage(static_cast<int>(m.wParam));
}

}  // namespace

void verComprobante(HWND padre, long facturaId) {
    Factura f;
    Cliente c;
    Lienzo l;
    if (!intentar(padre, [&] { l = lienzoDeFactura(facturaId, &f, &c); })) return;
    std::vector<BotonVisor> botones;
    botones.push_back({L"Imprimir", 110, [facturaId](HWND h) {
                           CursorEspera espera;
                           mostrarInfo(h, "El comprobante se envió a la impresora \"" + U(imprimirComprobante(facturaId)) + "\".");
                       }});
    botones.push_back({L"Abrir carpeta del cliente", 200, [c](HWND) { abrirCarpeta(carpetaFacturasCliente(c)); }});
    botones.push_back({L"Abrir en Excel", 140, [f, c](HWND h) {
                           generarYMostrar(h, docComprobanteCompra(repo().empresa(), f, c));
                       }});
    mostrarLienzo(padre, l, L"Comprobante de compra " + W(f.numeroTexto()) + L"  -  " + W(c.nombre), std::move(botones));
}

Lienzo lienzoInforme(const Fecha& dia) {
    const auto resumen = [](const Fecha& desde, const Fecha& hasta) {
        return sumarResumenes(resumirPorDia(repo().detalleCompras(desde, hasta)));
    };
    ResumenDia hoy = resumen(dia, dia);
    hoy.fecha = dia;
    return lienzoInformeDia(repo().empresa(), dia, hoy, resumen(dia.primerDiaDelMes(), dia),
                            resumen(inicioCosecha(dia), dia), inicioCosecha(dia));
}

void verInformeDia(HWND padre, const Fecha& dia) {
    Lienzo l;
    if (!intentar(padre, [&] { l = lienzoInforme(dia); })) return;
    std::vector<BotonVisor> botones;
    botones.push_back({L"Imprimir", 110, [l, dia](HWND h) {
                           const std::wstring impresora = impresoraConfigurada();
                           if (impresora.empty()) throw ErrorNegocio("No hay una impresora conectada.");
                           std::string error;
                           CursorEspera espera;
                           if (!imprimirLienzo(l, impresora, 1, W("Informe del día " + dia.texto()), std::wstring(), error))
                               throw std::runtime_error(error);
                           mostrarInfo(h, "El informe se envió a la impresora \"" + U(impresora) + "\".");
                       }});
    botones.push_back({L"Reporte general (Excel)", 200, [dia](HWND h) { generarReporteGeneral(h, dia); }});
    mostrarLienzo(padre, l, L"Informe del día " + W(dia.texto()), std::move(botones));
}

void generarReporteGeneral(HWND padre, const Fecha& hasta) {
    Documento doc;
    const Fecha desde = inicioCosecha(hasta);
    if (intentar(padre, [&] {
            doc = docReporteGeneral(repo().empresa(), desde, hasta, resumirPorDia(repo().detalleCompras(desde, hasta)));
        }))
        generarYMostrar(padre, doc);
}

}  // namespace bu::win
