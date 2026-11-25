# SPDX-FileCopyrightText: 2026 Phosphor Works Team
# SPDX-License-Identifier: LGPL-2.1-or-later

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
function(phosphorengineruntime_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name}
        PRIVATE
            PhosphorEngineRuntime::PhosphorEngineRuntime
            Qt6::Core
            Qt6::Gui
            Qt6::Test
    )
    add_test(NAME ${name} COMMAND ${name})
    phosphor_apply_test_isolation(${name})
    set_property(TEST ${name} APPEND PROPERTY LABELS "phosphorengineruntime")
    # The settings model reads the palette for its theme-following colours, so
    # the tests run a QGuiApplication, offscreen under CI.
    phosphor_append_test_environment(${name} "QT_QPA_PLATFORM=offscreen")
endfunction()

phosphorengineruntime_add_test(test_settings_model)
phosphorengineruntime_add_test(test_core_helpers)
phosphorengineruntime_add_test(test_drag_activation)
phosphorengineruntime_add_test(test_adopt_previous_locations)

# The bundled data in data/ at this tree's root: every layout and scrolling
# template against its schema and parser, every algorithm through the loader.
phosphorengineruntime_add_test(test_bundled_data)
target_link_libraries(test_bundled_data PRIVATE PhosphorFsLoader::PhosphorFsLoader PhosphorTiles::PhosphorTiles
                                                PhosphorZones::PhosphorZones)
target_compile_definitions(test_bundled_data
    PRIVATE "PHOSPHOR_ENGINE_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../../../data\"")
