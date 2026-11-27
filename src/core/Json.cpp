#include "core/Json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace bu {

namespace {

class Lector {
public:
    explicit Lector(const std::string& s) : s_(s) {}

    Json valor() {
        espacios();
        if (i_ >= s_.size()) error("fin inesperado");
        const char c = s_[i_];
        if (c == '{') return objeto();
        if (c == '[') return lista();
        if (c == '"') return Json(cadena());
        if (c == 't') return palabra("true", Json(true));
        if (c == 'f') return palabra("false", Json(false));
        if (c == 'n') return palabra("null", Json());
        return numero();
    }

    void fin() {
        espacios();
        if (i_ != s_.size()) error("texto sobrante");
    }

private:
    [[noreturn]] void error(const char* que) {
        throw std::runtime_error(std::string("JSON inválido (") + que + ") en la posición " + std::to_string(i_));
    }
    void espacios() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\n' || s_[i_] == '\r' || s_[i_] == '\t')) ++i_;
    }
    void esperar(char c) {
        espacios();
        if (i_ >= s_.size() || s_[i_] != c) error("falta un símbolo");
        ++i_;
    }
    Json palabra(const char* p, Json v) {
        const std::string w(p);
        if (s_.compare(i_, w.size(), w) != 0) error("palabra desconocida");
        i_ += w.size();
        return v;
    }
    Json numero() {
        const size_t inicio = i_;
        while (i_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[i_])) || s_[i_] == '-' || s_[i_] == '+' ||
                                  s_[i_] == '.' || s_[i_] == 'e' || s_[i_] == 'E'))
            ++i_;
        if (inicio == i_) error("valor desconocido");
        return Json(std::strtod(s_.substr(inicio, i_ - inicio).c_str(), nullptr));
    }
    static void utf8(std::string& r, unsigned long cp) {
        if (cp < 0x80) {
            r += static_cast<char>(cp);
        } else if (cp < 0x800) {
            r += static_cast<char>(0xC0 | (cp >> 6));
            r += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            r += static_cast<char>(0xE0 | (cp >> 12));
            r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            r += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            r += static_cast<char>(0xF0 | (cp >> 18));
            r += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            r += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    unsigned long hex4() {
        if (i_ + 4 > s_.size()) error("escape incompleto");
        const unsigned long v = std::strtoul(s_.substr(i_, 4).c_str(), nullptr, 16);
        i_ += 4;
        return v;
    }
    std::string cadena() {
        esperar('"');
        std::string r;
        while (true) {
            if (i_ >= s_.size()) error("texto sin cerrar");
            const char c = s_[i_++];
            if (c == '"') break;
            if (c != '\\') {
                r += c;
                continue;
            }
            if (i_ >= s_.size()) error("escape incompleto");
            const char e = s_[i_++];
            switch (e) {
                case '"': r += '"'; break;
                case '\\': r += '\\'; break;
                case '/': r += '/'; break;
                case 'b': r += '\b'; break;
                case 'f': r += '\f'; break;
                case 'n': r += '\n'; break;
                case 'r': r += '\r'; break;
                case 't': r += '\t'; break;
                case 'u': {
                    unsigned long cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 6 <= s_.size() && s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                        i_ += 2;
                        const unsigned long bajo = hex4();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (bajo - 0xDC00);
                    }
                    utf8(r, cp);
                    break;
                }
                default: error("escape desconocido");
            }
        }
        return r;
    }
    Json lista() {
        esperar('[');
        Json r = Json::lista();
        espacios();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            return r;
        }
        while (true) {
            r.agregar(valor());
            espacios();
            if (i_ < s_.size() && s_[i_] == ',') {
                ++i_;
                continue;
            }
            esperar(']');
            return r;
        }
    }
    Json objeto() {
        esperar('{');
        Json r = Json::objeto();
        espacios();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            return r;
        }
        while (true) {
            espacios();
            const std::string clave = cadena();
            esperar(':');
            r[clave] = valor();
            espacios();
            if (i_ < s_.size() && s_[i_] == ',') {
                ++i_;
                continue;
            }
            esperar('}');
            return r;
        }
    }

    const std::string& s_;
    size_t i_ = 0;
};

const Json& nulo() {
    static const Json n;
    return n;
}

}  // namespace

Json Json::parsear(const std::string& texto) {
    Lector l(texto);
    Json r = l.valor();
    l.fin();
    return r;
}

std::string Json::texto(const std::string& defecto) const {
    if (tipo_ == Tipo::Texto) return texto_;
    if (tipo_ == Tipo::Numero) {
        // Google devuelve algunos números como texto y viceversa: se acepta ambos.
        if (std::floor(numero_) == numero_ && std::fabs(numero_) < 1e15) return std::to_string(static_cast<long long>(numero_));
        return std::to_string(numero_);
    }
    return defecto;
}

double Json::numero(double defecto) const {
    if (tipo_ == Tipo::Numero) return numero_;
    if (tipo_ == Tipo::Texto && !texto_.empty()) {
        char* fin = nullptr;
        const double v = std::strtod(texto_.c_str(), &fin);
        if (fin && *fin == '\0') return v;
    }
    return defecto;
}

long long Json::entero(long long defecto) const {
    const double v = numero(static_cast<double>(defecto));
    return static_cast<long long>(std::llround(v));
}

bool Json::booleano(bool defecto) const { return tipo_ == Tipo::Booleano ? booleano_ : defecto; }

const Json& Json::operator[](const std::string& clave) const {
    if (tipo_ != Tipo::Objeto) return nulo();
    const auto it = objeto_.find(clave);
    return it == objeto_.end() ? nulo() : it->second;
}

Json& Json::operator[](const std::string& clave) {
    if (tipo_ != Tipo::Objeto) {
        *this = objeto();
    }
    return objeto_[clave];
}

bool Json::tiene(const std::string& clave) const { return tipo_ == Tipo::Objeto && objeto_.count(clave) > 0; }

void Json::agregar(Json j) {
    if (tipo_ != Tipo::Lista) *this = lista();
    lista_.push_back(std::move(j));
}

std::string jsonTexto(const std::string& s) {
    std::string r = "\"";
    for (const char c : s) {
        switch (c) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    r += buf;
                } else {
                    r += c;
                }
        }
    }
    return r + "\"";
}

std::string Json::serializar() const {
    switch (tipo_) {
        case Tipo::Nulo: return "null";
        case Tipo::Booleano: return booleano_ ? "true" : "false";
        case Tipo::Numero: {
            if (std::floor(numero_) == numero_ && std::fabs(numero_) < 1e15)
                return std::to_string(static_cast<long long>(numero_));
            char buf[32];
            std::snprintf(buf, sizeof buf, "%.10g", numero_);
            return buf;
        }
        case Tipo::Texto: return jsonTexto(texto_);
        case Tipo::Lista: {
            std::string r = "[";
            for (size_t i = 0; i < lista_.size(); ++i) r += (i ? "," : "") + lista_[i].serializar();
            return r + "]";
        }
        case Tipo::Objeto: {
            std::string r = "{";
            bool primero = true;
            for (const auto& kv : objeto_) {
                r += (primero ? "" : ",") + jsonTexto(kv.first) + ":" + kv.second.serializar();
                primero = false;
            }
            return r + "}";
        }
    }
    return "null";
}

}  // namespace bu
