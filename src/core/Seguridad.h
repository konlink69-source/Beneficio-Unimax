#pragma once
// Contraseñas: se guardan como PBKDF2-HMAC-SHA256 con sal aleatoria, nunca en texto.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bu {

// Usuario maestro que se crea la primera vez. En el primer ingreso se pide cambiar la contraseña.
constexpr const char* USUARIO_MAESTRO = "admin";
constexpr const char* CONTRASENA_INICIAL = "Unimax2026";
constexpr uint32_t ITERACIONES_HASH = 120000;

using Sha256 = std::array<uint8_t, 32>;

Sha256 sha256(const void* datos, size_t n);
Sha256 hmacSha256(const std::vector<uint8_t>& clave, const void* datos, size_t n);
Sha256 pbkdf2Sha256(const std::string& contrasena, const std::vector<uint8_t>& sal, uint32_t iteraciones);

std::string aHex(const uint8_t* datos, size_t n);
std::vector<uint8_t> desdeHex(const std::string& hex);

// "pbkdf2-sha256$<iteraciones>$<sal en hex>$<hash en hex>"
std::string crearHashContrasena(const std::string& contrasena, const std::vector<uint8_t>& sal,
                                uint32_t iteraciones = ITERACIONES_HASH);
bool verificarContrasena(const std::string& contrasena, const std::string& almacenado);

// Devuelve el problema de una contraseña nueva o vacío si es aceptable.
std::string validarContrasenaNueva(const std::string& contrasena, const std::string& usuario);

// Código de recuperación legible (por ejemplo "K7QM-2XRA-9DPT") a partir de bytes aleatorios.
std::string codigoRecuperacion(const std::vector<uint8_t>& aleatorios);
// Quita guiones y espacios y pasa a mayúsculas, para comparar códigos.
std::string normalizarCodigo(const std::string& codigo);

}  // namespace bu
