cmake_minimum_required(VERSION 3.20)
project(RedLight VERSION 0.6.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "RedLight requires a native Windows x64 build.")
endif()
if(MSVC AND NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    message(FATAL_ERROR "Select the x64 C++ toolchain (cmake -A x64).")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Portable release: no separate Visual C++ runtime installer required.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
configure_file(src/Version.h.in generated/Version.h @ONLY)
option(REDLIGHT_EMBED_SOURCE_REVISION "Include local source revision/state in support reports" ON)
add_custom_target(RedLightBuildInfo
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DOUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/generated/BuildInfo.h"
        "-DEMBED_SOURCE_REVISION=${REDLIGHT_EMBED_SOURCE_REVISION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildInfo.h"
    COMMENT "Read local source identity for support reports"
    VERBATIM
)

add_executable(RedLight WIN32
    src/Diagnostics.cpp src/Clipboard.cpp src/PreferencesStore.cpp src/TrayMenu.cpp src/SessionLimitDialog.cpp src/HotkeyManager.cpp src/StandardOutput.cpp src/StatusQuery.cpp src/StartupOptions.cpp src/ProfileValidation.cpp src/ProfileExport.cpp src/SessionNotifications.cpp src/BuildMetadata.cpp src/ProfileComparison.cpp src/ProfileSave.cpp src/DisplayContext.cpp src/SavedSettings.cpp src/CommandHelp.cpp src/StatusClipboard.cpp src/RecentDiagnosticsClipboard.cpp src/main.cpp
    src/GammaRampFilter.cpp src/MagnificationFilter.cpp src/TrayCommand.cpp src/PreferencesClipboard.cpp src/CurrentPreferences.cpp src/PreferencesQuery.cpp src/CountdownTimerRegistration.cpp src/RestorationHistoryJson.cpp src/RestorationHistoryClipboard.cpp src/VerificationJson.cpp src/VerificationClipboard.cpp src/ProcessContext.cpp
    src/TrayIcons.cpp src/BrightnessDialog.cpp src/StartupRegistration.cpp src/ScheduleClock.cpp src/ScheduleSession.cpp src/ScheduleDialog.cpp resources/app.rc
)
add_dependencies(RedLight RedLightBuildInfo)
target_include_directories(RedLight PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" resources)
target_compile_definitions(RedLight PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
target_link_libraries(RedLight PRIVATE user32 gdi32 Shell32 Magnification Wtsapi32 Advapi32)
if(MSVC)
    target_compile_options(RedLight PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_link_options(RedLight PRIVATE /MANIFEST:NO /DYNAMICBASE /NXCOMPAT)
endif()

option(REDLIGHT_BUILD_PROBE "Build the manual display probe (never a CTest test)" OFF)
if(REDLIGHT_BUILD_PROBE)
    add_executable(MagRedProbe experiments/MagRedProbe.cpp)
    target_link_libraries(MagRedProbe PRIVATE Magnification)
endif()

option(REDLIGHT_BUILD_WINDOWED_CONTROL "Compile isolated windowed control research library (no executable)" OFF)
if(REDLIGHT_BUILD_WINDOWED_CONTROL)
    add_library(RedLightWindowedControl STATIC experiments/WindowedRedControl.cpp experiments/WindowedTopology.cpp)
    target_include_directories(RedLightWindowedControl PRIVATE src)
    target_link_libraries(RedLightWindowedControl PRIVATE user32 Magnification)
    target_compile_definitions(RedLightWindowedControl PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
    if(MSVC)
        target_compile_options(RedLightWindowedControl PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(RedLightCodeTests
        tests/CodeTests.cpp tests/MemoryFault.cpp src/MagnificationFilter.cpp src/GammaRampFilter.cpp src/Clipboard.cpp src/PreferencesStore.cpp src/HotkeyManager.cpp src/StandardOutput.cpp src/StatusQuery.cpp src/StartupOptions.cpp src/ProfileValidation.cpp src/ProfileExport.cpp src/SessionNotifications.cpp src/BuildMetadata.cpp src/ProfileComparison.cpp src/ProfileSave.cpp src/DisplayContext.cpp src/SavedSettings.cpp src/CommandHelp.cpp src/StatusClipboard.cpp src/RecentDiagnosticsClipboard.cpp src/TrayMenu.cpp)
    # Fake headers provide every display API: this target cannot call real display APIs.
    target_include_directories(RedLightCodeTests BEFORE PRIVATE tests/fake_win32 src resources)
    target_compile_definitions(RedLightCodeTests PRIVATE REDLIGHT_FAKE_WIN32=1)
    target_sources(RedLightCodeTests PRIVATE src/TrayIcons.cpp src/StartupRegistration.cpp src/ScheduleClock.cpp src/ScheduleSession.cpp)
    target_sources(RedLightCodeTests PRIVATE src/TrayCommand.cpp src/PreferencesClipboard.cpp src/CurrentPreferences.cpp src/PreferencesQuery.cpp src/CountdownTimerRegistration.cpp src/RestorationHistoryJson.cpp src/RestorationHistoryClipboard.cpp src/VerificationJson.cpp src/VerificationClipboard.cpp src/ProcessContext.cpp)
    if(MSVC)
        target_compile_options(RedLightCodeTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    endif()
    add_executable(RedLightDiagnosticsCodeTests tests/DiagnosticsTests.cpp tests/MemoryFault.cpp src/Diagnostics.cpp)
    target_include_directories(RedLightDiagnosticsCodeTests BEFORE PRIVATE tests/fake_win32 src)
    target_compile_definitions(RedLightDiagnosticsCodeTests PRIVATE REDLIGHT_FAKE_WIN32=1)
    if(MSVC)
        target_compile_options(RedLightDiagnosticsCodeTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    endif()
    add_test(NAME RedLight.DiagnosticsCodeTests COMMAND RedLightDiagnosticsCodeTests)
    set_tests_properties(RedLight.DiagnosticsCodeTests PROPERTIES LABELS "code-only;no-display" TIMEOUT 30)
    add_test(NAME RedLight.CodeTests COMMAND RedLightCodeTests)
    set_tests_properties(RedLight.CodeTests PROPERTIES LABELS "code-only;no-display")
    add_executable(RedLightWindowedControlCodeTests tests/WindowedControlTests.cpp tests/MemoryFault.cpp experiments/WindowedRedControl.cpp)
    target_include_directories(RedLightWindowedControlCodeTests BEFORE PRIVATE tests/fake_win32 src experiments)
    target_compile_definitions(RedLightWindowedControlCodeTests PRIVATE REDLIGHT_FAKE_WIN32=1)
    if(MSVC)
        target_compile_options(RedLightWindowedControlCodeTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    endif()
    add_test(NAME RedLight.WindowedControlCodeTests COMMAND RedLightWindowedControlCodeTests)
    set_tests_properties(RedLight.WindowedControlCodeTests PROPERTIES LABELS "code-only;no-display" TIMEOUT 30)
    add_executable(RedLightWindowedTopologyCodeTests tests/WindowedTopologyTests.cpp tests/MemoryFault.cpp experiments/WindowedTopology.cpp)
    target_include_directories(RedLightWindowedTopologyCodeTests BEFORE PRIVATE tests/fake_win32 experiments)
    target_compile_definitions(RedLightWindowedTopologyCodeTests PRIVATE REDLIGHT_FAKE_WIN32=1)
    if(MSVC)
        target_compile_options(RedLightWindowedTopologyCodeTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    endif()
    add_test(NAME RedLight.WindowedTopologyCodeTests COMMAND RedLightWindowedTopologyCodeTests)
    set_tests_properties(RedLight.WindowedTopologyCodeTests PROPERTIES LABELS "code-only;no-display" TIMEOUT 30)
endif()

install(TARGETS RedLight RUNTIME DESTINATION .)
install(FILES README.md CHANGELOG.md LICENSE DESTINATION .)
install(CODE [[
    file(SHA256 "${CMAKE_INSTALL_PREFIX}/RedLight.exe" redlight_hash)
    file(WRITE "${CMAKE_INSTALL_PREFIX}/RedLight.exe.sha256" "${redlight_hash}  RedLight.exe\n")
]])
install(DIRECTORY docs/ DESTINATION docs)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME RedLight)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}-rc.1")
set(CPACK_PACKAGE_FILE_NAME "RedLight-${CPACK_PACKAGE_VERSION}-windows-x64")
include(CPack)
