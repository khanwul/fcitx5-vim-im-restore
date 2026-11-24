add_executable(test-commands commands.cpp ../src/commands.cpp)
target_include_directories(test-commands PRIVATE ../src)
target_compile_features(test-commands PRIVATE cxx_std_20)
target_link_libraries(test-commands PRIVATE Fcitx5::Utils)
add_test(NAME command-sequences COMMAND test-commands)

find_package(Fcitx5ModuleTestFrontend REQUIRED)
find_package(Fcitx5ModuleTestIM REQUIRED)
add_executable(test-integration integration.cpp)
target_compile_features(test-integration PRIVATE cxx_std_20)
target_link_libraries(test-integration PRIVATE Fcitx5::Core
    Fcitx5::Module::TestFrontend Fcitx5::Module::TestIM)
add_dependencies(test-integration viescape)

# IM identities backed by Fcitx's controllable testing engine.
foreach(role primary secondary command)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/inputmethod/test-${role}.conf"
        "[InputMethod]\nName=Test ${role}\nLangCode=und\nAddon=testim\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/addon/escapefilter.conf"
    "[Addon]\nName=Test Escape filter\nType=StaticLibrary\nLibrary=escapefilter\nCategory=InputMethod\nOnDemand=True\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/inputmethod/test-filter.conf"
    "[InputMethod]\nName=Test Escape filter\nLangCode=und\nAddon=escapefilter\n")
foreach(scenario IN ITEMS switching translated-command scroll-restore
                          command-line-restore search-history-restore
                          single-substitution-restore modified-substitution-confirm
                          consumed-escape overrides contexts edited-search-restore
                          shared-override filtered-escape)
    add_test(NAME "frontend-${scenario}" COMMAND test-integration
             "${scenario}" "$<TARGET_FILE_DIR:viescape>"
             "${PROJECT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
set(COMPOSITION_TEST_CASE "" CACHE STRING
    "Optional composition test (addon ID;input method ID;ASCII keystrokes;expected text)")
if(COMPOSITION_TEST_CASE)
    add_test(NAME frontend-composition COMMAND test-integration composition
             "$<TARGET_FILE_DIR:viescape>" "${PROJECT_BINARY_DIR}"
             "${CMAKE_CURRENT_BINARY_DIR}" ${COMPOSITION_TEST_CASE})
endif()
