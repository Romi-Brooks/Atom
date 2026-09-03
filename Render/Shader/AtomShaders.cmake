set(ATOM_VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}" CACHE PATH
    "Optional Vulkan SDK root used as a shader-tool search hint")
set(ATOM_DXC_ROOT "" CACHE PATH
    "Optional Microsoft DirectX Shader Compiler root used as a search hint")

if(WIN32)
    set(ATOM_SHADER_BUILD_DXIL_DEFAULT ON)
else()
    set(ATOM_SHADER_BUILD_DXIL_DEFAULT OFF)
endif()

if(APPLE)
    set(ATOM_SHADER_BUILD_MSL_DEFAULT ON)
else()
    set(ATOM_SHADER_BUILD_MSL_DEFAULT OFF)
endif()

option(ATOM_SHADER_BUILD_DXIL "Build DXIL shaders for the SDL_GPU D3D12 backend"
       ${ATOM_SHADER_BUILD_DXIL_DEFAULT})
option(ATOM_SHADER_BUILD_MSL "Build MSL shaders for the SDL_GPU Metal backend"
       ${ATOM_SHADER_BUILD_MSL_DEFAULT})

find_program(ATOM_GLSLC glslc
    HINTS "${ATOM_VULKAN_SDK_ROOT}/Bin" "${ATOM_VULKAN_SDK_ROOT}/bin"
    REQUIRED)
find_program(ATOM_SPIRV_VAL spirv-val
    HINTS "${ATOM_VULKAN_SDK_ROOT}/Bin" "${ATOM_VULKAN_SDK_ROOT}/bin"
    REQUIRED)

if(ATOM_SHADER_BUILD_DXIL OR ATOM_SHADER_BUILD_MSL)
    find_program(ATOM_SPIRV_CROSS spirv-cross
        HINTS "${ATOM_VULKAN_SDK_ROOT}/Bin" "${ATOM_VULKAN_SDK_ROOT}/bin"
        REQUIRED)
endif()

if(ATOM_SHADER_BUILD_DXIL)
    find_program(ATOM_DXC dxc
        HINTS "${ATOM_DXC_ROOT}/bin/x64" "${ATOM_DXC_ROOT}/bin"
        REQUIRED)
endif()

# Render/Shader/Source holds the hand-written GLSL; anchor to this module's own
# directory so the file stays correct wherever it is included from.
set(ATOM_SHADER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/Source")

set(ATOM_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
file(MAKE_DIRECTORY "${ATOM_SHADER_OUTPUT_DIR}/spirv")
if(ATOM_SHADER_BUILD_DXIL)
    file(MAKE_DIRECTORY "${ATOM_SHADER_OUTPUT_DIR}/hlsl" "${ATOM_SHADER_OUTPUT_DIR}/dxil")
endif()
if(ATOM_SHADER_BUILD_MSL)
    file(MAKE_DIRECTORY "${ATOM_SHADER_OUTPUT_DIR}/msl")
endif()

function(atom_compile_shader source stage)
    get_filename_component(shader_name "${source}" NAME)
    set(source_path "${ATOM_SHADER_SOURCE_DIR}/${source}")
    set(spv "${ATOM_SHADER_OUTPUT_DIR}/spirv/${shader_name}.spv")

    add_custom_command(OUTPUT "${spv}"
        COMMAND "${ATOM_GLSLC}" -fshader-stage=${stage} "${source_path}" -o "${spv}"
        COMMAND "${ATOM_SPIRV_VAL}" --target-env vulkan1.0 "${spv}"
        DEPENDS "${source_path}"
        COMMENT "Compiling ${source} to SPIR-V"
        VERBATIM)
    set_property(GLOBAL APPEND PROPERTY ATOM_SHADER_OUTPUTS "${spv}")

    if(ATOM_SHADER_BUILD_DXIL)
        set(hlsl "${ATOM_SHADER_OUTPUT_DIR}/hlsl/${shader_name}.hlsl")
        set(dxil "${ATOM_SHADER_OUTPUT_DIR}/dxil/${shader_name}.dxil")
        if(stage STREQUAL "vert")
            set(profile vs_6_0)
        else()
            set(profile ps_6_0)
        endif()
        add_custom_command(OUTPUT "${hlsl}"
            COMMAND "${ATOM_SPIRV_CROSS}" "${spv}" --hlsl --shader-model 60 --output "${hlsl}"
            DEPENDS "${spv}"
            COMMENT "Translating ${source} to HLSL"
            VERBATIM)
        add_custom_command(OUTPUT "${dxil}"
            COMMAND "${ATOM_DXC}" -T ${profile} -E main "${hlsl}" -Fo "${dxil}"
            DEPENDS "${hlsl}"
            COMMENT "Compiling ${source} to DXIL"
            VERBATIM)
        set_property(GLOBAL APPEND PROPERTY ATOM_SHADER_OUTPUTS "${hlsl}" "${dxil}")
    endif()

    if(ATOM_SHADER_BUILD_MSL)
        set(msl "${ATOM_SHADER_OUTPUT_DIR}/msl/${shader_name}.msl")
        add_custom_command(OUTPUT "${msl}"
            COMMAND "${ATOM_SPIRV_CROSS}" "${spv}" --msl --output "${msl}"
            DEPENDS "${spv}"
            COMMENT "Translating ${source} to MSL"
            VERBATIM)
        set_property(GLOBAL APPEND PROPERTY ATOM_SHADER_OUTPUTS "${msl}")
    endif()
endfunction()

atom_compile_shader(MeshUnlit.vert.glsl vert)
atom_compile_shader(MeshUnlit.frag.glsl frag)
atom_compile_shader(Sprite.vert.glsl vert)
atom_compile_shader(Sprite.frag.glsl frag)
atom_compile_shader(Primitive2D.vert.glsl vert)
atom_compile_shader(Primitive2D.frag.glsl frag)
atom_compile_shader(PostProcess.vert.glsl vert)
atom_compile_shader(ChromaticAberration.frag.glsl frag)
atom_compile_shader(Glitch.frag.glsl frag)
atom_compile_shader(GaussianBlur.frag.glsl frag)
get_property(ATOM_SHADER_OUTPUTS GLOBAL PROPERTY ATOM_SHADER_OUTPUTS)
add_custom_target(Atom_Shaders ALL DEPENDS ${ATOM_SHADER_OUTPUTS})
