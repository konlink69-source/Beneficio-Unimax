// Pestaña "Configuración": datos de la empresa, caja, numeración y recordatorios.
#include "core/Texto.h"
#include "win/App.h"
#include "core/Grafico.h"
#include "win/Avisos.h"
#include "win/Impresion.h"
#include "win/Sesion.h"
#include "win/Paginas.h"
#include "win/resource.h"

namespace bu::win {

namespace {

class PaginaConfiguracion : public Pagina {
public:
    const wchar_t* titulo() const override { return L"Configuración"; }

    void alIniciar() override {
        SendDlgItemMessageW(h_, IDC_G_EMPRESA, EM_LIMITTEXT, 150, 0);
        SendDlgItemMessageW(h_, IDC_G_DIRECCION, EM_LIMITTEXT, 255, 0);
        SendDlgItemMessageW(h_, IDC_G_HORA, EM_LIMITTEXT, 5, 0);
        ponerFuente(h_, IDC_G_AVISO_ESTADO, fuenteNegrita());
        for (int m = MARCA_NINGUNA; m <= MARCA_SELLO; ++m) comboAgregar(h_, IDC_G_MARCA, nombreMarcaAgua(m), m);
        SendDlgItemMessageW(h_, IDC_G_COPIAS, EM_LIMITTEXT, 1, 0);
    }

    void alMostrar() override { cargar(); }

    bool alComando(int id, int) override {
        switch (id) {
            case IDC_G_GUARDAR:
                guardar();
                return true;
            case IDC_G_ACTIVAR: {
                const std::string hora = recortar(texto(h_, IDC_G_HORA));
                std::string error;
                if (!horaValida(hora)) {
                    mostrarAviso(h_, "Escriba la hora con formato HH:MM de 24 horas (por ejemplo 07:30 o 16:00).");
                    return true;
                }
                repo().setConfig("HoraAviso", hora);
                CursorEspera espera;
                if (activarAvisoWindows(hora, error))
                    mostrarInfo(h_, "Aviso activado. Todos los días a las " + hora +
                                        " Windows mostrará un aviso con las deudas vencidas y por vencer "
                                        "(si la computadora está encendida y con la sesión iniciada).");
                else
                    mostrarError(h_, error);
                cargar();
                return true;
            }
            case IDC_G_DESACTIVAR: {
                std::string error;
                if (!desactivarAvisoWindows(error)) mostrarError(h_, error);
                cargar();
                return true;
            }
            case IDC_G_PROBAR:
                lanzarPrograma(L"/recordatorio /probar" + std::wstring(App::instancia().rutas().datosPersonalizados
                                                                         ? L" /datos \"" + App::instancia().rutas().datos + L"\""
                                                                         : L""));
                mostrarInfo(h_, "En unos segundos aparecerá el aviso en la esquina de la pantalla.");
                return true;
            case IDC_G_USUARIOS:
                dialogoUsuarios(h_);
                return true;
            case IDC_G_CLAVE:
                dialogoCambiarMiContrasena(h_);
                return true;
            case IDC_G_PROBAR_IMP:
                probarImpresora();
                return true;
            case IDC_G_RESPALDAR:
                App::instancia().respaldar(h_);
                return true;
            case IDC_G_CARPETA:
                abrirCarpeta(App::instancia().rutas().datos);
                return true;
            default:
                return false;
        }
    }

    void alAceptar() override { guardar(); }

private:
    void cargar() {
        Repositorio& r = repo();
        ponerTexto(h_, IDC_G_EMPRESA, r.config("EmpresaNombre", "Beneficio Unimax"));
        ponerTexto(h_, IDC_G_RTN, r.config("EmpresaRTN"));
        ponerTexto(h_, IDC_G_DIRECCION, r.config("EmpresaDireccion"));
        ponerTexto(h_, IDC_G_TELEFONO, r.config("EmpresaTelefono"));
        ponerTexto(h_, IDC_G_APERTURA, formatoMonto(parsearMonto(r.config("SaldoAperturaCaja", "0")).value_or(0)));
        ponerTexto(h_, IDC_G_NUMFAC, std::to_string(r.configEntero("NumeroInicialFactura", 1)));
        ponerTexto(h_, IDC_G_NUMVEN, std::to_string(r.configEntero("NumeroInicialVenta", 1)));
        CheckDlgButton(h_, IDC_G_PDF, r.config("GuardarPdf", "0") == "1" ? BST_CHECKED : BST_UNCHECKED);
        ponerTexto(h_, IDC_G_DIAS, std::to_string(r.configEntero("DiasAnticipacion", 3)));
        CheckDlgButton(h_, IDC_G_ALABRIR, r.config("RecordatorioAlAbrir", "1") == "1" ? BST_CHECKED : BST_UNCHECKED);
        ponerTexto(h_, IDC_G_HORA, r.config("HoraAviso", "07:30"));
        const bool activo = avisoWindowsActivo();
        ponerTexto(h_, IDC_G_AVISO_ESTADO,
                   activo ? "Aviso de Windows ACTIVO: todos los días a las " + r.config("HoraAviso", "07:30") +
                                " aparece una notificación con las deudas vencidas y por vencer."
                          : "Aviso de Windows DESACTIVADO. Presione \"Activar aviso\" para recibirlo todos los días.");
        colorTexto(IDC_G_AVISO_ESTADO, activo ? COLOR_VERDE : COLOR_GRIS);
        // Impresión y marca de agua.
        impresoras_ = impresoras();
        SendDlgItemMessageW(h_, IDC_G_IMPRESORA, CB_RESETCONTENT, 0, 0);
        const std::wstring predeterminada = impresoraPredeterminada();
        comboAgregar(h_, IDC_G_IMPRESORA,
                     "Predeterminada de Windows" + (predeterminada.empty() ? std::string() : " (" + U(predeterminada) + ")"), 0);
        const std::wstring elegida = W(r.config("Impresora"));
        int seleccion = 0;
        for (size_t i = 0; i < impresoras_.size(); ++i) {
            comboAgregar(h_, IDC_G_IMPRESORA, U(impresoras_[i]), static_cast<LPARAM>(i + 1));
            if (impresoras_[i] == elegida) seleccion = static_cast<int>(i + 1);
        }
        comboSeleccionar(h_, IDC_G_IMPRESORA, seleccion);
        ponerTexto(h_, IDC_G_COPIAS, std::to_string(r.configEntero("CopiasComprobante", 1)));
        CheckDlgButton(h_, IDC_G_PDF_COMP, r.config("GuardarPdfComprobante", "1") == "1" ? BST_CHECKED : BST_UNCHECKED);
        comboSeleccionar(h_, IDC_G_MARCA, App::instancia().marcaAgua());

        ponerTexto(h_, IDC_G_RUTA, "Base de datos: " + U(App::instancia().rutas().baseDatos) +
                                       "\nComprobantes y reportes: " + U(App::instancia().rutas().documentos));
    }

    void guardar() {
        const auto apertura = leerMonto(h_, IDC_G_APERTURA);
        const auto numFac = parsearDecimal(texto(h_, IDC_G_NUMFAC));
        const auto numVen = parsearDecimal(texto(h_, IDC_G_NUMVEN));
        const auto dias = parsearDecimal(texto(h_, IDC_G_DIAS));
        const std::string hora = recortar(texto(h_, IDC_G_HORA));
        if (recortar(texto(h_, IDC_G_EMPRESA)).empty()) return mostrarAviso(h_, "Escriba el nombre de la empresa.");
        if (!apertura || *apertura < 0) return mostrarAviso(h_, "El saldo inicial de la caja no es válido.");
        if (!numFac || *numFac < 1) return mostrarAviso(h_, "El primer número de comprobante debe ser 1 o mayor.");
        if (!numVen || *numVen < 1) return mostrarAviso(h_, "El primer número de venta debe ser 1 o mayor.");
        if (!dias || *dias < 0 || *dias > 60) return mostrarAviso(h_, "Los días de anticipación deben estar entre 0 y 60.");
        if (!horaValida(hora)) return mostrarAviso(h_, "La hora del aviso debe tener el formato HH:MM (por ejemplo 07:30).");
        const auto copias = parsearDecimal(texto(h_, IDC_G_COPIAS));
        if (!copias || *copias < 1 || *copias > 5) return mostrarAviso(h_, "Las copias del comprobante deben ser de 1 a 5.");
        if (!autorizar(h_, "cambiar la configuración")) return;
        Repositorio& r = repo();
        const int marcaAnterior = App::instancia().marcaAgua();
        const int marca = static_cast<int>(comboDato(h_, IDC_G_MARCA, MARCA_DIAGONAL));
        const std::string horaAnterior = r.config("HoraAviso", "07:30");
        intentar(h_, [&] {
            r.setConfig("EmpresaNombre", recortar(texto(h_, IDC_G_EMPRESA)));
            r.setConfig("EmpresaRTN", recortar(texto(h_, IDC_G_RTN)));
            r.setConfig("EmpresaDireccion", recortar(texto(h_, IDC_G_DIRECCION)));
            r.setConfig("EmpresaTelefono", recortar(texto(h_, IDC_G_TELEFONO)));
            r.setConfig("SaldoAperturaCaja", formatoMonto(*apertura));
            r.setConfig("NumeroInicialFactura", std::to_string(static_cast<long>(*numFac)));
            r.setConfig("NumeroInicialVenta", std::to_string(static_cast<long>(*numVen)));
            r.setConfig("GuardarPdf", IsDlgButtonChecked(h_, IDC_G_PDF) == BST_CHECKED ? "1" : "0");
            r.setConfig("DiasAnticipacion", std::to_string(static_cast<long>(*dias)));
            r.setConfig("RecordatorioAlAbrir", IsDlgButtonChecked(h_, IDC_G_ALABRIR) == BST_CHECKED ? "1" : "0");
            r.setConfig("HoraAviso", hora);
            r.setConfig("Impresora", U(impresoraElegida()));
            r.setConfig("CopiasComprobante", std::to_string(static_cast<long>(*copias)));
            r.setConfig("GuardarPdfComprobante", IsDlgButtonChecked(h_, IDC_G_PDF_COMP) == BST_CHECKED ? "1" : "0");
            r.setConfig("MarcaAgua", std::to_string(marca));
            if (marca != marcaAnterior) App::instancia().refrescarMarcaAgua();
            if (hora != horaAnterior && avisoWindowsActivo()) {
                std::string error;
                if (!activarAvisoWindows(hora, error)) mostrarError(h_, error);
            }
            mostrarInfo(h_, "Configuración guardada.");
        });
        cargar();
    }

    // Impresora elegida en la lista ("" = la predeterminada de Windows).
    std::wstring impresoraElegida() const {
        const LPARAM i = comboDato(h_, IDC_G_IMPRESORA, 0);
        return (i >= 1 && static_cast<size_t>(i) <= impresoras_.size()) ? impresoras_[static_cast<size_t>(i - 1)]
                                                                         : std::wstring();
    }

    void probarImpresora() {
        const std::wstring impresora = impresoraElegida().empty() ? impresoraPredeterminada() : impresoraElegida();
        const int marca = static_cast<int>(comboDato(h_, IDC_G_MARCA, MARCA_DIAGONAL));
        Lienzo l;
        if (marca == MARCA_DIAGONAL) l.imagen(115, 330, 620, 620, "marca1");
        if (marca == MARCA_SELLO) l.imagen(175, 360, 500, 500, "marca2");
        l.imagen(245, 40, 360, 138, "logo");
        l.texto(50, 220, 750, "Página de prueba", 22, true, 0x4A1E1B, ALINEAR_CENTRO);
        l.texto(50, 270, 750, "Impresora: " + U(impresora), 12, false, 0x1B1B1B, ALINEAR_CENTRO);
        l.texto(50, 295, 750, "Fecha: " + Fecha::hoy().texto() + "   ·   Marca de agua: " + nombreMarcaAgua(marca), 11,
                false, 0x666666, ALINEAR_CENTRO);
        l.rectangulo(50, 40, 750, 1000, SIN_COLOR, 0x999999, 2);
        std::string error;
        CursorEspera espera;
        if (imprimirLienzo(l, impresora, 1, L"Prueba de Beneficio Unimax", std::wstring(), error))
            mostrarInfo(h_, "Se envió una página de prueba a \"" + U(impresora) + "\".");
        else
            mostrarError(h_, error);
    }

    std::vector<std::wstring> impresoras_;
};

}  // namespace

std::unique_ptr<Pagina> crearPaginaConfiguracion() { return std::make_unique<PaginaConfiguracion>(); }

}  // namespace bu::win
