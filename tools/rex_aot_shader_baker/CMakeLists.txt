add_executable(rex_aot_shader_baker
    main.cpp
    ${REXGLUE_ROOT}/src/graphics/format/ucode.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/shader.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/interpreter.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/translator.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/translator_disasm.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/spirv_builder.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/spirv.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/spirv_translator.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/spirv_translator_alu.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/spirv_translator_fetch.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/spirv_translator_memexport.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/spirv_translator_rb.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/shader/prebaked_shader_cache.cpp
    ${REXGLUE_ROOT}/src/graphics/flags.cpp
    ${REXGLUE_ROOT}/src/graphics/registers.cpp
    ${REXGLUE_ROOT}/src/graphics/register_file.cpp
    ${REXGLUE_ROOT}/src/graphics/util/draw.cpp
)

rex_config_postfix(rex_aot_shader_baker)

target_include_directories(rex_aot_shader_baker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REXGLUE_ROOT}/include
    ${REXGLUE_ROOT}/src
    ${REXGLUE_ROOT}/thirdparty/plume
    ${REXGLUE_ROOT}/thirdparty/renderdoc
)

target_compile_options(rex_aot_shader_baker PRIVATE -Wno-unused-variable)

target_link_libraries(rex_aot_shader_baker PRIVATE
    rexruntime
    xxHash::xxhash
    Vulkan::Headers
    glslang::SPIRV
    glslang
    MachineIndependent
    GenericCodeGen
    OSDependent
    OGLCompiler
)
