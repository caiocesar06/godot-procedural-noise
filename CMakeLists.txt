cmake_minimum_required(VERSION 3.16)

project(
    PerlinNoiseProject
    VERSION 1.0.0
    LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(
        CMAKE_BUILD_TYPE
        Release
        CACHE STRING
        "Build type"
    )

    set_property(
        CACHE CMAKE_BUILD_TYPE
        PROPERTY STRINGS
        Debug
        Release
        RelWithDebInfo
        MinSizeRel
    )
endif()

option(
    PERLIN_ENABLE_FAST_MATH
    "Ativa otimizacoes agressivas de ponto flutuante"
    OFF
)

# tests/ e benchmark/ NAO estao nesta branch: eles vivem na branch
# dev/tests-benchmark, para a master ficar so com o addon. Por isso os dois
# option() abaixo tem padrao OFF aqui; naquela branch, ON.
# O nucleo matematico (perlin_core.hpp, permutation_table.hpp,
# fractal.hpp) nao depende de godot-cpp. Desligar PERLIN_BUILD_EXTENSION
# permite compilar testes e benchmark sem sequer configurar o submodulo,
# o que deixa a verificacao do nucleo em segundos em vez de minutos.
option(
    PERLIN_BUILD_EXTENSION
    "Compila a GDExtension (exige o submodulo godot-cpp)"
    ON
)

option(
    PERLIN_BUILD_TESTS
    "Compila os testes do nucleo matematico"
    OFF
)

option(
    PERLIN_BUILD_BENCHMARK
    "Compila o harness de benchmark"
    OFF
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenMP)

if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP habilitado.")
else()
    message(
        WARNING
        "OpenMP nao encontrado; usando execucao serial."
    )
endif()

# Flags compartilhadas por todos os alvos. O benchmark precisa das mesmas
# flags da extensao, senao mede outra coisa.
function(perlin_apply_common_options TARGET_NAME)
    target_compile_features(
        ${TARGET_NAME}
        PRIVATE
        cxx_std_17
    )

    target_include_directories(
        ${TARGET_NAME}
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            ${TARGET_NAME}
            PRIVATE
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:RelWithDebInfo>:-O2>
        )

        if(PERLIN_ENABLE_FAST_MATH)
            target_compile_options(
                ${TARGET_NAME}
                PRIVATE
                $<$<CONFIG:Release>:-ffast-math>
            )
        endif()
    endif()

    if(OpenMP_CXX_FOUND)
        target_link_libraries(
            ${TARGET_NAME}
            PRIVATE
            OpenMP::OpenMP_CXX
        )
    endif()
endfunction()

if(PERLIN_ENABLE_FAST_MATH)
    message(
        STATUS
        "Fast-Math habilitado para builds Release."
    )
endif()

#-----------------------------------------------------------------------
# GDExtension
#-----------------------------------------------------------------------

if(PERLIN_BUILD_EXTENSION)
    add_subdirectory(godot-cpp)

    add_library(
        perlin_noise
        SHARED
        src/register_types.cpp
        src/noise_base.cpp
        src/perlin_noise.cpp
    )

    perlin_apply_common_options(perlin_noise)

    target_link_libraries(
        perlin_noise
        PRIVATE
        godot-cpp
    )

    if(WIN32)
        set(LIB_PREFIX "")
        set(LIB_SUFFIX ".dll")

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # A linkagem estatica (-static-libgcc -static-libstdc++) fica
            # desligada de proposito: o Erro 126 e resolvido copiando as
            # DLLs de runtime abaixo, e o manifesto .gdextension declara
            # essas dependencias explicitamente.

            get_filename_component(
                MINGW_BIN_DIR
                "${CMAKE_CXX_COMPILER}"
                DIRECTORY
            )

            set(
                MINGW_RUNTIME_DLLS
                "${MINGW_BIN_DIR}/libwinpthread-1.dll"
                "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
                "${MINGW_BIN_DIR}/libstdc++-6.dll"
            )

            if(OpenMP_CXX_FOUND)
                list(
                    APPEND
                    MINGW_RUNTIME_DLLS
                    "${MINGW_BIN_DIR}/libgomp-1.dll"
                )
            endif()

            foreach(RUNTIME_DLL IN LISTS MINGW_RUNTIME_DLLS)
                if(EXISTS "${RUNTIME_DLL}")
                    add_custom_command(
                        TARGET perlin_noise
                        POST_BUILD
                        COMMAND
                            "${CMAKE_COMMAND}"
                            -E copy_if_different
                            "${RUNTIME_DLL}"
                            "$<TARGET_FILE_DIR:perlin_noise>"
                        VERBATIM
                    )
                else()
                    message(
                        WARNING
                        "DLL de runtime nao encontrada: ${RUNTIME_DLL}"
                    )
                endif()
            endforeach()
        endif()

    elseif(APPLE)
        set(LIB_PREFIX "lib")
        set(LIB_SUFFIX ".dylib")

    elseif(UNIX)
        set(LIB_PREFIX "lib")
        set(LIB_SUFFIX ".so")

    else()
        message(
            FATAL_ERROR
            "Sistema operacional nao suportado."
        )
    endif()

    set(PERLIN_OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/project/addons/perlin_noise/bin"
    )

    set_target_properties(
        perlin_noise
        PROPERTIES
        PREFIX "${LIB_PREFIX}"
        SUFFIX "${LIB_SUFFIX}"

        LIBRARY_OUTPUT_DIRECTORY "${PERLIN_OUTPUT_DIRECTORY}"
        RUNTIME_OUTPUT_DIRECTORY "${PERLIN_OUTPUT_DIRECTORY}"
    )

    foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${CONFIG}" CONFIG_UPPER)

        set_target_properties(
            perlin_noise
            PROPERTIES
            "LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER}"
                "${PERLIN_OUTPUT_DIRECTORY}"
            "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}"
                "${PERLIN_OUTPUT_DIRECTORY}"
        )
    endforeach()
endif()

#-----------------------------------------------------------------------
# Testes do nucleo -- sem godot-cpp
#-----------------------------------------------------------------------

if(PERLIN_BUILD_TESTS)
    enable_testing()

    add_executable(
        perlin_tests
        tests/test_core.cpp
    )

    perlin_apply_common_options(perlin_tests)

    add_test(
        NAME nucleo_matematico
        COMMAND perlin_tests
    )
endif()

#-----------------------------------------------------------------------
# Benchmark -- sem godot-cpp
#-----------------------------------------------------------------------

if(PERLIN_BUILD_BENCHMARK)
    add_executable(
        perlin_benchmark
        benchmark/benchmark_main.cpp
    )

    perlin_apply_common_options(perlin_benchmark)
endif()
