# SPDX-FileCopyrightText: 2026 fuddlesworth
# SPDX-License-Identifier: LGPL-2.1-or-later

# PhosphorEngine unit tests. Headless — pure value/helper logic, no QPA needed.

find_package(Qt6 REQUIRED COMPONENTS Test)

# Shared test isolation: a private D-Bus session (so a test that touches D-Bus
# cannot activate the INSTALLED daemon, which then holds the test's stdout pipe
# open and hangs ctest after the test itself passed) plus a per-target XDG
# sandbox (so nothing writes into the developer's real ~/.config/plasmazones).
# The module lives in phosphor-libs, which this tree adds first and which
# names its cmake/ directory in PHOSPHOR_LIBS_CMAKE_DIR. The guard covers a
# standalone configure of this library directory, where nothing has set it.
if(PHOSPHOR_LIBS_CMAKE_DIR AND EXISTS "${PHOSPHOR_LIBS_CMAKE_DIR}/PhosphorTestIsolation.cmake")
    include(${PHOSPHOR_LIBS_CMAKE_DIR}/PhosphorTestIsolation.cmake)
else()
    # Stand-ins so the call sites below need no guard of their own.
    # phosphor_append_test_environment is reproduced EXACTLY, because it is
    # self-contained and because callers use it for things like
    # QT_QPA_PLATFORM=offscreen that the tests need to pass at all — a no-op
    # there would break them rather than merely un-isolate them. The isolation
    # helper degrades to nothing: its private D-Bus session and XDG sandbox need
    # module-level state that does not exist without the module.
    function(phosphor_apply_test_isolation _target)
    endfunction()
    function(phosphor_append_test_environment _test_name)
        set_property(TEST ${_test_name} APPEND PROPERTY ENVIRONMENT ${ARGN})
    endfunction()
endif()
function(pe_add_test _name)
    add_executable(${_name} ${ARGN})
    set_target_properties(${_name} PROPERTIES AUTOMOC ON)
    target_compile_features(${_name} PRIVATE cxx_std_20)
    target_link_libraries(${_name}
        PRIVATE
            Qt6::Test
            Qt6::Core
            PhosphorEngine::PhosphorEngine
    )
    add_test(NAME ${_name} COMMAND ${_name})
    phosphor_apply_test_isolation(${_name})
    set_tests_properties(${_name} PROPERTIES LABELS "phosphorengine")
endfunction()

pe_add_test(test_engine_screencontexttracker test_screencontexttracker.cpp)
pe_add_test(test_engine_iplacementengine_defaults test_iplacementengine_defaults.cpp)
pe_add_test(test_engine_perscreenstates test_perscreenstates.cpp)
pe_add_test(test_engine_windowcontext test_windowcontext.cpp)
pe_add_test(test_engine_layerfocusswitch test_layerfocusswitch.cpp)
pe_add_test(test_engine_desktopremap test_desktopremap.cpp)
pe_add_test(test_engine_windowregistry test_windowregistry.cpp)
