#include "core/Seguridad.h"

#include <cctype>
#include <cstring>

namespace bu {

namespace {

// ---------------------------------------------------------------- SHA-256 (FIPS 180-4)
const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

struct Contexto {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint8_t bloque[64] = {0};
    size_t usados = 0;
    uint64_t total = 0;

    void comprimir(const uint8_t* p) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(p[4 * i]) << 24) | (uint32_t(p[4 * i + 1]) << 16) | (uint32_t(p[4 * i + 2]) << 8) |
                   uint32_t(p[4 * i + 3]);
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = S0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    void agregar(const void* datos, size_t n) {
        const uint8_t* p = static_cast<const uint8_t*>(datos);
        total += n;
        while (n > 0) {
            const size_t k = (64 - usados) < n ? (64 - usados) : n;
            std::memcpy(bloque + usados, p, k);
            usados += k;
            p += k;
            n -= k;
            if (usados == 64) {
                comprimir(bloque);
                usados = 0;
            }
        }
    }

    Sha256 terminar() {
        const uint64_t bits = total * 8;
        const uint8_t uno = 0x80;
        agregar(&uno, 1);
        const uint8_t cero = 0;
        while (usados != 56) agregar(&cero, 1);
        uint8_t largo[8];
        for (int i = 0; i < 8; ++i) largo[i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
        agregar(largo, 8);
        Sha256 r;
        for (int i = 0; i < 8; ++i) {
            r[4 * i] = static_cast<uint8_t>(h[i] >> 24);
            r[4 * i + 1] = static_cast<uint8_t>(h[i] >> 16);
            r[4 * i + 2] = static_cast<uint8_t>(h[i] >> 8);
            r[4 * i + 3] = static_cast<uint8_t>(h[i]);
        }
        return r;
    }
};

// HMAC con los bloques de la clave ya procesados (para PBKDF2 rápido).
struct Hmac {
    Contexto interno, externo;
    explicit Hmac(const std::vector<uint8_t>& clave) {
        uint8_t k[64] = {0};
        if (clave.size() > 64) {
            const Sha256 d = sha256(clave.data(), clave.size());
            std::memcpy(k, d.data(), d.size());
        } else if (!clave.empty()) {
            std::memcpy(k, clave.data(), clave.size());
        }
        uint8_t ipad[64], opad[64];
        for (int i = 0; i < 64; ++i) {
            ipad[i] = k[i] ^ 0x36;
            opad[i] = k[i] ^ 0x5c;
        }
        interno.agregar(ipad, 64);
        externo.agregar(opad, 64);
    }
    Sha256 calcular(const void* datos, size_t n) const {
        Contexto a = interno;
        a.agregar(datos, n);
        const Sha256 d = a.terminar();
        Contexto b = externo;
        b.agregar(d.data(), d.size());
        return b.terminar();
    }
};

bool igualesTiempoConstante(const std::vector<uint8_t>& a, const uint8_t* b, size_t n) {
    if (a.size() != n) return false;
    uint8_t dif = 0;
    for (size_t i = 0; i < n; ++i) dif |= static_cast<uint8_t>(a[i] ^ b[i]);
    return dif == 0;
}

}  // namespace

Sha256 sha256(const void* datos, size_t n) {
    Contexto c;
    c.agregar(datos, n);
    return c.terminar();
}

Sha256 hmacSha256(const std::vector<uint8_t>& clave, const void* datos, size_t n) { return Hmac(clave).calcular(datos, n); }

Sha256 pbkdf2Sha256(const std::string& contrasena, const std::vector<uint8_t>& sal, uint32_t iteraciones) {
    const Hmac prf(std::vector<uint8_t>(contrasena.begin(), contrasena.end()));
    std::vector<uint8_t> entrada(sal);
    entrada.insert(entrada.end(), {0, 0, 0, 1});  // bloque 1 (basta un bloque de 32 bytes)
    Sha256 u = prf.calcular(entrada.data(), entrada.size());
    Sha256 t = u;
    for (uint32_t i = 1; i < iteraciones; ++i) {
        u = prf.calcular(u.data(), u.size());
        for (size_t j = 0; j < t.size(); ++j) t[j] ^= u[j];
    }
    return t;
}

std::string aHex(const uint8_t* datos, size_t n) {
    static const char* const D = "0123456789abcdef";
    std::string r;
    r.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        r += D[datos[i] >> 4];
        r += D[datos[i] & 15];
    }
    return r;
}

std::vector<uint8_t> desdeHex(const std::string& hex) {
    auto valor = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<uint8_t> r;
    if (hex.size() % 2) return r;
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int a = valor(hex[i]), b = valor(hex[i + 1]);
        if (a < 0 || b < 0) return {};
        r.push_back(static_cast<uint8_t>(a * 16 + b));
    }
    return r;
}

std::string crearHashContrasena(const std::string& contrasena, const std::vector<uint8_t>& sal, uint32_t iteraciones) {
    const Sha256 h = pbkdf2Sha256(contrasena, sal, iteraciones);
    return "pbkdf2-sha256$" + std::to_string(iteraciones) + "$" + aHex(sal.data(), sal.size()) + "$" +
           aHex(h.data(), h.size());
}

bool verificarContrasena(const std::string& contrasena, const std::string& almacenado) {
    // pbkdf2-sha256$iter$sal$hash
    const size_t a = almacenado.find('$');
    const size_t b = a == std::string::npos ? a : almacenado.find('$', a + 1);
    const size_t c = b == std::string::npos ? b : almacenado.find('$', b + 1);
    if (c == std::string::npos || almacenado.substr(0, a) != "pbkdf2-sha256") return false;
    uint32_t iteraciones = 0;
    try {
        iteraciones = static_cast<uint32_t>(std::stoul(almacenado.substr(a + 1, b - a - 1)));
    } catch (...) {
        return false;
    }
    if (iteraciones == 0 || iteraciones > 10000000) return false;
    const std::vector<uint8_t> sal = desdeHex(almacenado.substr(b + 1, c - b - 1));
    const std::vector<uint8_t> esperado = desdeHex(almacenado.substr(c + 1));
    if (sal.empty() || esperado.size() != 32) return false;
    const Sha256 h = pbkdf2Sha256(contrasena, sal, iteraciones);
    return igualesTiempoConstante(esperado, h.data(), h.size());
}

std::string validarContrasenaNueva(const std::string& contrasena, const std::string& usuario) {
    if (contrasena.size() < 6) return "La contraseña debe tener al menos 6 caracteres.";
    bool letra = false, numero = false;
    for (char ch : contrasena) {
        if (std::isdigit(static_cast<unsigned char>(ch))) numero = true;
        else if (std::isalpha(static_cast<unsigned char>(ch)) || static_cast<unsigned char>(ch) >= 0x80) letra = true;
    }
    if (!letra || !numero) return "La contraseña debe tener letras y números.";
    if (contrasena == CONTRASENA_INICIAL) return "Escriba una contraseña distinta de la contraseña inicial.";
    std::string a = contrasena, b = usuario;
    for (auto& ch : a) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    for (auto& ch : b) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (!b.empty() && a == b) return "La contraseña no puede ser igual al nombre de usuario.";
    return std::string();
}

std::string codigoRecuperacion(const std::vector<uint8_t>& aleatorios) {
    // Sin letras que se confunden (0/O, 1/I/L).
    static const char* const ALFABETO = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
    const size_t n = std::strlen(ALFABETO);
    std::string r;
    for (size_t i = 0; i < 12; ++i) {
        if (i > 0 && i % 4 == 0) r += '-';
        const uint8_t b = i < aleatorios.size() ? aleatorios[i] : static_cast<uint8_t>(i * 37);
        r += ALFABETO[b % n];
    }
    return r;
}

std::string normalizarCodigo(const std::string& codigo) {
    std::string r;
    for (char ch : codigo) {
        if (ch == '-' || ch == ' ') continue;
        r += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return r;
}

}  // namespace bu
