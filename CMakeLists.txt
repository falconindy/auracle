cmake_minimum_required(VERSION 3.19)

project(auracle VERSION 0)
set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)
find_package(fmt REQUIRED)
find_package(absl REQUIRED COMPONENTS container status strings time format hash)

find_package(GTest REQUIRED COMPONENTS gtest_main gmock CONFIG)
include(GoogleTest)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libalpm REQUIRED IMPORTED_TARGET libalpm)
pkg_check_modules(libsystemd REQUIRED IMPORTED_TARGET libsystemd)

add_library(aur STATIC
    src/aur/aur.cc src/aur/aur.hh
    src/aur/package.cc src/aur/package.hh
    src/aur/request.cc src/aur/request.hh
    src/aur/response.cc src/aur/response.hh
)
target_link_libraries(aur
    PkgConfig::libsystemd
    absl::time
)
add_library(libauracle STATIC
    src/auracle/auracle.cc src/auracle/auracle.hh
    src/auracle/dependency_kind.cc src/auracle/dependency_kind.hh
    src/auracle/format.cc src/auracle/format.hh
    src/auracle/package_cache.cc src/auracle/package_cache.hh
    src/auracle/pacman.cc src/auracle/pacman.hh
    src/auracle/search_fragment.cc src/auracle/search_fragment.hh
    src/auracle/sort.cc src/auracle/sort.hh
    src/auracle/terminal.cc src/auracle/terminal.hh
)
target_include_directories(libauracle PUBLIC src)
target_compile_definitions(libauracle PUBLIC -DPROJECT_VERSION=\"${CMAKE_PROJECT_VERSION}\")
target_link_libraries(libauracle aur)

add_executable(auracle src/auracle_main.cc)
target_link_libraries(auracle
    CURL::libcurl
    absl::container
    absl::flat_hash_set
    absl::status
    absl::strings
    fmt::fmt
    nlohmann_json::nlohmann_json
    PkgConfig::libalpm

    libauracle
)
install(TARGETS auracle)

# Tests
enable_testing()

add_executable(libaur_test src/aur/request_test.cc src/aur/response_test.cc)
target_link_libraries(libaur_test aur absl::flat_hash_set CURL::libcurl GTest::gtest_main GTest::gmock)
gtest_discover_tests(libaur_test)

add_executable(libauracle_test
    src/auracle/dependency_kind_test.cc
    src/auracle/package_cache_test.cc
    src/auracle/format_test.cc
    src/auracle/search_fragment_test.cc
    src/auracle/sort_test.cc
)
target_link_libraries(libauracle_test
    libauracle
    PkgConfig::libalpm
    absl::flat_hash_set
    absl::status
    fmt::fmt
    GTest::gtest_main
    GTest::gmock
)
gtest_discover_tests(libauracle_test)
