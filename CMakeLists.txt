cmake_minimum_required(VERSION 3.16)
project(BeneficioUnimax VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Visual Studio: incluir la biblioteca de C++ dentro del .exe, para que el
# programa abra en cualquier PC sin instalar "Visual C++ Redistributable".
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------------------------------------------------------------- Núcleo
# Lógica del negocio y base de datos (no depende de las ventanas).
add_library(bu_nucleo STATIC
    src/core/Fecha.cpp
    src/core/Dinero.cpp
    src/core/Identificacion.cpp
    src/core/NumeroLetras.cpp
    src/core/Texto.cpp
    src/core/Modelos.cpp
    src/core/Documentos.cpp
    src/core/Seguridad.cpp
    src/core/Grafico.cpp
    src/core/Json.cpp
    src/core/Sincronizacion.cpp
    src/core/Panel.cpp
    src/db/Odbc.cpp
    src/db/Esquema.cpp
    src/db/Repositorio.cpp
)
target_include_directories(bu_nucleo PUBLIC src)

# Las opciones solo se aplican a C++ (no al compilador de recursos rc.exe).
if(MSVC)
    target_compile_options(bu_nucleo PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:/utf-8;/W3;/permissive->")
    target_compile_definitions(bu_nucleo PUBLIC UNICODE _UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(bu_nucleo PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra>")
    if(WIN32)
        target_compile_definitions(bu_nucleo PUBLIC UNICODE _UNICODE NOMINMAX)
    endif()
endif()

if(WIN32)
    target_link_libraries(bu_nucleo PUBLIC odbc32 odbccp32)
    if(MSVC)
        # odbccp32.lib usa funciones de stdio antiguas que desde VS 2015 están en esta biblioteca.
        target_link_libraries(bu_nucleo PUBLIC legacy_stdio_definitions)
    endif()
else()
    find_package(ODBC REQUIRED)
    target_link_libraries(bu_nucleo PUBLIC ODBC::ODBC)
endif()

# ---------------------------------------------------------------- Programa de Windows
if(WIN32)
    enable_language(RC)
    add_executable(BeneficioUnimax WIN32
        src/win/main.cpp
        src/win/App.cpp
        src/win/Util.cpp
        src/win/Excel.cpp
        src/win/Avisos.cpp
        src/win/Instalacion.cpp
        src/win/Impresion.cpp
        src/win/Http.cpp
        src/win/Tareas.cpp
        src/win/GoogleDrive.cpp
        src/win/Nube.cpp
        src/win/Actualizacion.cpp
        src/win/Sesion.cpp
        src/win/Dialogos.cpp
        src/win/Paginas.cpp
        src/win/PaginaInicio.cpp
        src/win/Buscador.cpp
        src/win/Periodo.cpp
        src/win/Tema.cpp
        src/win/PaginaFactura.cpp
        src/win/PaginaFacturas.cpp
        src/win/PaginaClientes.cpp
        src/win/PaginaPrecios.cpp
        src/win/PaginaDeudas.cpp
        src/win/PaginaCaja.cpp
        src/win/PaginaVentas.cpp
        src/win/PaginaReportes.cpp
        src/win/PaginaConfiguracion.cpp
        src/win/app.rc
    )
    target_link_libraries(BeneficioUnimax PRIVATE bu_nucleo comctl32 ole32 oleaut32 uuid shell32 shlwapi advapi32 bcrypt winspool gdi32 comdlg32 winhttp ws2_32 crypt32 gdiplus uxtheme)
    # Versión (la pone GitHub Actions) y datos de la aplicación de Google (secretos del repositorio).
    set(BU_VERSION "0" CACHE STRING "Número de versión del programa")
    set(BU_GOOGLE_CLIENT_ID "$ENV{BU_GOOGLE_CLIENT_ID}" CACHE STRING "ID de cliente de Google (OAuth, Escritorio)")
    set(BU_GOOGLE_CLIENT_SECRET "$ENV{BU_GOOGLE_CLIENT_SECRET}" CACHE STRING "Secreto de cliente de Google")
    set(BU_REPOSITORIO "konlink69-source/Beneficio-Unimax" CACHE STRING "Repositorio de GitHub (público) con las descargas")
    target_compile_definitions(BeneficioUnimax PRIVATE
        BU_VERSION=${BU_VERSION}
        "BU_GOOGLE_CLIENT_ID=\"${BU_GOOGLE_CLIENT_ID}\""
        "BU_GOOGLE_CLIENT_SECRET=\"${BU_GOOGLE_CLIENT_SECRET}\""
        "BU_REPOSITORIO=\"${BU_REPOSITORIO}\"")
    if(MSVC)
        # El manifiesto (estilos visuales y DPI) va dentro de app.rc.
        target_link_options(BeneficioUnimax PRIVATE /MANIFEST:NO)
    else()
        target_compile_options(BeneficioUnimax PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra>")
        target_link_options(BeneficioUnimax PRIVATE -municode -static)
        set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -c 65001")
    endif()
    # Copia las plantillas de Excel junto al .exe.
    add_custom_command(TARGET BeneficioUnimax POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/plantillas $<TARGET_FILE_DIR:BeneficioUnimax>/plantillas)
endif()

# ---------------------------------------------------------------- Pruebas
option(BU_PRUEBAS "Compilar el programa de pruebas" ON)
if(BU_PRUEBAS)
    enable_testing()
    add_executable(pruebas tests/pruebas.cpp)
    target_link_libraries(pruebas PRIVATE bu_nucleo)
    if(MINGW)
        target_link_options(pruebas PRIVATE -static)
    endif()
    add_test(NAME pruebas COMMAND pruebas)
endif()
