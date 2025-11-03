cmake_minimum_required(VERSION 3.10)

project(PathTracer
    LANGUAGES C CXX
    VERSION 0.1.0
    DESCRIPTION "Vulkan PathTracer"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(GLSLC_EXE NAMES glslc HINTS Vulkan::glslc)
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

if(WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()

add_subdirectory(thirdparty/volk)
add_subdirectory(thirdparty/glm)
add_subdirectory(thirdparty/vma)
add_subdirectory(thirdparty/spdlog)
add_subdirectory(thirdparty/glfw)

add_executable(${PROJECT_NAME}
    src/Main.cpp

    src/Core/Types.hpp
    src/Core/Application.hpp
    src/Core/Application.cpp
    src/Core/Logger.hpp
    src/Core/Logger.cpp
    src/Core/KeyCodes.hpp
    src/Core/Events.hpp
    src/Core/Window.hpp
    src/Core/Window.cpp
    src/Core/Input.hpp
    src/Core/Input.cpp

    src/Renderer/Renderer.hpp
    src/Renderer/Renderer.cpp

    src/RHI/VulkanTypes.hpp
    src/RHI/VulkanContext.hpp
    src/RHI/VulkanContext.cpp
    src/RHI/VulkanDevice.hpp
    src/RHI/VulkanDevice.cpp
    src/RHI/VulkanSwapchain.hpp
    src/RHI/VulkanSwapchain.cpp
    src/RHI/VulkanCommand.hpp
    src/RHI/VulkanCommand.cpp
    src/RHI/VulkanImage.hpp
    src/RHI/VulkanImage.cpp
    src/RHI/VulkanBuffer.hpp
    src/RHI/VulkanBuffer.cpp
    src/RHI/VulkanAccelerationStructure.hpp
    src/RHI/VulkanAccelerationStructure.cpp
    src/RHI/VulkanDescriptorSetLayout.hpp
    src/RHI/VulkanDescriptorSetLayout.cpp
    src/RHI/VulkanDescriptorPool.hpp
    src/RHI/VulkanDescriptorPool.cpp
    src/RHI/VulkanDescriptorWriter.hpp
    src/RHI/VulkanDescriptorWriter.cpp
    src/RHI/VulkanPipelineLayout.hpp
    src/RHI/VulkanPipelineLayout.cpp
    src/RHI/VulkanShader.hpp
    src/RHI/VulkanShader.cpp
    src/RHI/VulkanPipeline.hpp
    src/RHI/VulkanPipeline.cpp
    src/RHI/VulkanShaderBindingTable.hpp
    src/RHI/VulkanShaderBindingTable.cpp

    src/Platform/VMA/VMAImpl.cpp
    src/Platform/STB/STBImpl.cpp
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    src
    thirdparty/stb
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    volk
    glm
    VulkanMemoryAllocator 
    spdlog
    glfw
)

target_compile_definitions(${PROJECT_NAME}
PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    SHADER_DIR=\"${SHADER_BIN_DIR}\"
    GLFW_INCLUDE_NONE
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        GLFW_EXPOSE_NATIVE_WIN32
    )
endif()

target_precompile_headers(${PROJECT_NAME}
PRIVATE
    src/pch.hpp
)

if(MSVC)
else()
    target_compile_options(${PROJECT_NAME}
    PRIVATE
        "-Wno-nullability-completeness"
    )
endif()

function(compile_shaders_target target_name)
    file(GLOB_RECURSE SHADER_SOURCES
        ${SHADER_SRC_DIR}/*.vert
        ${SHADER_SRC_DIR}/*.frag
        ${SHADER_SRC_DIR}/*.comp
        ${SHADER_SRC_DIR}/*.geom
        ${SHADER_SRC_DIR}/*.tesc
        ${SHADER_SRC_DIR}/*.tese
        ${SHADER_SRC_DIR}/*.mesh
        ${SHADER_SRC_DIR}/*.task
        ${SHADER_SRC_DIR}/*.rgen
        ${SHADER_SRC_DIR}/*.rchit
        ${SHADER_SRC_DIR}/*.rmiss
    )

    if(NOT SHADER_SOURCES)
        message(WARNING "No shader sources found in ${SHADER_SRC_DIR}")
        return()
    endif()

    add_custom_command(
        OUTPUT ${SHADER_BIN_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BIN_DIR}
        COMMENT "Creating shader output directory: ${SHADER_BIN_DIR}"
    )

    set(SPV_SHADERS "")

    foreach(shader_source ${SHADER_SOURCES})
        get_filename_component(shader_name ${shader_source} NAME)
        set(spv_output ${SHADER_BIN_DIR}/${shader_name}.spv)

        add_custom_command(
            OUTPUT ${spv_output}
            COMMAND ${GLSLC_EXE}
                --target-spv=spv1.4
                -o ${spv_output}
                ${shader_source}
            DEPENDS ${shader_source} ${SHADER_BIN_DIR}
            COMMENT "Compiling shader: ${shader_name}"
            VERBATIM
        )

        list(APPEND SPV_SHADERS ${spv_output})
    endforeach()

    add_custom_target(${target_name}
        DEPENDS ${SPV_SHADERS}
        COMMENT "Building all shaders"
    )

    add_dependencies(${PROJECT_NAME} ${target_name})
    
    target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${SHADER_BIN_DIR}
    )
endfunction()

compile_shaders_target(compile_shaders)
