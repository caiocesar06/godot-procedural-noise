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

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(godot-cpp)

add_library(
    perlin_noise
    SHARED
    src/register_types.cpp
    src/noise_base.cpp
    src/perlin_noise.cpp
)

target_compile_features(
    perlin_noise
    PRIVATE
    cxx_std_17
)

target_include_directories(
    perlin_noise
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(
    perlin_noise
    PRIVATE
    godot-cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        perlin_noise
        PRIVATE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O2>
    )

    if(PERLIN_ENABLE_FAST_MATH)
        target_compile_options(
            perlin_noise
            PRIVATE
            $<$<CONFIG:Release>:-ffast-math>
        )

        message(
            STATUS
            "Fast-Math habilitado para builds Release."
        )
    endif()
endif()

find_package(OpenMP)

if(OpenMP_CXX_FOUND)
    target_link_libraries(
        perlin_noise
        PRIVATE
        OpenMP::OpenMP_CXX
    )

    message(STATUS "OpenMP habilitado.")
else()
    message(
        WARNING
        "OpenMP nao encontrado; usando execucao serial."
    )
endif()

if(WIN32)
    set(LIB_PREFIX "")
    set(LIB_SUFFIX ".dll")

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # target_link_options(
        #     perlin_noise
        #     PRIVATE
        #     -static-libgcc
        #     -static-libstdc++
        # )

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
