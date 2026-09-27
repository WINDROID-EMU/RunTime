add_executable(rex_render_benchmark
    main.cpp
    bench_shaders.cpp
    bench_textures.cpp
    bench_dispatch.cpp
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
    ${REXGLUE_ROOT}/src/graphics/pipeline/texture/conversion.cpp
    ${REXGLUE_ROOT}/src/graphics/pipeline/texture/util.cpp
    ${REXGLUE_ROOT}/src/graphics/pm4_plume_transpiler.cpp
    ${REXGLUE_ROOT}/src/graphics/flags.cpp
    ${REXGLUE_ROOT}/src/graphics/registers.cpp
    ${REXGLUE_ROOT}/src/graphics/register_file.cpp
    ${REXGLUE_ROOT}/src/graphics/util/draw.cpp
)

rex_config_postfix(rex_render_benchmark)

target_include_directories(rex_render_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REXGLUE_ROOT}/include
    ${REXGLUE_ROOT}/src
    ${REXGLUE_ROOT}/thirdparty/plume
    ${REXGLUE_ROOT}/thirdparty/renderdoc
)

target_compile_options(rex_render_benchmark PRIVATE -Wno-unused-variable)

target_link_libraries(rex_render_benchmark PRIVATE
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
