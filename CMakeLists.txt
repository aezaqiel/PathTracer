cmake_minimum_required(VERSION 3.30)

project(RayTracing C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)

add_subdirectory(thirdparty/spdlog)
add_subdirectory(thirdparty/volk)
add_subdirectory(thirdparty/vma)
add_subdirectory(thirdparty/glm)
add_subdirectory(thirdparty/tinygltf)

# set(ASSET_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/assets)
file(TO_CMAKE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets ASSET_FOLDER)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/PathConfig.inl.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/PathConfig.inl
    @ONLY
)

add_executable(${PROJECT_NAME}
    src/Main.cpp
    src/Types.hpp

    src/Core/Application.hpp
    src/Core/Application.cpp
    src/Core/Logger.hpp
    src/Core/Logger.cpp

    src/RHI/VkTypes.hpp
    src/RHI/Instance.hpp
    src/RHI/Instance.cpp
    src/RHI/Device.hpp
    src/RHI/Device.cpp
    src/RHI/CommandManager.hpp
    src/RHI/Image.hpp
    src/RHI/Image.cpp
    src/RHI/Buffer.hpp
    src/RHI/Buffer.cpp
    src/RHI/AccelerationStructure.hpp
    src/RHI/AccelerationStructure.cpp

    src/Scene/SceneData.hpp
    src/Scene/SceneLoader.hpp
    src/Scene/SceneLoader.cpp

    src/Platform/VMAImpl.cpp
    src/Platform/TinyGlTFImpl.cpp
    src/Platform/STBImpl.cpp
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    src
    thirdparty/stb
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    spdlog
    volk
    VulkanMemoryAllocator
    glm
    tinygltf
)

target_compile_definitions(${PROJECT_NAME}
PRIVATE
    NOMINMAX
    GLM_ENABLE_EXPERIMENTAL
    TINYGLTF_NOEXCEPTION
)

target_precompile_headers(${PROJECT_NAME}
PRIVATE
    src/PCH.hpp
)

if(NOT MSVC)
    target_compile_options(${PROJECT_NAME}
    PRIVATE
        "-Wno-nullability-completeness"
    )
endif()
