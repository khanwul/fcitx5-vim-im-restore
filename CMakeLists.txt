cmake_minimum_required(VERSION 3.20)
project(fcitx5-vim-im-restore VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    option(FCITX_INSTALL_USE_FCITX_SYS_PATHS "Use Fcitx's installed addon directories" ON)
endif()
find_package(Fcitx5Core 5.1.13 REQUIRED)

add_library(viescape MODULE src/addon.cpp src/commands.cpp)
target_compile_features(viescape PRIVATE cxx_std_20)
target_link_libraries(viescape PRIVATE Fcitx5::Core)
set_target_properties(viescape PROPERTIES CXX_VISIBILITY_PRESET hidden
                      VISIBILITY_INLINES_HIDDEN YES)
configure_file(data/viescape.conf.in addon/viescape.conf @ONLY)
install(TARGETS viescape LIBRARY DESTINATION "${FCITX_INSTALL_ADDONDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/addon/viescape.conf"
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

option(BUILD_TESTING "Build the addon tests" OFF)
if(BUILD_TESTING)
    include(CTest)
    add_subdirectory(tests)
endif()
