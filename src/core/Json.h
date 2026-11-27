#pragma once
// JSON mínimo para hablar con Google Drive y guardar el estado de la sincronización.
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace bu {

class Json {
public:
    enum class Tipo { Nulo, Booleano, Numero, Texto, Lista, Objeto };

    Json() = default;
    Json(bool b) : tipo_(Tipo::Booleano), booleano_(b) {}
    Json(double n) : tipo_(Tipo::Numero), numero_(n) {}
    Json(int n) : tipo_(Tipo::Numero), numero_(n) {}
    Json(long n) : tipo_(Tipo::Numero), numero_(static_cast<double>(n)) {}
    Json(long long n) : tipo_(Tipo::Numero), numero_(static_cast<double>(n)) {}
    Json(const char* s) : tipo_(Tipo::Texto), texto_(s) {}
    Json(std::string s) : tipo_(Tipo::Texto), texto_(std::move(s)) {}

    static Json lista() {
        Json j;
        j.tipo_ = Tipo::Lista;
        return j;
    }
    static Json objeto() {
        Json j;
        j.tipo_ = Tipo::Objeto;
        return j;
    }
    // Lanza std::runtime_error si el texto no es JSON válido.
    static Json parsear(const std::string& texto);

    Tipo tipo() const { return tipo_; }
    bool esNulo() const { return tipo_ == Tipo::Nulo; }
    bool esObjeto() const { return tipo_ == Tipo::Objeto; }
    bool esLista() const { return tipo_ == Tipo::Lista; }

    // Lectura tolerante: si el tipo no coincide devuelve el valor por defecto.
    std::string texto(const std::string& defecto = "") const;
    double numero(double defecto = 0) const;
    long long entero(long long defecto = 0) const;
    bool booleano(bool defecto = false) const;

    // Objetos: devuelve un nulo si la clave no existe.
    const Json& operator[](const std::string& clave) const;
    Json& operator[](const std::string& clave);  // crea la clave (y convierte en objeto)
    bool tiene(const std::string& clave) const;
    const std::map<std::string, Json>& miembros() const { return objeto_; }

    // Listas.
    size_t tamano() const { return lista_.size(); }
    const Json& operator[](size_t i) const { return lista_.at(i); }
    void agregar(Json j);
    const std::vector<Json>& elementos() const { return lista_; }

    std::string serializar() const;

private:
    Tipo tipo_ = Tipo::Nulo;
    bool booleano_ = false;
    double numero_ = 0;
    std::string texto_;
    std::vector<Json> lista_;
    std::map<std::string, Json> objeto_;
};

// Texto entre comillas con los escapes de JSON.
std::string jsonTexto(const std::string& s);

}  // namespace bu
