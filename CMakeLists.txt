cmake_minimum_required(VERSION 3.25)
project(epex-proxy VERSION 1.0.1 LANGUAGES CXX)

cmake_policy(SET CMP0167 NEW)  # use BoostConfig, not legacy FindBoost

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.81 REQUIRED CONFIG)
find_package(OpenSSL REQUIRED)

# pugixml — prefer system package, fall back to FetchContent
find_package(pugixml QUIET)
if(NOT pugixml_FOUND)
    include(FetchContent)
    FetchContent_Declare(pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG        v1.14
        GIT_SHALLOW    TRUE
        SYSTEM  # treat includes as system headers (suppresses warnings from them)
    )
    FetchContent_MakeAvailable(pugixml)
endif()

# nlohmann_json — prefer system package, fall back to FetchContent
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        SYSTEM  # treat includes as system headers (suppresses warnings from them)
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_executable(epex-proxy
    src/main.cpp
    src/config.cpp
    src/store.cpp
    src/time_util.cpp
    src/calculator.cpp
    src/parser.cpp
    src/entsoe.cpp
    src/scheduler.cpp
    src/http/server.cpp
    src/http/handlers.cpp
)

target_include_directories(epex-proxy PRIVATE src)

target_link_libraries(epex-proxy PRIVATE
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    pugixml::pugixml
    nlohmann_json::nlohmann_json
)

# ── Install rules ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS epex-proxy
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Systemd unit — generated from .in so ExecStart reflects the actual install prefix.
# configure_file runs at cmake configure time, so pass -DCMAKE_INSTALL_PREFIX=/usr
# when configuring for a Debian package build.
configure_file(
    systemd/epex-proxy.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/epex-proxy.service
    @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/epex-proxy.service
    DESTINATION lib/systemd/system
)

# Shipping config under share/; postinst copies it to /etc if no config exists yet
install(FILES config.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/epex-proxy
    RENAME config.json.example
)

# ── CPack — Debian .deb ────────────────────────────────────────────────────────
# Build the package with:
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
#   cmake --build build
#   cmake --build build --target package   (requires dpkg-dev on the host)
#
# -DCMAKE_INSTALL_PREFIX=/usr is required because configure_file bakes
# CMAKE_INSTALL_FULL_BINDIR into the generated systemd unit at configure time.
# CPACK_PACKAGING_INSTALL_PREFIX below sets the in-package paths for everything
# else (binary, data files) and does not affect configure_file output.
set(CPACK_PACKAGE_NAME             "epex-proxy")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "EPEX day-ahead electricity price proxy for the Netherlands")
set(CPACK_PACKAGE_CONTACT          "Martijn Otto <claude@martijnotto.nl>")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

set(CPACK_GENERATOR                "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)   # auto-detect runtime deps via dpkg-shlibdeps
set(CPACK_DEBIAN_PACKAGE_SECTION   "net")
set(CPACK_DEBIAN_PACKAGE_PRIORITY  "optional")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst"
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm"
    "${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm"
)

include(CPack)

target_compile_options(epex-proxy PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wimplicit-fallthrough
        -Werror
        # GCC false positive: -Wnull-dereference fires on Boost Beast/Asio code
        # inlined at -O2/-O3 (https://github.com/chriskohlhoff/asio/issues/1616).
        # Demote to warning so genuine null-deref issues in our code still surface.
        -Wno-error=null-dereference
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /WX
        /permissive-
    >
)
