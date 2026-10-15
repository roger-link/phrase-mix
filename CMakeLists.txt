cmake_minimum_required(VERSION 3.22)
project(PhraseMix VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout")
if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
 message(FATAL_ERROR "Set -DJUCE_PATH=/absolute/path/to/JUCE")
endif()
add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/juce")
juce_add_gui_app(PhraseMix
 PRODUCT_NAME "Phrase Mix"
 COMPANY_NAME "Phrase Mix"
 BUNDLE_ID "co.phrasemix.prototype"
 ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/AppIcon.png"
 ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/AppIcon.png"
 MICROPHONE_PERMISSION_ENABLED TRUE
 MICROPHONE_PERMISSION_TEXT "Phrase Mix records sounds from the microphone so you can use them as Found sources.")
target_sources(PhraseMix PRIVATE Source/Main.cpp)
file(GLOB SAMPLE_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Samples/*")
target_sources(PhraseMix PRIVATE ${SAMPLE_ASSETS})
set_source_files_properties(${SAMPLE_ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/Samples)
target_compile_definitions(PhraseMix PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(PhraseMix PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
