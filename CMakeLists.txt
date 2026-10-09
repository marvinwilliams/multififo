cmake_minimum_required(VERSION 3.16)

project(
  multififo
  VERSION 3.0.0
  LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(utils)
disable_intree_builds()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(MULTIFIFO_BUILD_EXAMPLES "Build examples" OFF)
# The target to be linked against by other targets. This library is header-only
# and as such does not compile by itself. This target rather sets include
# directories and required compiler flags.
add_library(multififo INTERFACE)

# Also include headers generated at build time. When installing the library, all
# files will be copied to the same location such that no additional directories
# have to be included.
target_include_directories(
  multififo INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                       $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_include_directories(
  multififo SYSTEM
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/multififo/third_party/pcg>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/multififo/third_party/pcg>)

# Require c++17 or above
target_compile_features(multififo INTERFACE cxx_std_17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)
find_package(Boost)

# The namespace alias can be used as link target if this project is a
# subproject.
add_library("multififo::multififo" ALIAS multififo)

if(NOT TARGET Catch2::Catch2WithMain)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Catch2/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/Catch2" EXCLUDE_FROM_ALL)
  else()
    find_package(Catch2 3 QUIET)
  endif()
endif()
if(TARGET Catch2::Catch2WithMain)
  option(MULTIFIFO_BUILD_TESTS "Generate unit test targets" ON)
else()
  message(WARNING "Catch2 not found, tests unavailable")
  set(MULTIFIFO_BUILD_TESTS OFF)
endif()

if(MULTIFIFO_BUILD_TESTS)
  include(CTest)
  set(TEST_RUNNER_PARAMS
      ""
      CACHE STRING "Options added to the test runner")
  add_subdirectory(tests)
endif()
if(MULTIFIFO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

include(InstallRequiredSystemLibraries)

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_LIST_DIR}/README.md")
include(CPack)

set(INSTALL_MODULEDIR "${CMAKE_INSTALL_LIBDIR}/cmake")
set(INSTALL_CMAKEDIR "${INSTALL_MODULEDIR}/multififo")

configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/cmake/multififoConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/multififoConfig.cmake"
  INSTALL_DESTINATION "${INSTALL_CMAKEDIR}"
  PATH_VARS INSTALL_MODULEDIR)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/multififoConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)

# Define the targets to add to the export set. Since this is a header-only
# library, only the usage requirements are propagated
install(TARGETS multififo EXPORT multififoExport)

# On install, copy the header files to the appropriate location.
install(
  DIRECTORY "include/multififo/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/multififo"
  COMPONENT Development
  FILES_MATCHING
  PATTERN "*.hpp")

# Copy the package config file to the appropriate directory so that it can be
# found by find_package()
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/multififoConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/multififoConfigVersion.cmake"
        DESTINATION "${INSTALL_CMAKEDIR}")

# Install the export set consisting of the multififo target
install(
  EXPORT multififoExport
  DESTINATION "${INSTALL_CMAKEDIR}"
  NAMESPACE multififo::
  FILE multififoTargets.cmake)

install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}")
