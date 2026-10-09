add_executable(multififo_test multififo.cpp)
target_link_libraries(multififo_test PRIVATE multififo Threads::Threads Catch2::Catch2WithMain)

add_executable(modes_test modes.cpp)
target_link_libraries(modes_test PRIVATE multififo Threads::Threads Catch2::Catch2WithMain)

if(DEFINED Catch2_SOURCE_DIR)
  list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
else()
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/third_party/Catch2/extras")
endif()
include(Catch)

if(BUILD_TESTING)
  catch_discover_tests(multififo_test)
  catch_discover_tests(modes_test)
endif()
