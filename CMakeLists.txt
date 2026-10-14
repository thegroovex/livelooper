cmake_minimum_required(VERSION 3.22)

project(livelooper VERSION 1.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(LIVELOOPER_BUILD_TESTS "Build the test executables" ON)
option(LIVELOOPER_COPY_PLUGIN "Copy the plugin to the system plug-in folder after build" OFF)
set(LIVELOOPER_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (otherwise JUCE is downloaded)")

# ---------------------------------------------------------------- JUCE
if(LIVELOOPER_JUCE_PATH)
    add_subdirectory(${LIVELOOPER_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------- build info (About window)
find_package(Git QUIET)
set(LIVELOOPER_GIT_HASH "unknown")
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE _ll_hash OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _ll_res)
    if(_ll_res EQUAL 0 AND _ll_hash)
        set(LIVELOOPER_GIT_HASH "${_ll_hash}")
    endif()
endif()
if(DEFINED ENV{GITHUB_RUN_NUMBER})
    set(LIVELOOPER_BUILD_NUMBER "$ENV{GITHUB_RUN_NUMBER}")
else()
    set(LIVELOOPER_BUILD_NUMBER "local")
endif()
string(TIMESTAMP LIVELOOPER_BUILD_DATE "%Y-%m-%d" UTC)
configure_file(src/BuildInfo.h.in ${CMAKE_BINARY_DIR}/generated/BuildInfo.h @ONLY)

# ---------------------------------------------------------------- Ableton Link (GPLv2+, submodule third_party/link)
option(LIVELOOPER_WITH_LINK "Build with Ableton Link" ON)
set(LIVELOOPER_LINK_TARGET "")
set(LIVELOOPER_LINK_DEFINES "")
if(LIVELOOPER_WITH_LINK)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/link/AbletonLinkConfig.cmake)
        message(FATAL_ERROR "Ableton Link is missing: run  git submodule update --init --recursive  (or configure with -DLIVELOOPER_WITH_LINK=OFF)")
    endif()
    include(${CMAKE_CURRENT_SOURCE_DIR}/third_party/link/AbletonLinkConfig.cmake)
    set(LIVELOOPER_LINK_TARGET Ableton::Link)
    set(LIVELOOPER_LINK_DEFINES LIVELOOPER_WITH_LINK=1)
    if(WIN32)
        list(APPEND LIVELOOPER_LINK_DEFINES _WIN32_WINNT=0x0601)
    endif()
endif()

# ---------------------------------------------------------------- embedded resources (help, licences, logo, drum samples)
file(GLOB LIVELOOPER_DRUM_SAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/resources/drums/*.flac)
set(_ll_help ${CMAKE_BINARY_DIR}/help)
configure_file(docs/en/PUSH.md ${_ll_help}/PUSH-en.md COPYONLY)
configure_file(docs/de/PUSH.md ${_ll_help}/PUSH-de.md COPYONLY)
configure_file(docs/en/MIDI.md ${_ll_help}/MIDI-en.md COPYONLY)
configure_file(docs/de/MIDI.md ${_ll_help}/MIDI-de.md COPYONLY)
configure_file(docs/en/INSTALLATION.md ${_ll_help}/INSTALLATION-en.md COPYONLY)
configure_file(docs/de/INSTALLATION.md ${_ll_help}/INSTALLATION-de.md COPYONLY)
juce_add_binary_data(livelooper_resources
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        docs/en/USER-GUIDE.md
        docs/de/BEDIENUNGSANLEITUNG.md
        ${_ll_help}/PUSH-en.md ${_ll_help}/PUSH-de.md
        ${_ll_help}/MIDI-en.md ${_ll_help}/MIDI-de.md
        ${_ll_help}/INSTALLATION-en.md ${_ll_help}/INSTALLATION-de.md
        LICENSE
        THIRD-PARTY.md
        docs/brand/beattactics-logo-transparent.png
        ${LIVELOOPER_DRUM_SAMPLES})
set_target_properties(livelooper_resources PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------- shared sources
# The engine is UI-free DSP code (only juce_audio_basics); the plugin sources add
# processor, parameters and editor. Both lists are compiled into every target that
# needs them (JUCE modules must not be linked into intermediate static libraries).
set(LIVELOOPER_THIRD_PARTY_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/signalsmith-stretch/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/signalsmith-linear/include
    ${CMAKE_BINARY_DIR}/generated)

set(LIVELOOPER_ENGINE_SOURCES
    src/engine/LoopTrack.cpp
    src/engine/LooperEngine.cpp
    src/engine/Effects.cpp
    src/engine/RhythmGenerator.cpp
    src/engine/MidiMap.cpp
    src/engine/Performance.cpp
    src/engine/BeatFx.cpp)
set(LIVELOOPER_PLUGIN_SOURCES
    src/plugin/Localisation.cpp
    src/plugin/AppSettings.cpp
    src/plugin/Updates.cpp
    src/plugin/InfoWindows.cpp
    src/plugin/DrumSampleLoader.cpp
    src/plugin/LinkSync.cpp
    src/plugin/KeyMap.cpp
    src/plugin/KeyMapEditor.cpp
    src/plugin/Parameters.cpp
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/LooperLookAndFeel.cpp
    src/plugin/Widgets.cpp
    src/plugin/push/PushProtocol.cpp
    src/plugin/push/PushSurface.cpp
    src/plugin/push/PushDevice.cpp)
set(LIVELOOPER_DEFINES LIVELOOPER_VERSION="${PROJECT_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    ${LIVELOOPER_LINK_DEFINES})
set(LIVELOOPER_MODULES juce::juce_audio_utils juce::juce_audio_formats juce::juce_gui_extra juce::juce_dsp juce::juce_cryptography livelooper_resources ${LIVELOOPER_LINK_TARGET})

# ---------------------------------------------------------------- plugin
set(LIVELOOPER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND LIVELOOPER_FORMATS AU)
endif()

juce_add_plugin(livelooper
    PRODUCT_NAME "livelooper"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/livelooper-icon-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/livelooper-icon-256.png"
    COMPANY_NAME "groovex"
    BUNDLE_ID "com.groovex.livelooper"
    PLUGIN_MANUFACTURER_CODE Grvx
    PLUGIN_CODE Lvlp
    FORMATS ${LIVELOOPER_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "livelooper records audio from your microphone or audio interface into its loop tracks."
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD ${LIVELOOPER_COPY_PLUGIN}
    DESCRIPTION "Five-track live looper with effects, rhythm and full MIDI mapping")

target_sources(livelooper PRIVATE ${LIVELOOPER_ENGINE_SOURCES} ${LIVELOOPER_PLUGIN_SOURCES})
target_include_directories(livelooper PRIVATE src ${LIVELOOPER_THIRD_PARTY_INCLUDES})
target_compile_definitions(livelooper PUBLIC ${LIVELOOPER_DEFINES})

target_link_libraries(livelooper
    PRIVATE
        ${LIVELOOPER_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- tests
if(LIVELOOPER_BUILD_TESTS)
    enable_testing()

    # Engine + processor unit tests (no plug-in host involved)
    juce_add_console_app(livelooper_tests PRODUCT_NAME "livelooper_tests")
    target_sources(livelooper_tests PRIVATE
        tests/TestMain.cpp
        tests/EngineTests.cpp
        tests/EffectsTests.cpp
        tests/MidiMapTests.cpp
        tests/ProcessorTests.cpp
        tests/LocalisationTests.cpp
        tests/KeyboardTests.cpp
        tests/FeatureTests.cpp
        tests/Feature13Tests.cpp
        tests/Feature14Tests.cpp
        tests/PushTests.cpp
        tests/Push14Tests.cpp
        tests/RhythmTests.cpp
        tests/BeatFxTests.cpp
        ${LIVELOOPER_ENGINE_SOURCES} ${LIVELOOPER_PLUGIN_SOURCES})
    target_include_directories(livelooper_tests PRIVATE src ${LIVELOOPER_THIRD_PARTY_INCLUDES})
    target_compile_definitions(livelooper_tests PRIVATE ${LIVELOOPER_DEFINES} LIVELOOPER_NO_PLUGIN_ENTRY=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(livelooper_tests PRIVATE ${LIVELOOPER_MODULES}
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME unit_tests COMMAND livelooper_tests)

    # Loads the built VST3 through JUCE's plug-in hosting code and drives it like a DAW would
    juce_add_console_app(livelooper_hosttest PRODUCT_NAME "livelooper_hosttest")
    target_sources(livelooper_hosttest PRIVATE tests/HostTest.cpp)
    target_compile_definitions(livelooper_hosttest PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(livelooper_hosttest PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
    add_dependencies(livelooper_hosttest livelooper_VST3)
    add_test(NAME vst3_host_test
             COMMAND livelooper_hosttest "${CMAKE_BINARY_DIR}/livelooper_artefacts/$<CONFIG>/VST3/livelooper.vst3")
    set_tests_properties(vst3_host_test PROPERTIES ENVIRONMENT "LIVELOOPER_LANGUAGE=en")

    # Renders the editor into a PNG (used for the documentation screenshots)
    juce_add_console_app(livelooper_screenshot PRODUCT_NAME "livelooper_screenshot")
    target_sources(livelooper_screenshot PRIVATE tools/Screenshot.cpp
        ${LIVELOOPER_ENGINE_SOURCES} ${LIVELOOPER_PLUGIN_SOURCES})
    target_include_directories(livelooper_screenshot PRIVATE src ${LIVELOOPER_THIRD_PARTY_INCLUDES})
    target_compile_definitions(livelooper_screenshot PRIVATE ${LIVELOOPER_DEFINES} LIVELOOPER_NO_PLUGIN_ENTRY=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(livelooper_screenshot PRIVATE ${LIVELOOPER_MODULES} juce::juce_recommended_config_flags)
endif()
