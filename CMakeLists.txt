cmake_minimum_required(VERSION 3.22)

project(ClassicMix VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7c89e11f6b7316c369f3d3f22227c60e816e738b
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(ClassicMix
    PRODUCT_NAME "Classic Mix"
    COMPANY_NAME "Classic Studio"
    BUNDLE_ID com.classicstudio.classicmix
    VERSION "0.1.0"
    NEEDS_FULL_SCREEN FALSE
)

target_sources(ClassicMix PRIVATE
    src/main.cpp
    src/analysis/AudioAnalyzer.cpp
    src/ai/MixPlanner.cpp
    src/ai/OpenAiMixService.cpp
    src/audio/MixSession.cpp
    src/audio/MixRenderer.cpp
    src/ui/MainComponent.cpp
)

target_include_directories(ClassicMix PRIVATE src)

target_compile_definitions(ClassicMix PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
)

target_link_libraries(ClassicMix PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_generate_juce_header(ClassicMix)
