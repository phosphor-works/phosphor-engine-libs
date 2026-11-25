# SPDX-FileCopyrightText: 2026 Phosphor Works Team
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Phosphor engine libraries: the window-placement engines (snap, tile,
# scroll), their zone, tiling-algorithm and workspace models, placement state,
# the zone overlays and the engine runtime a host embeds. They build on
# phosphor-libs.
#
# Two ways in:
#
#   * On its own. cmake --workflow --preset dev configures, builds and tests
#     every library here. phosphor-libs comes from the commit
#     cmake/PhosphorEngineLibsDependencies.cmake pins, fetched with CPM, or from
#     a local checkout with -DCPM_phosphor-libs_SOURCE=<dir>.
#   * As a subproject of an app. PlasmaZones pins one commit of this repository
#     and adds it with CPM after phosphor-libs, so the app's phosphor-libs pin
#     wins and this tree fetches nothing of its own. CPM's
#     -DCPM_phosphor-engine-libs_SOURCE=<dir> builds a local checkout instead.
#
# As a subproject the app's top level has already set up ECM, the KDE settings,
# Qt and the compile definitions below. Each of those steps here is either
# idempotent or runs only at the top level, so the app's settings are the ones
# in force.
#
# Each library keeps its own CMakeLists, project() metadata and find_package
# config, so it still configures alone from its own directory. Add new
# libraries under libs/ following the phosphor-<name> convention.

# 3.23: the phosphor-libs tree added below asks for it (target_sources with
# FILE_SET). The presets (CMakePresets.json) need 3.25.
cmake_minimum_required(VERSION 3.23)

project(PhosphorEngineLibs VERSION 0.1.0 LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PHOSPHOR_ENGINE_LIBS_TOP_LEVEL ON)
else()
    set(PHOSPHOR_ENGINE_LIBS_TOP_LEVEL OFF)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Top-level setup. Skipped as a subproject, where the app has done it.
# ═══════════════════════════════════════════════════════════════════════════════

if(PHOSPHOR_ENGINE_LIBS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC_PARALLEL_ENABLED ON)

    option(CMAKE_UNITY_BUILD "Enable unity builds for faster compilation" ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 16 CACHE STRING "Number of sources per unity batch")
    option(ENABLE_PCH "Share one precompiled header across the test targets" ON)

    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    endif()

    set(QT_MIN_VERSION "6.10.0")
    set(KF_MIN_VERSION "6.26.0")

    find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

    # Must come before KDECMakeSettings, which includes CTest and locks
    # BUILD_TESTING into the cache as ON.
    option(BUILD_TESTING "Build the test suite" OFF)

    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings NO_POLICY_SCOPE)
    include(FeatureSummary)

    # Makes the Qt6 string-literal rule a compile error rather than a
    # convention: no implicit const char* to QString.
    add_compile_definitions(QT_NO_CAST_FROM_ASCII)

    include(KDEClangFormat)
    file(GLOB_RECURSE PHOSPHOR_ENGINE_LIBS_CLANG_FORMAT_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.hpp
    )
    kde_clang_format(${PHOSPHOR_ENGINE_LIBS_CLANG_FORMAT_FILES})
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Shared lookups. find_package is idempotent, so these also run as a subproject.
# ═══════════════════════════════════════════════════════════════════════════════
# At this scope, before phosphor-libs is added. An IMPORTED target belongs to the
# directory that found it, and phosphor-libs' own top level, which repeats these
# lookups, is a subdirectory here: the targets it found there (Qt's private
# modules) would be invisible to the libraries below.

if(NOT DEFINED QT_MIN_VERSION)
    set(QT_MIN_VERSION "6.10.0")
endif()

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Concurrent
    Core
    DBus
    Gui
    Qml
    Quick
    QuickControls2
    ShaderTools
    Svg
    WaylandClient
    Widgets
)

# CMake's if(POLICY) knows only CMP policies. Qt's existence test for its own
# QTP policies is the QT_KNOWN_POLICY_<policy> variable.
if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# The private Qt modules the libraries use (QRhi for the zone overlays,
# QuickPrivate, the Wayland QPA plugin through phosphor-libs). Looked up here,
# once, so each IMPORTED target is visible to every library and consumer.
# Some distros (Fedora) create these targets inside the public module's
# Config file instead of shipping standalone ones, hence the TARGET checks.
foreach(_phosphor_private GuiPrivate ShaderToolsPrivate QuickPrivate WaylandClientPrivate)
    if(NOT TARGET Qt6::${_phosphor_private})
        find_package(Qt6${_phosphor_private} CONFIG REQUIRED)
    endif()
endforeach()

# ═══════════════════════════════════════════════════════════════════════════════
# phosphor-libs
# ═══════════════════════════════════════════════════════════════════════════════
# The libraries every engine library links. Added first, so each find_package
# below resolves to its in-tree target.

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PhosphorEngineLibsDependencies.cmake")
phosphor_engine_libs_add_phosphor_libs()

# The shared test helpers (PhosphorTestIsolation, PhosphorLibTesting,
# PhosphorTestPch) live in phosphor-libs, reachable by name from here on.
list(APPEND CMAKE_MODULE_PATH "${PHOSPHOR_LIBS_CMAKE_DIR}")

# ═══════════════════════════════════════════════════════════════════════════════
# Libraries
# ═══════════════════════════════════════════════════════════════════════════════
#
# Declaration order matters where a library find_package()s a sibling: the
# in-tree target must exist before the consumer configures, or the lookup
# resolves a stale installed copy. Everything these link from phosphor-libs
# (identity, protocol, layout-api, geometry, screens, rules, config, registry,
# scripting, rendering, shaders, animation, layer, overlay, surfaces) is
# declared by the time it is added above.

add_subdirectory(libs/phosphor-zones)            # zone primitives + LayoutManager (depends on phosphor-config IBackend, phosphor-screens ScreenIdentity)
add_subdirectory(libs/phosphor-context-resolver) # frozen-snapshot per-screen mode + disable/lock cascade façade (depends on phosphor-zones for AssignmentEntry::Mode)
add_subdirectory(libs/phosphor-tiles)            # auto-tiling algorithm primitives (TilingAlgorithm, AlgorithmRegistry, SplitTree)
# phosphor-workspaces links PhosphorEngine, and the engines link both, so
# these stay in this order.
add_subdirectory(libs/phosphor-engine)           # unified placement engine contracts + window registry (SHARED lib)
add_subdirectory(libs/phosphor-workspaces)       # virtual desktop / workspace management
add_subdirectory(libs/phosphor-tile-engine)      # autotile placement engine (IAutotileSettings + engine code)
add_subdirectory(libs/phosphor-snap-engine)      # zone-based snap placement engine
add_subdirectory(libs/phosphor-scroll-engine)    # niri-style scrolling placement engine (strip model + engine)
add_subdirectory(libs/phosphor-placement)        # window placement state tracking
add_subdirectory(libs/phosphor-zone-rendering)   # zone overlays drawn with shader packs (ZoneShaderItem) - depends on rendering/shaders/zones
add_subdirectory(libs/phosphor-engine-runtime)   # the placement engine runtime a host embeds - depends on the engines, zones, screens, config, animation, layer/overlay/surfaces and zone rendering

# ═══════════════════════════════════════════════════════════════════════════════
# Data
# ═══════════════════════════════════════════════════════════════════════════════
# The data the engines load: the snapping layouts, the Luau tiling algorithms
# and the scrolling templates. They install under the suite folder, where the
# engine runtime finds them through XDG_DATA_DIRS whichever host embeds it. The
# rules run as a subproject too, so an app that adds this tree ships them.
# PHOSPHOR_ENGINE_LIBS_DATA_DIR names the source tree, for an app's tests and
# checks that read the bundled files.

set(PHOSPHOR_ENGINE_LIBS_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data" CACHE INTERNAL "")

include("${PHOSPHOR_LIBS_CMAKE_DIR}/PhosphorSuite.cmake")
if(NOT DEFINED KDE_INSTALL_DATADIR)
    include(GNUInstallDirs)
    set(KDE_INSTALL_DATADIR ${CMAKE_INSTALL_DATAROOTDIR})
endif()

install(DIRECTORY data/layouts/
    DESTINATION ${KDE_INSTALL_DATADIR}/${PHOSPHOR_SUITE_DIRECTORY_NAME}/layouts
    FILES_MATCHING PATTERN "*.json"
)
install(DIRECTORY data/scrolling-templates/
    DESTINATION ${KDE_INSTALL_DATADIR}/${PHOSPHOR_SUITE_DIRECTORY_NAME}/scrolling-templates
    FILES_MATCHING PATTERN "*.json"
)
install(DIRECTORY data/algorithms/
    DESTINATION ${KDE_INSTALL_DATADIR}/${PHOSPHOR_SUITE_DIRECTORY_NAME}/algorithms
    FILES_MATCHING PATTERN "*.luau"
)

# ═══════════════════════════════════════════════════════════════════════════════
# Engine QML firewall
# ═══════════════════════════════════════════════════════════════════════════════

# Canonical list of engine library target names. Consumed by the
# engine QML firewall below. Kept in sync with the add_subdirectory calls
# above by hand; the firewall FATAL_ERRORs naming any
# entry that matches no target, so a single renamed engine cannot quietly
# stop being guarded.
set(PHOSPHOR_ENGINE_TARGETS
    PhosphorZones
    PhosphorSnapEngine
    PhosphorTileEngine
    PhosphorScrollEngine
    PhosphorEngine
    PhosphorWorkspaces
    PhosphorWorkspacesPlasma
    PhosphorPlacement
)

# ─── Engine QML firewall ─────────────────────────────────────────────────────
#
# Engine libraries hold authoritative placement/snap/zone state and must
# stay free of the QML/JS engine. The moment one of them links Qt6::Qml or
# Qt6::Quick, the cost model changes: every QObject in the engine gets a
# JS scope, bindings cross language boundaries, and ownership becomes
# implicit through QQmlEngine. That's the trajectory Noctalia couldn't
# escape, their engines and config were both QML, so multi-day uptime
# leaks (FD exhaustion, GC pauses, JSValue retention across reloads) had
# nowhere to be fixed without a full rewrite.
#
# Refuse to configure if any engine target reaches Qml/Quick anywhere in its
# link closure: its own direct links (PUBLIC and PRIVATE), then the
# INTERFACE_LINK_LIBRARIES of every target those name, recursively, through
# in-tree libraries and imported Qt targets alike. A check over the direct
# lists alone let PhosphorRegistry's PUBLIC Qt6::Qml reach every engine
# through PhosphorEngine -> PhosphorLayoutApi -> PhosphorRegistry, so the
# engines compiled with QT_QML_LIB while this guard reported them clean. The
# failure names the whole chain, so the offending edge is visible.
#
# Every entry must also match a real target, so the firewall can't
# silently degrade if PHOSPHOR_ENGINE_TARGETS goes stale (e.g. an engine
# lib gets renamed but the list doesn't follow). An unmatched entry would
# let a future Qml link into that engine through unnoticed, which is
# exactly the failure mode the rationale above is designed to prevent.

# Strip the link-list wrappers that carry a target name through to the link
# step, and answer the bare item. Directory-scope markers (`::@...`, recorded
# when target_link_libraries runs in a directory other than the target's) and
# install-only entries answer empty, since neither names a build-tree target.
function(_phosphor_engine_link_item _raw _out)
    set(_item "${_raw}")
    if(_item MATCHES "^::@" OR _item MATCHES "^\\$<INSTALL_INTERFACE:")
        set(${_out} "" PARENT_SCOPE)
        return()
    endif()
    while(_item MATCHES "^\\$<(LINK_ONLY|BUILD_INTERFACE|BUILD_LOCAL_INTERFACE):(.*)>$")
        set(_item "${CMAKE_MATCH_2}")
    endwhile()
    if(TARGET "${_item}")
        get_target_property(_aliased "${_item}" ALIASED_TARGET)
        if(_aliased)
            set(_item "${_aliased}")
        endif()
    endif()
    set(${_out} "${_item}" PARENT_SCOPE)
endfunction()

# Breadth-first walk of <engine>'s link closure. Sets <out> to the chain
# "engine -> ... -> Qt6::Qml" for the first Qml/Quick target reached, or to
# empty when the closure is clean. Each target is visited once, so a diamond in
# the graph costs nothing extra and a cycle cannot loop.
function(_phosphor_engine_qml_chain _engine _out)
    set(_queue "")
    set(_seen "${_engine}")
    get_target_property(_direct ${_engine} LINK_LIBRARIES)
    if(NOT _direct)
        set(_direct "")
    endif()
    get_target_property(_iface ${_engine} INTERFACE_LINK_LIBRARIES)
    if(_iface)
        list(APPEND _direct ${_iface})
    endif()
    foreach(_raw IN LISTS _direct)
        _phosphor_engine_link_item("${_raw}" _item)
        if(_item STREQUAL "" OR _item IN_LIST _seen)
            continue()
        endif()
        list(APPEND _seen "${_item}")
        list(APPEND _queue "${_item}")
        string(MAKE_C_IDENTIFIER "${_item}" _key)
        set(_p_via_${_key} "${_engine}")
    endforeach()
    while(_queue)
        list(POP_FRONT _queue _node)
        if(_node MATCHES "^Qt6?::(Qml|Quick)(Private)?$")
            set(_chain "${_node}")
            set(_cursor "${_node}")
            while(NOT _cursor STREQUAL _engine)
                string(MAKE_C_IDENTIFIER "${_cursor}" _key)
                set(_cursor "${_p_via_${_key}}")
                set(_chain "${_cursor} -> ${_chain}")
            endwhile()
            set(${_out} "${_chain}" PARENT_SCOPE)
            return()
        endif()
        if(NOT TARGET "${_node}")
            continue()
        endif()
        get_target_property(_next "${_node}" INTERFACE_LINK_LIBRARIES)
        if(NOT _next)
            continue()
        endif()
        foreach(_raw IN LISTS _next)
            _phosphor_engine_link_item("${_raw}" _item)
            if(_item STREQUAL "" OR _item IN_LIST _seen)
                continue()
            endif()
            list(APPEND _seen "${_item}")
            list(APPEND _queue "${_item}")
            string(MAKE_C_IDENTIFIER "${_item}" _key)
            set(_p_via_${_key} "${_node}")
        endforeach()
    endwhile()
    set(${_out} "" PARENT_SCOPE)
endfunction()

set(_p_engine_unmatched "")
foreach(_p_engine_lib IN LISTS PHOSPHOR_ENGINE_TARGETS)
    if(NOT TARGET ${_p_engine_lib})
        list(APPEND _p_engine_unmatched ${_p_engine_lib})
        continue()
    endif()
    _phosphor_engine_qml_chain(${_p_engine_lib} _p_engine_chain)
    if(_p_engine_chain)
        message(FATAL_ERROR
            "Engine library '${_p_engine_lib}' reaches the QML engine through "
            "its link closure: ${_p_engine_chain}. Engine libs must stay "
            "QML-free, see the comment above this check in phosphor-engine-libs' CMakeLists.txt "
            "for the rationale. Drop the Qml/Quick edge from the library that "
            "carries it, or move the engine's dependency off that library.")
    endif()
endforeach()
if(_p_engine_unmatched)
    # PER-ENTRY, not "did anything match". Every name in the list is an
    # unconditional target by this point in the file, so a name that matches
    # nothing means that ONE engine silently stopped being guarded while the
    # others still are — the realistic staleness mode, and one a
    # matched-anything check sails straight past.
    message(FATAL_ERROR
        "Engine QML firewall matched no target for: ${_p_engine_unmatched}. "
        "Those entries of PHOSPHOR_ENGINE_TARGETS guard nothing, so a Qml "
        "link into them would go unnoticed. Either the library was renamed "
        "without updating the list, the add_subdirectory order changed so it "
        "has not been declared yet at this point, or it got gated behind an "
        "option that is currently OFF. Update PHOSPHOR_ENGINE_TARGETS to "
        "match the real engine target names, or move this firewall block "
        "below the add_subdirectory calls that declare the engines.")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Top-level finish
# ═══════════════════════════════════════════════════════════════════════════════

if(PHOSPHOR_ENGINE_LIBS_TOP_LEVEL)
    # One precompiled header shared by every test target, phosphor-libs' as
    # well. An app that adds this tree makes the same call itself.
    if(BUILD_TESTING AND ENABLE_PCH)
        include(PhosphorTestPch)
        phosphor_share_test_pch()
    endif()

    feature_summary(WHAT REQUIRED_PACKAGES_FOUND REQUIRED_PACKAGES_NOT_FOUND
                         OPTIONAL_PACKAGES_FOUND OPTIONAL_PACKAGES_NOT_FOUND
                    FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
