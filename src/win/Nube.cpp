#include "win/Nube.h"

#include <commdlg.h>

#include <algorithm>
#include <map>
#include <set>
#include <thread>

#include "win/App.h"
#include "win/Dialogos.h"
#include "win/GoogleDrive.h"
#include "win/Sesion.h"
#include "win/Tareas.h"
#include "win/Version.h"
#include "win/resource.h"

namespace bu::win {

namespace {

const char* const CARPETA_RAIZ = "Beneficio Unimax";
const char* const ARCHIVO_DATOS = "BeneficioUnimax.accdb";
const char* const ARCHIVO_ESTADO = "estado.json";
const char* const TIPO_ACCESS = "application/msaccess";

std::wstring archivoLocal() { return App::instancia().rutas().datos + L"\\nube.json"; }

std::wstring archivoTemporal(const wchar_t* prefijo) {
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    return std::wstring(dir) + prefijo + std::to_wstring(GetCurrentProcessId()) + L"_" +
           std::to_wstring(GetTickCount64()) + L".accdb";
}

std::string textoHora(Momento m) { return momentoTexto(m, desfaseLocal()); }

void progresoBytes(Avance* a, const std::string& que, long long hecho, long long total) {
    if (!a) return;
    if (total > 0) {
        a->porcentaje(static_cast<int>(hecho * 100 / total));
        a->texto(que + " (" + std::to_string(hecho / 1024) + " de " + std::to_string(total / 1024) + " KB)");
    }
}

// Recorre una carpeta local: devuelve "ruta/relativa" -> clave "ruta|tamaño|fecha".
void recorrer(const std::wstring& base, const std::wstring& rel, std::map<std::string, std::string>& r) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((base + (rel.empty() ? L"" : L"\\" + rel) + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::wstring nombre = fd.cFileName;
        if (nombre == L"." || nombre == L"..") continue;
        if (nombre.rfind(L"~$", 0) == 0 || (nombre.size() > 6 && nombre.substr(nombre.size() - 6) == L".nuevo")) continue;
        const std::wstring sub = rel.empty() ? nombre : rel + L"\\" + nombre;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            recorrer(base, sub, r);
        } else {
            std::string ruta = U(sub);
            for (auto& c : ruta)
                if (c == '\\') c = '/';
            const unsigned long long tam = (static_cast<unsigned long long>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
            const unsigned long long fecha =
                (static_cast<unsigned long long>(fd.ftLastWriteTime.dwHighDateTime) << 32) | fd.ftLastWriteTime.dwLowDateTime;
            r[ruta] = ruta + "|" + std::to_string(tam) + "|" + std::to_string(fecha / 10000000ULL);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

std::string claveArchivo(const std::wstring& ruta, const std::string& rel) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(ruta.c_str(), GetFileExInfoStandard, &d)) return "";
    const unsigned long long tam = (static_cast<unsigned long long>(d.nFileSizeHigh) << 32) | d.nFileSizeLow;
    const unsigned long long fecha =
        (static_cast<unsigned long long>(d.ftLastWriteTime.dwHighDateTime) << 32) | d.ftLastWriteTime.dwLowDateTime;
    return rel + "|" + std::to_string(tam) + "|" + std::to_string(fecha / 10000000ULL);
}

}  // namespace

Momento ahoraUtc() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const unsigned long long t = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    return static_cast<Momento>((t - 116444736000000000ULL) / 10000000ULL);
}

long desfaseLocal() {
    TIME_ZONE_INFORMATION tz{};
    const DWORD r = GetTimeZoneInformation(&tz);
    long bias = tz.Bias;
    if (r == TIME_ZONE_ID_DAYLIGHT) bias += tz.DaylightBias;
    else if (r == TIME_ZONE_ID_STANDARD) bias += tz.StandardBias;
    return -bias * 60;
}

std::string nombreEquipo() {
    wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(buf, &n);
    return U(buf);
}

Nube& Nube::instancia() {
    static Nube n;
    return n;
}

bool Nube::conectada() { return GoogleDrive::instancia().conectado(); }

// ---------------------------------------------------------------- Estado local
void Nube::cargarLocal() {
    std::lock_guard<std::recursive_mutex> l(m_);
    if (localCargado_) return;
    localCargado_ = true;
    std::string datos;
    if (leerArchivo(archivoLocal(), datos)) {
        try {
            local_ = estadoLocalDesdeJson(Json::parsear(datos));
        } catch (...) {
        }
    }
    if (local_.dispositivo.empty()) {
        const std::vector<uint8_t> b = bytesAleatorios(12);
        local_.dispositivo = base64Url(b.data(), b.size());
        guardarLocal();
    }
}

void Nube::guardarLocal() {
    std::lock_guard<std::recursive_mutex> l(m_);
    escribirArchivo(archivoLocal(), aJson(local_).serializar());
}

void Nube::marcarCambio() {
    if (soloConsulta_) return;
    ++cambios_;
    cargarLocal();
    std::lock_guard<std::recursive_mutex> l(m_);
    if (!local_.pendiente) {
        local_.pendiente = true;
        guardarLocal();
    }
}

// ---------------------------------------------------------------- Drive
std::string Nube::carpetaRaiz() {
    std::lock_guard<std::recursive_mutex> l(m_);
    if (idRaiz_.empty()) idRaiz_ = GoogleDrive::instancia().carpeta(CARPETA_RAIZ, "root");
    return idRaiz_;
}

std::string Nube::subcarpeta(const std::string& nombre) {
    static std::map<std::string, std::string> cache;
    std::lock_guard<std::recursive_mutex> l(m_);
    auto it = cache.find(nombre);
    if (it != cache.end()) return it->second;
    const std::string id = GoogleDrive::instancia().carpeta(nombre, carpetaRaiz());
    cache[nombre] = id;
    return id;
}

bool Nube::leerEstado(EstadoNube& e, std::string& idEstado, std::string& idDatos) {
    auto& g = GoogleDrive::instancia();
    const std::string raiz = carpetaRaiz();
    e = EstadoNube{};
    idEstado.clear();
    idDatos.clear();
    if (const auto a = g.buscar(ARCHIVO_ESTADO, raiz)) {
        idEstado = a->id;
        try {
            e = estadoNubeDesdeJson(Json::parsear(g.leer(a->id)));
        } catch (const std::runtime_error&) {
            // estado.json dañado: se reescribe en la próxima subida.
        }
    }
    if (const auto d = g.buscar(ARCHIVO_DATOS, raiz)) idDatos = d->id;
    std::lock_guard<std::recursive_mutex> l(m_);
    idEstado_ = idEstado;
    idDatos_ = idDatos;
    return !idDatos.empty();
}

void Nube::escribirEstado(const EstadoNube& e, const std::string& idEstado) {
    const ArchivoDrive a = GoogleDrive::instancia().subir(aJson(e).serializar(), ARCHIVO_ESTADO, carpetaRaiz(), idEstado,
                                                         "application/json");
    std::lock_guard<std::recursive_mutex> l(m_);
    idEstado_ = a.id;
}

Bloqueo Nube::miBloqueo(Momento desde) {
    Bloqueo b;
    b.dispositivo = local_.dispositivo;
    b.equipo = nombreEquipo();
    const Usuario& u = App::instancia().usuario();
    b.usuario = u.id ? (u.nombre.empty() ? u.usuario : u.nombre) : "";
    b.desde = desde;
    b.latido = ahoraUtc();
    return b;
}

void Nube::tomarBloqueo() {
    EstadoNube e;
    std::string idEstado, idDatos;
    leerEstado(e, idEstado, idDatos);
    const EstadoBloqueo b = evaluarBloqueo(e.bloqueo, local_.dispositivo, ahoraUtc());
    if (b == EstadoBloqueo::Ajeno) {
        tengoBloqueo_ = false;
        return;
    }
    if (b != EstadoBloqueo::Propio || desdeBloqueo_ == 0) desdeBloqueo_ = ahoraUtc();
    e.bloqueo = miBloqueo(desdeBloqueo_);
    escribirEstado(e, idEstado);
    tengoBloqueo_ = true;
    ultimoLatido_ = ahoraUtc();
    if (e.version > local_.versionBase && !idDatos.empty()) nubeMasNueva_ = true;
}

void Nube::subirDatos(const std::wstring& archivo, const std::string& motivo, bool liberar, unsigned long cambiosHasta,
                      Avance* avance) {
    auto& g = GoogleDrive::instancia();
    if (avance) avance->texto("Revisando Google Drive...");
    EstadoNube e;
    std::string idEstado, idDatos;
    const bool hayDatos = leerEstado(e, idEstado, idDatos);
    std::string datos;
    if (!leerArchivo(archivo, datos)) throw std::runtime_error("No se pudo leer la copia de la base de datos.");
    const Momento ahora = ahoraUtc();

    if (hayDatos && e.version > local_.versionBase) {
        // Otra computadora subió datos mientras tanto: no se pisan. Lo de aquí queda como respaldo y,
        // al volver a abrir el programa, se pregunta qué datos usar.
        nubeMasNueva_ = true;
        if (avance) avance->texto("Guardando sus cambios como respaldo aparte...");
        g.subir(datos, nombreRespaldo(ahora, desfaseLocal(), "cambios de " + nombreEquipo() + " sin unir"),
                subcarpeta("Respaldos"), "", TIPO_ACCESS);
        ultimoError_ = "Otra computadora subió datos más nuevos. Sus cambios se guardaron como respaldo aparte; "
                       "al volver a abrir el programa podrá elegir qué datos usar.";
        return;
    }

    const std::string que = "Subiendo la base de datos";
    const ArchivoDrive a = g.subir(datos, ARCHIVO_DATOS, carpetaRaiz(), idDatos, TIPO_ACCESS,
                                   [&](long long h, long long t) { progresoBytes(avance, que, h, t); });
    e.version = std::max(e.version, local_.versionBase) + 1;
    e.dispositivo = local_.dispositivo;
    e.equipo = nombreEquipo();
    const Usuario& u = App::instancia().usuario();
    e.usuario = u.nombre.empty() ? u.usuario : u.nombre;
    e.subido = ahora;
    e.versionPrograma = VERSION_PROGRAMA;
    const EstadoBloqueo b = evaluarBloqueo(e.bloqueo, local_.dispositivo, ahora);
    if (liberar) {
        if (b == EstadoBloqueo::Propio || b == EstadoBloqueo::Vencido) e.bloqueo = Bloqueo{};
        tengoBloqueo_ = false;
    } else if (b != EstadoBloqueo::Ajeno) {
        if (desdeBloqueo_ == 0) desdeBloqueo_ = ahora;
        e.bloqueo = miBloqueo(desdeBloqueo_);
        tengoBloqueo_ = true;
        ultimoLatido_ = ahora;
    }
    if (avance) avance->texto("Guardando el estado...");
    escribirEstado(e, idEstado);
    {
        std::lock_guard<std::recursive_mutex> l(m_);
        idDatos_ = a.id;
        local_.versionBase = e.version;
        local_.ultimaSubida = ahora;
        local_.pendiente = cambios_.load() != cambiosHasta;
        guardarLocal();
    }
    sinInternet_ = false;
    ultimoError_.clear();
    if (!motivo.empty()) {
        if (avance) avance->texto("Guardando el respaldo con fecha...");
        respaldoEnNube(a.id, motivo);
        limpiarRespaldos();
    }
    subirArchivosNuevos(avance);
}

void Nube::respaldoEnNube(const std::string& idDatos, const std::string& motivo) {
    const Momento ahora = ahoraUtc();
    GoogleDrive::instancia().copiar(idDatos, nombreRespaldo(ahora, desfaseLocal(), motivo), subcarpeta("Respaldos"));
    std::lock_guard<std::recursive_mutex> l(m_);
    local_.ultimoRespaldo = ahora;
    guardarLocal();
}

void Nube::limpiarRespaldos() {
    auto& g = GoogleDrive::instancia();
    std::vector<ArchivoRespaldo> lista;
    for (const auto& a : g.listar(subcarpeta("Respaldos")))
        if (!a.carpeta) lista.push_back({a.id, a.creado ? a.creado : a.modificado});
    for (const auto& id : respaldosABorrar(lista, ahoraUtc(), desfaseLocal())) {
        try {
            g.borrar(id);
        } catch (...) {
        }
    }
}

void Nube::subirArchivosNuevos(Avance* avance) {
    auto& g = GoogleDrive::instancia();
    const Rutas& r = App::instancia().rutas();
    std::set<std::string> subidos;
    {
        std::lock_guard<std::recursive_mutex> l(m_);
        subidos.insert(local_.archivosSubidos.begin(), local_.archivosSubidos.end());
    }
    struct Pendiente {
        std::string carpeta, rel, clave;
        std::wstring ruta;
    };
    std::vector<Pendiente> pendientes;
    const std::pair<const char*, std::wstring> bases[] = {{"Documentos", r.documentos}, {"Plantillas", r.plantillas}};
    for (const auto& b : bases) {
        std::map<std::string, std::string> archivos;
        recorrer(b.second, L"", archivos);
        for (const auto& a : archivos) {
            const std::string clave = std::string(b.first) + "/" + a.second;
            if (subidos.count(clave)) continue;
            std::wstring ruta = W(a.first);
            for (auto& c : ruta)
                if (c == L'/') c = L'\\';
            pendientes.push_back({b.first, a.first, clave, b.second + L"\\" + ruta});
        }
    }
    std::map<std::string, std::string> carpetas;  // "Documentos/Facturas" -> id
    size_t n = 0;
    for (const auto& p : pendientes) {
        if (avance) {
            if (avance->cancelado()) break;
            avance->porcentaje(static_cast<int>(n * 100 / pendientes.size()));
            avance->texto("Copiando documentos y plantillas a Google Drive (" + std::to_string(n + 1) + " de " +
                          std::to_string(pendientes.size()) + ")...");
        }
        ++n;
        // Carpeta de destino, creando las que falten.
        std::string padre = subcarpeta(p.carpeta), camino = p.carpeta;
        const size_t ultima = p.rel.rfind('/');
        if (ultima != std::string::npos) {
            size_t i = 0;
            while (i < ultima) {
                const size_t fin = std::min(p.rel.find('/', i), ultima);
                const std::string nombre = p.rel.substr(i, fin - i);
                camino += "/" + nombre;
                auto it = carpetas.find(camino);
                if (it == carpetas.end()) it = carpetas.emplace(camino, g.carpeta(nombre, padre)).first;
                padre = it->second;
                i = fin + 1;
            }
        }
        const std::string nombre = ultima == std::string::npos ? p.rel : p.rel.substr(ultima + 1);
        std::string datos;
        if (!leerArchivo(p.ruta, datos)) continue;
        const auto existente = g.buscar(nombre, padre);
        g.subir(datos, nombre, padre, existente ? existente->id : "", "application/octet-stream");
        std::lock_guard<std::recursive_mutex> l(m_);
        // Se quita la clave vieja del mismo archivo (si cambió) y se pone la nueva.
        const std::string prefijo = p.carpeta + "/" + p.rel + "|";
        auto& lista = local_.archivosSubidos;
        lista.erase(std::remove_if(lista.begin(), lista.end(),
                                   [&](const std::string& k) { return k.rfind(prefijo, 0) == 0; }),
                    lista.end());
        lista.push_back(p.clave);
        if (n % 10 == 0) guardarLocal();
    }
    guardarLocal();
}

void Nube::bajarPlantillasFaltantes() {
    auto& g = GoogleDrive::instancia();
    const std::wstring base = App::instancia().rutas().plantillas;
    // Recorre "Plantillas" en Drive (con sus subcarpetas, por ejemplo "Anteriores\Versión 2").
    std::vector<std::pair<std::string, std::string>> porRevisar{{subcarpeta("Plantillas"), ""}};
    while (!porRevisar.empty()) {
        const auto actual = porRevisar.back();
        porRevisar.pop_back();
        for (const auto& a : g.listar(actual.first)) {
            const std::string rel = actual.second.empty() ? a.nombre : actual.second + "/" + a.nombre;
            if (a.carpeta) {
                porRevisar.push_back({a.id, rel});
                continue;
            }
            std::wstring ruta = W(rel);
            for (auto& c : ruta)
                if (c == L'/') c = L'\\';
            const std::wstring destino = base + L"\\" + ruta;
            if (existeArchivo(destino)) continue;
            crearCarpetas(carpetaDe(destino));
            g.descargar(a.id, destino);
            const std::string clave = claveArchivo(destino, rel);
            std::lock_guard<std::recursive_mutex> l(m_);
            if (!clave.empty()) local_.archivosSubidos.push_back("Plantillas/" + clave);
        }
    }
    guardarLocal();
}

// ---------------------------------------------------------------- Al abrir
void Nube::conectarInteractivo(HWND padre) {
    std::string error;
    const bool ok = ejecutarConProgreso(
        padre, "Conectar con Google Drive", [](Avance& a) { GoogleDrive::instancia().conectar(a); }, error, true);
    if (!ok && !error.empty()) mostrarError(padre, error);
    else if (ok && GoogleDrive::instancia().conectado())
        mostrarInfo(padre, "Listo: esta computadora quedó conectada con Google Drive (" + GoogleDrive::instancia().cuenta() +
                               ").\n\nLos datos se guardan en la carpeta \"Beneficio Unimax\" de su Drive.");
}

void Nube::alIniciar(std::wstring& baseDatos) {
    cargarLocal();
    iniciada_ = true;
    ultimaSubidaHora_ = ahoraUtc();
    auto& g = GoogleDrive::instancia();
    const bool hayLocal = existeArchivo(baseDatos);
    if (!g.conectado()) {
        // Computadora nueva: se ofrece traer los datos de Google Drive antes de crear una base vacía.
        if (hayLocal || !g.cliente().valido()) return;
        const int r = MessageBoxW(
            nullptr,
            L"¿Ya usa Beneficio Unimax en otra computadora y guarda sus datos en Google Drive?\n\n"
            L"Sí: conectar con su cuenta de Google y traer los datos (clientes, facturas, caja, usuarios y "
            L"contraseñas) para seguir trabajando donde lo dejó.\n\n"
            L"No: empezar con datos nuevos (puede conectar Google Drive después, en el menú Nube).",
            L"Beneficio Unimax", MB_YESNO | MB_ICONQUESTION);
        if (r != IDYES) return;
        conectarInteractivo(nullptr);
        if (!g.conectado()) return;
    }

    // ---- 1. Revisar qué hay en la nube
    EstadoNube nube;
    std::string idEstado, idDatos;
    bool hayNube = false;
    std::string error;
    const bool revisado = ejecutarConProgreso(
        nullptr, "Google Drive",
        [&](Avance& a) {
            a.texto("Revisando los datos guardados en Google Drive...");
            hayNube = leerEstado(nube, idEstado, idDatos);
        },
        error, true);
    if (!revisado) {
        sinInternet_ = true;
        ultimoError_ = error;
        if (!error.empty())
            MessageBoxW(nullptr,
                        W("No se pudo revisar Google Drive:\n" + error +
                          "\n\nSe trabajará con los datos de esta computadora. Los cambios se subirán cuando vuelva "
                          "la conexión.\n\nSi usó el programa en otra computadora hace poco, tenga en cuenta que sus "
                          "últimos cambios todavía no están aquí.")
                            .c_str(),
                        L"Beneficio Unimax", MB_ICONWARNING);
        return;
    }

    const Momento ahora = ahoraUtc();
    const auto descargar = [&](const std::wstring& destino, Avance& a) {
        const std::string que = "Descargando los datos de Google Drive";
        a.texto(que + "...");
        GoogleDrive::instancia().descargar(idDatos, destino, [&](long long h, long long t) { progresoBytes(&a, que, h, t); });
    };

    // ---- 2. ¿Está abierto en otra computadora?
    if (hayNube && evaluarBloqueo(nube.bloqueo, local_.dispositivo, ahora) == EstadoBloqueo::Ajeno) {
        soloConsulta_ = true;
        motivoSoloConsulta_ = "los datos están abiertos en la computadora " + nube.bloqueo.equipo +
                              (nube.bloqueo.usuario.empty() ? "" : " (" + nube.bloqueo.usuario + ")") + " desde el " +
                              textoHora(nube.bloqueo.desde);
        MessageBoxW(nullptr,
                    W("Beneficio Unimax está abierto en la computadora " + nube.bloqueo.equipo +
                      (nube.bloqueo.usuario.empty() ? "" : " (usuario " + nube.bloqueo.usuario + ")") + " desde el " +
                      textoHora(nube.bloqueo.desde) +
                      ".\n\nPara no mezclar ni perder datos, aquí se abrirá en modo SOLO CONSULTA con la última copia "
                      "guardada en Google Drive (subida el " + textoHora(nube.subido) +
                      ").\n\nPara trabajar en esta computadora, cierre el programa en " + nube.bloqueo.equipo +
                      " y vuelva a abrirlo aquí.")
                        .c_str(),
                    L"Beneficio Unimax - Solo consulta", MB_ICONINFORMATION);
        const std::wstring carpeta = App::instancia().rutas().datos + L"\\Consulta";
        crearCarpetas(carpeta);
        const std::wstring copia = carpeta + L"\\BeneficioUnimax_consulta.accdb";
        DeleteFileW(copia.c_str());
        ejecutarConProgreso(nullptr, "Google Drive", [&](Avance& a) { descargar(copia, a); }, error, true);
        if (!existeArchivo(copia) && hayLocal) CopyFileW(baseDatos.c_str(), copia.c_str(), FALSE);
        baseDatos = copia;
        return;
    }

    // ---- 3. ¿Bajar, subir o preguntar?
    AccionAlAbrir accion = decidirAlAbrir(local_, hayNube, nube, hayLocal);
    bool guardarLocalComoRespaldo = false;  // conflicto resuelto a favor de la nube
    bool guardarNubeComoRespaldo = false;   // conflicto resuelto a favor de esta computadora
    if (accion == AccionAlAbrir::Conflicto) {
        const int r = MessageBoxW(
            nullptr,
            W("Hay datos distintos en esta computadora y en Google Drive:\n\n"
              "•  En Google Drive: subidos desde " + nube.equipo + (nube.usuario.empty() ? "" : " por " + nube.usuario) +
              " el " + textoHora(nube.subido) +
              ".\n•  En esta computadora: cambios que todavía no se subieron (última subida: " +
              textoHora(local_.ultimaSubida) +
              ").\n\n¿Qué datos desea usar?\n\n"
              "Sí = los de Google Drive (los de esta computadora se guardan como respaldo)\n"
              "No = los de esta computadora (los de Google Drive se guardan como respaldo)\n"
              "Cancelar = salir sin cambiar nada")
                .c_str(),
            L"Beneficio Unimax - ¿Qué datos usar?", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1);
        if (r == IDCANCEL) ExitProcess(0);
        if (r == IDYES) {
            accion = AccionAlAbrir::Descargar;
            guardarLocalComoRespaldo = true;
        } else {
            accion = AccionAlAbrir::Subir;
            guardarNubeComoRespaldo = true;
        }
    }

    const bool hecho = ejecutarConProgreso(
        nullptr, "Google Drive",
        [&](Avance& a) {
            auto& gd = GoogleDrive::instancia();
            if (accion == AccionAlAbrir::Descargar) {
                if (guardarLocalComoRespaldo) {
                    a.texto("Guardando los datos de esta computadora como respaldo...");
                    std::string datos;
                    if (leerArchivo(baseDatos, datos))
                        gd.subir(datos, nombreRespaldo(ahoraUtc(), desfaseLocal(), "datos de " + nombreEquipo() + " antes de bajar"),
                                 subcarpeta("Respaldos"), "", TIPO_ACCESS);
                }
                const std::wstring temporal = baseDatos + L".descarga";
                descargar(temporal, a);
                if (existeArchivo(baseDatos)) {
                    const std::wstring copia = App::instancia().rutas().respaldos + L"\\" +
                                               W(nombreRespaldo(ahoraUtc(), desfaseLocal(), "antes de bajar de Drive"));
                    MoveFileExW(baseDatos.c_str(), copia.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
                }
                if (!MoveFileExW(temporal.c_str(), baseDatos.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
                    throw std::runtime_error("No se pudo poner la base de datos descargada.");
                std::lock_guard<std::recursive_mutex> l(m_);
                local_.versionBase = nube.version;
                local_.pendiente = false;
                guardarLocal();
            } else if (accion == AccionAlAbrir::Subir || accion == AccionAlAbrir::PrimeraSubida) {
                if (guardarNubeComoRespaldo && !idDatos.empty()) {
                    a.texto("Guardando los datos de Google Drive como respaldo...");
                    respaldoEnNube(idDatos, "reemplazados por " + nombreEquipo());
                }
                {
                    std::lock_guard<std::recursive_mutex> l(m_);
                    local_.versionBase = std::max(local_.versionBase, nube.version);
                }
                subirDatos(baseDatos, accion == AccionAlAbrir::PrimeraSubida ? "primera copia" : "", false,
                           cambios_.load(), &a);
            }
            a.porcentaje(-1);
            a.texto("Reservando los datos para esta computadora...");
            tomarBloqueo();
            a.texto("Revisando plantillas...");
            bajarPlantillasFaltantes();
        },
        error, false);
    if (!hecho && !error.empty()) {
        sinInternet_ = true;
        ultimoError_ = error;
        MessageBoxW(nullptr,
                    W("No se pudo terminar la sincronización con Google Drive:\n" + error +
                      "\n\nSe trabajará con los datos de esta computadora.")
                        .c_str(),
                    L"Beneficio Unimax", MB_ICONWARNING);
    }
}

// ---------------------------------------------------------------- Mientras se trabaja
std::wstring Nube::instantanea() {
    const std::wstring ruta = archivoTemporal(L"BU_subida_");
    if (!App::instancia().instantanea(ruta)) return L"";
    return ruta;
}

void Nube::enSegundoPlano(const std::string& motivo, bool conCopia) {
    if (soloConsulta_ || !conectada()) return;
    bool libre = false;
    if (!ocupado_.compare_exchange_strong(libre, true)) {
        if (conCopia) respaldoPendiente_ = motivo;  // se intenta en el próximo minuto
        return;
    }
    const unsigned long cambiosAntes = cambios_.load();
    const std::wstring archivo = instantanea();
    if (archivo.empty()) {
        ocupado_ = false;
        return;
    }
    HWND ventana = App::instancia().ventana;
    std::thread([this, archivo, motivo, conCopia, cambiosAntes, ventana] {
        try {
            subirDatos(archivo, conCopia ? motivo : "", false, cambiosAntes, nullptr);
            if (conCopia) {
                std::lock_guard<std::recursive_mutex> l(m_);
                if (respaldoPendiente_ == motivo) respaldoPendiente_.clear();
            }
        } catch (const ErrorRed& e) {
            sinInternet_ = true;
            std::lock_guard<std::recursive_mutex> l(m_);
            ultimoError_ = e.what();
            if (conCopia) respaldoPendiente_ = motivo;
        } catch (const std::exception& e) {
            std::lock_guard<std::recursive_mutex> l(m_);
            ultimoError_ = e.what();
        }
        DeleteFileW(archivo.c_str());
        ocupado_ = false;
        if (ventana) PostMessageW(ventana, WM_APP_NUBE, 0, 0);
    }).detach();
}

void Nube::respaldoCierreCaja() { enSegundoPlano("cierre de caja", true); }

void Nube::cadaMinuto() {
    if (!iniciada_ || soloConsulta_ || !conectada()) return;
    // Con una ventana abierta encima (guardando algo, pidiendo datos) no se toca la base de datos.
    HWND principal = App::instancia().ventana;
    const bool hayVentanaEncima = principal && !IsWindowEnabled(principal);
    const Momento ahora = ahoraUtc();
    std::string pendiente;
    {
        std::lock_guard<std::recursive_mutex> l(m_);
        pendiente = respaldoPendiente_;
    }
    if (!pendiente.empty() && !hayVentanaEncima) {
        enSegundoPlano(pendiente, true);
        return;
    }
    if (local_.pendiente && ahora - ultimaSubidaHora_ >= 3600 && !hayVentanaEncima) {
        ultimaSubidaHora_ = ahora;
        enSegundoPlano("cada hora", true);
        return;
    }
    if (ahora - ultimoLatido_ < LATIDO_SEGUNDOS) return;
    // Latido: "este equipo sigue trabajando con los datos".
    bool libre = false;
    if (!ocupado_.compare_exchange_strong(libre, true)) return;
    ultimoLatido_ = ahora;
    HWND ventana = App::instancia().ventana;
    const bool teniaBloqueo = tengoBloqueo_.load();
    std::thread([this, ventana, teniaBloqueo] {
        int aviso = 0;
        try {
            EstadoNube e;
            std::string idEstado, idDatos;
            leerEstado(e, idEstado, idDatos);
            const EstadoBloqueo b = evaluarBloqueo(e.bloqueo, local_.dispositivo, ahoraUtc());
            if (b == EstadoBloqueo::Ajeno) {
                if (teniaBloqueo) aviso = 1;  // otra computadora tomó los datos
                tengoBloqueo_ = false;
            } else {
                if (b != EstadoBloqueo::Propio) desdeBloqueo_ = ahoraUtc();
                e.bloqueo = miBloqueo(desdeBloqueo_);
                escribirEstado(e, idEstado);
                tengoBloqueo_ = true;
                if (e.version > local_.versionBase && !idDatos.empty()) nubeMasNueva_ = true;
            }
            sinInternet_ = false;
        } catch (const ErrorRed& e) {
            sinInternet_ = true;
            std::lock_guard<std::recursive_mutex> l(m_);
            ultimoError_ = e.what();
        } catch (const std::exception& e) {
            std::lock_guard<std::recursive_mutex> l(m_);
            ultimoError_ = e.what();
        }
        ocupado_ = false;
        if (ventana) PostMessageW(ventana, WM_APP_NUBE, static_cast<WPARAM>(aviso), 0);
    }).detach();
}

void Nube::perdioBloqueo(HWND padre) {
    if (soloConsulta_) return;
    // Se guardan los cambios de aquí como respaldo aparte y el equipo pasa a solo consulta.
    const std::wstring archivo = instantanea();
    std::string error;
    ejecutarConProgreso(
        padre, "Google Drive",
        [&](Avance& a) {
            if (archivo.empty()) return;
            a.texto("Guardando sus cambios como respaldo aparte...");
            std::string datos;
            if (leerArchivo(archivo, datos))
                GoogleDrive::instancia().subir(datos,
                                               nombreRespaldo(ahoraUtc(), desfaseLocal(), "cambios de " + nombreEquipo() + " sin unir"),
                                               subcarpeta("Respaldos"), "", TIPO_ACCESS);
        },
        error, false);
    if (!archivo.empty()) DeleteFileW(archivo.c_str());
    soloConsulta_ = true;
    motivoSoloConsulta_ = "otra computadora tomó los datos mientras este equipo estaba sin conexión";
    mostrarAviso(padre, "Otra computadora abrió los datos mientras este equipo estuvo más de 20 minutos sin conexión "
                        "a Internet.\n\nPara no mezclar datos, este equipo pasa a SOLO CONSULTA. Sus cambios sin subir "
                        "se guardaron como respaldo en Google Drive (carpeta Respaldos)" +
                            std::string(error.empty() ? "." : ", pero hubo un problema: " + error) +
                            "\n\nAl volver a abrir el programa podrá elegir qué datos usar.");
    {
        std::lock_guard<std::recursive_mutex> l(m_);
        local_.pendiente = true;  // para que al abrir se pregunte
        guardarLocal();
    }
}

void Nube::esperarLibre() {
    // Espera (atendiendo la ventana) a que termine una subida en segundo plano.
    for (int i = 0; i < 1200 && ocupado_.load(); ++i) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(50);
    }
}

void Nube::alSalir(HWND padre, const std::string& motivo) {
    if (!iniciada_ || soloConsulta_ || !conectada()) return;
    esperarLibre();
    ocupado_ = true;
    const bool hayCambios = local_.pendiente;
    const unsigned long cambiosAntes = cambios_.load();
    const std::wstring archivo = hayCambios ? instantanea() : L"";
    std::string error;
    const bool ok = ejecutarConProgreso(
        padre, "Guardando en Google Drive",
        [&](Avance& a) {
            if (!archivo.empty()) {
                subirDatos(archivo, motivo, true, cambiosAntes, &a);
                return;
            }
            a.texto("Liberando los datos para otras computadoras...");
            EstadoNube e;
            std::string idEstado, idDatos;
            leerEstado(e, idEstado, idDatos);
            if (evaluarBloqueo(e.bloqueo, local_.dispositivo, ahoraUtc()) == EstadoBloqueo::Propio) {
                e.bloqueo = Bloqueo{};
                escribirEstado(e, idEstado);
            }
            tengoBloqueo_ = false;
            subirArchivosNuevos(&a);
        },
        error, true);
    if (!archivo.empty()) DeleteFileW(archivo.c_str());
    ocupado_ = false;
    if (!ok && !error.empty() && hayCambios)
        MessageBoxW(padre,
                    W("No se pudieron subir los cambios a Google Drive:\n" + error +
                      "\n\nNo se perdió nada: quedan guardados en esta computadora y se subirán la próxima vez que "
                      "abra el programa con Internet.")
                        .c_str(),
                    L"Beneficio Unimax", MB_ICONWARNING);
    else if (ok && !ultimoError_.empty() && nubeMasNueva_)
        MessageBoxW(padre, W(ultimoError_).c_str(), L"Beneficio Unimax", MB_ICONWARNING);
}

std::string Nube::resumen() {
    if (!conectada()) return "";
    if (soloConsulta_) return "Nube: SOLO CONSULTA";
    if (nubeMasNueva_) return "Nube: hay datos más nuevos de otra computadora (cierre y abra el programa)";
    if (sinInternet_) return local_.pendiente ? "Nube: sin conexión (los cambios se subirán después)" : "Nube: sin conexión";
    if (local_.pendiente) return "Nube: cambios por subir (última subida " + textoHora(local_.ultimaSubida) + ")";
    return "Nube: al día (" + textoHora(local_.ultimaSubida) + ")";
}

// ---------------------------------------------------------------- Menú "Nube"
void Nube::sincronizarAhora(HWND padre) {
    if (!conectada()) {
        dialogoNube(padre);
        return;
    }
    if (soloConsulta_) {
        mostrarAviso(padre, "Este equipo está en SOLO CONSULTA: " + motivoSoloConsulta_ + ".");
        return;
    }
    esperarLibre();
    ocupado_ = true;
    const unsigned long cambiosAntes = cambios_.load();
    const std::wstring archivo = instantanea();
    std::string error;
    const bool ok = ejecutarConProgreso(
        padre, "Guardando en Google Drive",
        [&](Avance& a) {
            if (archivo.empty()) throw std::runtime_error("No se pudo copiar la base de datos.");
            subirDatos(archivo, "guardado a mano", false, cambiosAntes, &a);
        },
        error, true);
    if (!archivo.empty()) DeleteFileW(archivo.c_str());
    ocupado_ = false;
    ultimaSubidaHora_ = ahoraUtc();
    if (App::instancia().ventana) PostMessageW(App::instancia().ventana, WM_APP_NUBE, 0, 0);
    if (!ok) {
        if (!error.empty()) mostrarError(padre, "No se pudo guardar en Google Drive:\n" + error);
        return;
    }
    if (nubeMasNueva_) mostrarAviso(padre, ultimoError_);
    else mostrarInfo(padre, "Listo: los datos quedaron guardados en Google Drive con un respaldo de este momento.");
}

namespace {

class DialogoNube : public DialogoModal {
protected:
    void alIniciar() override { refrescar(); }

    bool alComando(int id, int) override {
        auto& g = GoogleDrive::instancia();
        switch (id) {
            case IDC_N_CONECTAR:
                if (g.conectado()) {
                    if (!confirmar(h_, "¿Desconectar esta computadora de Google Drive?\n\nLos datos que ya están en Drive "
                                       "no se borran, pero ya no se subirán respaldos desde aquí."))
                        return true;
                    if (!autorizar(h_, "desconectar Google Drive")) return true;
                    Nube::instancia().alSalir(h_, "al desconectar");
                    g.desconectar();
                } else {
                    guardarCliente(false);
                    std::string error;
                    const bool ok = ejecutarConProgreso(
                        h_, "Conectar con Google Drive", [](Avance& a) { GoogleDrive::instancia().conectar(a); }, error,
                        true);
                    if (!ok && !error.empty()) mostrarError(h_, error);
                    if (ok && g.conectado()) {
                        mostrarInfo(h_, "Listo: conectado con " + g.cuenta() +
                                            ".\n\nAhora se guardará una copia de los datos en Google Drive.");
                        Nube::instancia().despuesDeConectar(h_);
                    }
                }
                refrescar();
                return true;
            case IDC_N_SINCRONIZAR:
                Nube::instancia().sincronizarAhora(h_);
                refrescar();
                return true;
            case IDC_N_RESPALDOS:
                Nube::instancia().dialogoRespaldos(h_);
                refrescar();
                return true;
            case IDC_N_GUARDAR:
                guardarCliente(true);
                return true;
            default:
                return false;
        }
    }

private:
    void guardarCliente(bool avisar) {
        auto& g = GoogleDrive::instancia();
        const ClienteGoogle actual = g.cliente();
        const ClienteGoogle nuevo{texto(h_, IDC_N_CLIENTE), texto(h_, IDC_N_SECRETO)};
        if (nuevo.id == actual.id && nuevo.secreto == actual.secreto) {
            if (avisar) mostrarInfo(h_, "No hay cambios.");
            return;
        }
        g.guardarCliente(nuevo);
        if (avisar) mostrarInfo(h_, "Datos de la aplicación de Google guardados en esta computadora.");
    }

    void refrescar() {
        auto& g = GoogleDrive::instancia();
        Nube& n = Nube::instancia();
        std::string t;
        if (g.conectado()) {
            t = "Conectado con Google Drive: " + (g.cuenta().empty() ? std::string("(cuenta de Google)") : g.cuenta()) +
                "\nCarpeta en Drive: \"Beneficio Unimax\"\nEste equipo: " + nombreEquipo() + "\n" + n.resumen();
            if (!n.ultimoError().empty()) t += "\nÚltimo problema: " + n.ultimoError();
        } else {
            t = "Esta computadora no está conectada con Google Drive.\n\nPresione \"Conectar con Google Drive\": se abrirá "
                "el navegador para elegir su cuenta de Google y permitir que Beneficio Unimax guarde sus datos en la "
                "carpeta \"Beneficio Unimax\" de su Drive.";
        }
        ponerTexto(h_, IDC_N_ESTADO, t);
        ponerTexto(h_, IDC_N_CONECTAR, g.conectado() ? "Desconectar" : "Conectar con Google Drive");
        habilitar(h_, IDC_N_SINCRONIZAR, g.conectado());
        habilitar(h_, IDC_N_RESPALDOS, g.conectado());
        const ClienteGoogle c = g.cliente();
        ponerTexto(h_, IDC_N_CLIENTE, c.id);
        ponerTexto(h_, IDC_N_SECRETO, c.secreto);
    }
};

struct FilaRespaldo {
    ArchivoDrive archivo;
};

class DialogoRespaldos : public DialogoModal {
protected:
    void alIniciar() override {
        lvConfigurar(h_, IDC_RN_LISTA, {{L"Fecha", 90}, {L"Respaldo", 230}, {L"Tamaño", 60, true}});
        cargar();
    }

    bool alComando(int id, int) override {
        if (id == IDC_RN_RESTAURAR) {
            restaurar();
            return true;
        }
        if (id == IDC_RN_DESCARGAR) {
            descargarCopia();
            return true;
        }
        return false;
    }

private:
    void cargar() {
        std::string error;
        std::vector<ArchivoDrive> lista;
        if (!ejecutarConProgreso(
                h_, "Google Drive",
                [&](Avance& a) {
                    a.texto("Leyendo los respaldos guardados en Google Drive...");
                    lista = GoogleDrive::instancia().listar(Nube::instancia().idRespaldos());
                },
                error, true)) {
            if (!error.empty()) mostrarError(h_, error);
            return;
        }
        std::sort(lista.begin(), lista.end(), [](const ArchivoDrive& a, const ArchivoDrive& b) {
            return (a.creado ? a.creado : a.modificado) > (b.creado ? b.creado : b.modificado);
        });
        archivos_.clear();
        lvLimpiar(h_, IDC_RN_LISTA);
        for (const auto& a : lista) {
            if (a.carpeta) continue;
            archivos_.push_back(a);
            lvAgregar(h_, IDC_RN_LISTA,
                      {momentoTexto(a.creado ? a.creado : a.modificado, desfaseLocal()), a.nombre,
                       std::to_string((a.tamano + 1023) / 1024) + " KB"},
                      static_cast<LPARAM>(archivos_.size()));
        }
        if (archivos_.empty()) ponerTexto(h_, IDC_RN_INFO, "Todavía no hay respaldos en Google Drive.");
    }

    const ArchivoDrive* seleccionado() {
        const LPARAM i = lvDatoSeleccionado(h_, IDC_RN_LISTA, 0);
        if (i <= 0 || static_cast<size_t>(i) > archivos_.size()) {
            mostrarAviso(h_, "Seleccione un respaldo de la lista.");
            return nullptr;
        }
        return &archivos_[static_cast<size_t>(i - 1)];
    }

    void descargarCopia() {
        const ArchivoDrive* a = seleccionado();
        if (!a) return;
        std::wstring nombre = W(a->nombre);
        for (auto& c : nombre)
            if (c == L':' || c == L'/' || c == L'\\') c = L'-';
        std::wstring archivo(MAX_PATH, L'\0');
        nombre.copy(&archivo[0], std::min<size_t>(nombre.size(), MAX_PATH - 1));
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof ofn;
        ofn.hwndOwner = h_;
        ofn.lpstrFilter = L"Base de datos de Access (*.accdb)\0*.accdb\0";
        ofn.lpstrFile = &archivo[0];
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = App::instancia().rutas().respaldos.c_str();
        ofn.lpstrDefExt = L"accdb";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameW(&ofn)) return;
        const std::wstring destino = archivo.c_str();
        const std::string id = a->id;
        std::string error;
        if (ejecutarConProgreso(
                h_, "Google Drive",
                [&](Avance& av) {
                    av.texto("Descargando el respaldo...");
                    GoogleDrive::instancia().descargar(id, destino, [&](long long h, long long t) {
                        progresoBytes(&av, "Descargando el respaldo", h, t);
                    });
                },
                error, true))
            mostrarInfo(h_, "Respaldo guardado en:\n" + U(destino));
        else if (!error.empty())
            mostrarError(h_, error);
    }

    void restaurar() {
        const ArchivoDrive* a = seleccionado();
        if (!a) return;
        if (Nube::instancia().soloConsulta()) {
            mostrarAviso(h_, "Este equipo está en SOLO CONSULTA: no se puede restaurar desde aquí.");
            return;
        }
        if (!confirmar(h_, "¿Restaurar el respaldo \"" + a->nombre +
                               "\"?\n\nTodos los datos volverán a como estaban en ese momento (en esta y en las demás "
                               "computadoras). Los datos actuales se guardan antes como respaldo, así que se puede "
                               "deshacer restaurando ese respaldo."))
            return;
        if (!autorizar(h_, "restaurar un respaldo")) return;
        std::string error;
        if (Nube::instancia().restaurar(h_, a->id, error)) {
            mostrarInfo(h_, "Listo: se restauró el respaldo. Ingrese de nuevo con su usuario.");
            EndDialog(h_, IDOK);
            if (App::instancia().ventana) PostMessageW(App::instancia().ventana, WM_COMMAND, IDM_CERRAR_SESION, 0);
        } else if (!error.empty()) {
            mostrarError(h_, "No se pudo restaurar el respaldo:\n" + error);
        }
    }

    std::vector<ArchivoDrive> archivos_;
};

}  // namespace

std::string Nube::idRespaldos() { return subcarpeta("Respaldos"); }

std::string Nube::ultimoError() {
    std::lock_guard<std::recursive_mutex> l(m_);
    return ultimoError_;
}

void Nube::despuesDeConectar(HWND padre) {
    // Primera conexión con el programa ya abierto: se sube lo de aquí (o se avisa si Drive ya tiene datos).
    cargarLocal();
    iniciada_ = true;
    EstadoNube nube;
    std::string idEstado, idDatos, error;
    bool hayNube = false;
    if (!ejecutarConProgreso(padre, "Google Drive", [&](Avance& a) {
            a.texto("Revisando Google Drive...");
            hayNube = leerEstado(nube, idEstado, idDatos);
        }, error, true))
        return;
    if (hayNube && nube.version > local_.versionBase) {
        mostrarAviso(padre, "Su Google Drive ya tiene datos de Beneficio Unimax (subidos desde " + nube.equipo + " el " +
                                textoHora(nube.subido) +
                                ").\n\nCierre y vuelva a abrir el programa para elegir qué datos usar.");
        nubeMasNueva_ = true;
        return;
    }
    marcarCambio();
    sincronizarAhora(padre);
}

bool Nube::restaurar(HWND padre, const std::string& idRespaldo, std::string& error) {
    esperarLibre();
    ocupado_ = true;
    const std::wstring descargado = archivoTemporal(L"BU_restaurar_");
    const std::wstring actual = instantanea();
    const bool bajado = ejecutarConProgreso(
        padre, "Restaurar respaldo",
        [&](Avance& a) {
            auto& g = GoogleDrive::instancia();
            if (!actual.empty()) {
                a.texto("Guardando los datos actuales como respaldo...");
                std::string datos;
                if (leerArchivo(actual, datos))
                    g.subir(datos, nombreRespaldo(ahoraUtc(), desfaseLocal(), "antes de restaurar"), subcarpeta("Respaldos"),
                            "", TIPO_ACCESS);
            }
            a.texto("Descargando el respaldo...");
            g.descargar(idRespaldo, descargado, [&](long long h, long long t) { progresoBytes(&a, "Descargando el respaldo", h, t); });
        },
        error, true);
    if (!actual.empty()) DeleteFileW(actual.c_str());
    if (!bajado) {
        DeleteFileW(descargado.c_str());
        ocupado_ = false;
        return false;
    }
    const std::wstring copia = App::instancia().rutas().respaldos + L"\\" +
                               W(nombreRespaldo(ahoraUtc(), desfaseLocal(), "antes de restaurar"));
    if (!App::instancia().reemplazarBase(descargado, copia, error)) {
        ocupado_ = false;
        return false;
    }
    // El respaldo restaurado pasa a ser la versión actual en Drive.
    marcarCambio();
    const unsigned long cambiosAntes = cambios_.load();
    const std::wstring archivo = instantanea();
    std::string e2;
    ejecutarConProgreso(padre, "Google Drive", [&](Avance& a) { subirDatos(archivo, "", false, cambiosAntes, &a); }, e2, false);
    if (!archivo.empty()) DeleteFileW(archivo.c_str());
    ocupado_ = false;
    if (!e2.empty())
        mostrarAviso(padre, "El respaldo se restauró en esta computadora, pero no se pudo subir a Google Drive: " + e2 +
                                "\nSe subirá más tarde.");
    return true;
}

void Nube::dialogoNube(HWND padre) {
    DialogoNube d;
    d.mostrar(padre, IDD_NUBE);
    if (App::instancia().ventana) PostMessageW(App::instancia().ventana, WM_APP_NUBE, 0, 0);
}

void Nube::dialogoRespaldos(HWND padre) {
    if (!conectada()) {
        mostrarAviso(padre, "Primero conecte esta computadora con Google Drive (menú Nube → Google Drive...).");
        return;
    }
    DialogoRespaldos d;
    d.mostrar(padre, IDD_RESPALDOS_NUBE);
}

}  // namespace bu::win
