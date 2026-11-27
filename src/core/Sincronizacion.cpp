#include "core/Sincronizacion.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>

#include "core/Fecha.h"
#include "core/Seguridad.h"

namespace bu {

namespace {
constexpr Momento DIA = 86400;

void partes(Momento m, Fecha& f, int& h, int& mi, int& s) {
    long dias = static_cast<long>(m / DIA);
    Momento resto = m % DIA;
    if (resto < 0) {
        resto += DIA;
        --dias;
    }
    f = Fecha::desdeNumeroDia(dias);
    h = static_cast<int>(resto / 3600);
    mi = static_cast<int>(resto % 3600 / 60);
    s = static_cast<int>(resto % 60);
}

long diaLocal(Momento m, long desfase) {
    const Momento x = m + desfase;
    return static_cast<long>(x >= 0 ? x / DIA : (x - DIA + 1) / DIA);
}
}  // namespace

Momento momentoDesdeRfc3339(const std::string& s) {
    int a = 0, me = 0, d = 0, h = 0, mi = 0;
    double se = 0;
    if (s.size() < 19 || std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%lf", &a, &me, &d, &h, &mi, &se) < 5) return 0;
    const Fecha f(a, me, d);
    if (!f.valida()) return 0;
    Momento m = static_cast<Momento>(f.numeroDia()) * DIA + h * 3600 + mi * 60 + static_cast<Momento>(se);
    // Zona: "Z", "+hh:mm" o "-hh:mm" después de los segundos.
    const size_t pos = s.find_first_of("Z+-", 19);
    if (pos != std::string::npos && s[pos] != 'Z') {
        int zh = 0, zm = 0;
        if (std::sscanf(s.c_str() + pos + 1, "%2d:%2d", &zh, &zm) >= 1) {
            const Momento desf = zh * 3600 + zm * 60;
            m += (s[pos] == '+') ? -desf : desf;
        }
    }
    return m;
}

std::string momentoRfc3339(Momento m) {
    Fecha f;
    int h, mi, s;
    partes(m, f, h, mi, s);
    char buf[40];
    std::snprintf(buf, sizeof buf, "%sT%02d:%02d:%02dZ", f.iso().c_str(), h, mi, s);
    return buf;
}

std::string momentoTexto(Momento m, long desfase) {
    if (m <= 0) return "(nunca)";
    Fecha f;
    int h, mi, s;
    partes(m + desfase, f, h, mi, s);
    char buf[16];
    std::snprintf(buf, sizeof buf, " %02d:%02d", h, mi);
    return f.texto() + buf;
}

EstadoBloqueo evaluarBloqueo(const Bloqueo& b, const std::string& miDispositivo, Momento ahora) {
    if (b.vacio()) return EstadoBloqueo::Libre;
    if (b.dispositivo == miDispositivo) return EstadoBloqueo::Propio;
    if (ahora - b.latido > BLOQUEO_VENCE_SEGUNDOS) return EstadoBloqueo::Vencido;
    return EstadoBloqueo::Ajeno;
}

Json aJson(const EstadoNube& e) {
    Json j = Json::objeto();
    j["version"] = Json(e.version);
    j["dispositivo"] = e.dispositivo;
    j["equipo"] = e.equipo;
    j["usuario"] = e.usuario;
    j["subido"] = momentoRfc3339(e.subido);
    j["versionPrograma"] = Json(e.versionPrograma);
    Json b = Json::objeto();
    if (!e.bloqueo.vacio()) {
        b["dispositivo"] = e.bloqueo.dispositivo;
        b["equipo"] = e.bloqueo.equipo;
        b["usuario"] = e.bloqueo.usuario;
        b["desde"] = momentoRfc3339(e.bloqueo.desde);
        b["latido"] = momentoRfc3339(e.bloqueo.latido);
    }
    j["abiertoEn"] = b;
    return j;
}

EstadoNube estadoNubeDesdeJson(const Json& j) {
    EstadoNube e;
    e.version = j["version"].entero();
    e.dispositivo = j["dispositivo"].texto();
    e.equipo = j["equipo"].texto();
    e.usuario = j["usuario"].texto();
    e.subido = momentoDesdeRfc3339(j["subido"].texto());
    e.versionPrograma = static_cast<int>(j["versionPrograma"].entero());
    const Json& b = j["abiertoEn"];
    e.bloqueo.dispositivo = b["dispositivo"].texto();
    e.bloqueo.equipo = b["equipo"].texto();
    e.bloqueo.usuario = b["usuario"].texto();
    e.bloqueo.desde = momentoDesdeRfc3339(b["desde"].texto());
    e.bloqueo.latido = momentoDesdeRfc3339(b["latido"].texto());
    return e;
}

Json aJson(const EstadoLocal& e) {
    Json j = Json::objeto();
    j["dispositivo"] = e.dispositivo;
    j["versionBase"] = Json(e.versionBase);
    j["pendiente"] = Json(e.pendiente);
    j["ultimaSubida"] = momentoRfc3339(e.ultimaSubida);
    j["ultimoRespaldo"] = momentoRfc3339(e.ultimoRespaldo);
    Json lista = Json::lista();
    for (const auto& a : e.archivosSubidos) lista.agregar(a);
    j["archivosSubidos"] = lista;
    return j;
}

EstadoLocal estadoLocalDesdeJson(const Json& j) {
    EstadoLocal e;
    e.dispositivo = j["dispositivo"].texto();
    e.versionBase = j["versionBase"].entero();
    e.pendiente = j["pendiente"].booleano();
    e.ultimaSubida = momentoDesdeRfc3339(j["ultimaSubida"].texto());
    e.ultimoRespaldo = momentoDesdeRfc3339(j["ultimoRespaldo"].texto());
    for (const auto& a : j["archivosSubidos"].elementos()) e.archivosSubidos.push_back(a.texto());
    return e;
}

AccionAlAbrir decidirAlAbrir(const EstadoLocal& local, bool hayDatosEnNube, const EstadoNube& nube,
                             bool hayDatosLocales) {
    if (!hayDatosEnNube) return hayDatosLocales ? AccionAlAbrir::PrimeraSubida : AccionAlAbrir::Nada;
    if (!hayDatosLocales) return AccionAlAbrir::Descargar;
    if (nube.version > local.versionBase) {
        // Esta computadora nunca se sincronizó (versión 0) pero ya tiene datos: no se pisa nada sin preguntar.
        if (local.pendiente || local.versionBase == 0) return AccionAlAbrir::Conflicto;
        return AccionAlAbrir::Descargar;
    }
    return local.pendiente ? AccionAlAbrir::Subir : AccionAlAbrir::Nada;
}

std::vector<std::string> respaldosABorrar(std::vector<ArchivoRespaldo> respaldos, Momento ahora, long desfase) {
    std::sort(respaldos.begin(), respaldos.end(),
              [](const ArchivoRespaldo& a, const ArchivoRespaldo& b) { return a.momento > b.momento; });
    std::set<long> dias;            // días que ya tienen su respaldo (el más reciente)
    std::set<long> meses;           // año*12+mes que ya tienen el suyo
    std::vector<std::string> borrar;
    const long hoy = diaLocal(ahora, desfase);
    for (size_t i = 0; i < respaldos.size(); ++i) {
        const auto& r = respaldos[i];
        const long dia = diaLocal(r.momento, desfase);
        const Fecha f = Fecha::desdeNumeroDia(dia);
        const long mes = f.anio * 12L + f.mes;
        bool conservar = i < 10 || ahora - r.momento <= 2 * DIA;
        if (!conservar && hoy - dia <= 30) conservar = dias.insert(dia).second;
        else if (!conservar) conservar = meses.insert(mes).second;
        // El último de cada día o mes cuenta aunque se haya conservado por otra regla.
        dias.insert(dia);
        if (hoy - dia > 30) meses.insert(mes);
        if (!conservar) borrar.push_back(r.id);
    }
    return borrar;
}

std::string nombreRespaldo(Momento m, long desfase, const std::string& motivo) {
    Fecha f;
    int h, mi, s;
    partes(m + desfase, f, h, mi, s);
    char buf[16];
    std::snprintf(buf, sizeof buf, " %02d.%02d", h, mi);
    return "Respaldo " + f.iso() + buf + (motivo.empty() ? "" : " (" + motivo + ")") + ".accdb";
}

InfoVersion parsearInfoVersion(const std::string& texto) {
    InfoVersion v;
    size_t i = 0;
    bool enNotas = false;
    while (i <= texto.size()) {
        size_t fin = texto.find('\n', i);
        if (fin == std::string::npos) fin = texto.size();
        std::string linea = texto.substr(i, fin - i);
        if (!linea.empty() && linea.back() == '\r') linea.pop_back();
        i = fin + 1;
        if (enNotas) {
            v.notas += (v.notas.empty() ? "" : "\n") + linea;
            continue;
        }
        if (linea == "---") {
            enNotas = true;
            continue;
        }
        const size_t igual = linea.find('=');
        if (igual == std::string::npos) continue;
        const std::string clave = linea.substr(0, igual), valor = linea.substr(igual + 1);
        if (clave == "version") v.numero = std::atoi(valor.c_str());
        else if (clave == "sha256") {
            v.sha256 = valor;
            for (auto& c : v.sha256) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (clave == "tamano") v.tamano = std::atoll(valor.c_str());
    }
    while (!v.notas.empty() && (v.notas.back() == '\n' || v.notas.back() == ' ')) v.notas.pop_back();
    return v;
}

namespace {
std::string base64Con(const uint8_t* d, size_t n, const char* alfabeto, bool relleno) {
    std::string r;
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t b = (static_cast<uint32_t>(d[i]) << 16) | (i + 1 < n ? static_cast<uint32_t>(d[i + 1]) << 8 : 0) |
                           (i + 2 < n ? d[i + 2] : 0);
        r += alfabeto[(b >> 18) & 63];
        r += alfabeto[(b >> 12) & 63];
        if (i + 1 < n) r += alfabeto[(b >> 6) & 63];
        else if (relleno) r += '=';
        if (i + 2 < n) r += alfabeto[b & 63];
        else if (relleno) r += '=';
    }
    return r;
}
}  // namespace

std::string base64Url(const uint8_t* datos, size_t n) {
    return base64Con(datos, n, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", false);
}

std::string base64(const uint8_t* datos, size_t n) {
    return base64Con(datos, n, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", true);
}

std::string codificarUrl(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string r;
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            r += static_cast<char>(c);
        } else {
            r += '%';
            r += hex[c >> 4];
            r += hex[c & 15];
        }
    }
    return r;
}

std::string decodificarUrl(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') r += ' ';
        else if (s[i] == '%' && i + 2 < s.size()) {
            r += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else r += s[i];
    }
    return r;
}

std::string parametroConsulta(const std::string& consulta, const std::string& nombre) {
    size_t i = 0;
    while (i < consulta.size()) {
        size_t fin = consulta.find('&', i);
        if (fin == std::string::npos) fin = consulta.size();
        const std::string par = consulta.substr(i, fin - i);
        const size_t igual = par.find('=');
        if (igual != std::string::npos && par.substr(0, igual) == nombre) return decodificarUrl(par.substr(igual + 1));
        i = fin + 1;
    }
    return "";
}

std::string verificadorPkce(const std::vector<uint8_t>& aleatorios) {
    return base64Url(aleatorios.data(), aleatorios.size());
}

std::string desafioPkce(const std::string& verificador) {
    const Sha256 h = sha256(verificador.data(), verificador.size());
    return base64Url(h.data(), h.size());
}

}  // namespace bu
