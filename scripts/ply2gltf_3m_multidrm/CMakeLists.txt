cmake_minimum_required(VERSION 3.16)
project(ply2gltf_3m_multidrm VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()

# Function to clone external dependencies
function(CLONE NAME URL BRANCH)
  if(NOT EXISTS ${CMAKE_SOURCE_DIR}/external/${NAME})
    message(STATUS "Cloning ${NAME} ${URL} ${BRANCH}")
    execute_process(
      COMMAND git clone ${URL} -b ${BRANCH} --depth 1 ${CMAKE_SOURCE_DIR}/external/${NAME}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL "0")
      message(FATAL_ERROR "git clone ${URL} -b ${BRANCH} failed with ${GIT_RESULT}")
    endif()
  else()
    message(STATUS "Using vendored dependency: external/${NAME}")
  endif()
endfunction()

# Clone dependencies
CLONE(rapidjson https://github.com/Tencent/rapidjson.git      v1.1.0)
CLONE(tinyply   https://github.com/ddiakopoulos/tinyply.git   2.3.4)

# Include directories
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/external/rapidjson/include
  ${CMAKE_SOURCE_DIR}/external/tinyply/source
)

# Source files
set(SOURCES
  src/main.cpp
  src/PlyReader.cpp
  src/GltfWriter.cpp
  src/MultiFramePlyReader.cpp
  src/CircularBufferWriter.cpp
  src/MPEGGltfWriter.cpp
  # src/StreamingMultiFrameReader.cpp      # Temporarily disabled
  # src/StreamingCircularBufferWriter.cpp  # Temporarily disabled
  # src/StreamingMPEGGltfWriter.cpp       # Temporarily disabled
  external/tinyply/source/tinyply.cpp
)

# Create executable
add_executable(ply2gltf_3m_multidrm ${SOURCES})

# Link libraries
if(UNIX AND NOT APPLE)
  target_link_libraries(ply2gltf_3m_multidrm pthread)
endif()

# Set output directory
set_target_properties(ply2gltf_3m_multidrm PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Merge PLY utility
set(MERGE_PLY_SOURCES
  src/merge_ply.cpp
  src/PlyReader.cpp
  external/tinyply/source/tinyply.cpp
)

add_executable(merge_ply ${MERGE_PLY_SOURCES})

# Link libraries
if(UNIX AND NOT APPLE)
  target_link_libraries(merge_ply pthread)
endif()

set_target_properties(merge_ply PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Installation
install(TARGETS ply2gltf_3m_multidrm merge_ply
  RUNTIME DESTINATION bin
)
