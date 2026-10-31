cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED EDITOR_PRODUCT_NAME OR "${EDITOR_PRODUCT_NAME}" STREQUAL "")
    set(EDITOR_PRODUCT_NAME "ModelSwapperEditor")
endif()
if(NOT DEFINED EDITOR_BEAUTIFUL_NAME OR "${EDITOR_BEAUTIFUL_NAME}" STREQUAL "")
    set(EDITOR_BEAUTIFUL_NAME "Model Swapper Editor")
endif()

# This directory can also be configured directly to build just the editor with
# an existing Qt 6 installation, without the SKSE/CommonLib dependencies.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(${EDITOR_PRODUCT_NAME} VERSION 0.3.0 LANGUAGES CXX)
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

# Qt's public imageformat_png feature can remain enabled even when its PNG
# decoder is disabled. Windows splitter cursors require the actual decoder.
get_target_property(editor_qt_disabled_private_features Qt6::Gui QT_DISABLED_PRIVATE_FEATURES)
if("png" IN_LIST editor_qt_disabled_private_features)
    message(FATAL_ERROR
        "${EDITOR_BEAUTIFUL_NAME} requires Qt PNG support for Windows cursors and icons. "
        "Enable the qtbase[png] feature alongside widgets, then reconfigure and rebuild "
        "the dependencies and editor. For standalone builds, select a Qt installation with PNG support.")
endif()

set(editor_headers
    include/ConfigDocument.h
    include/EditorWindow.h
    include/JsonForm.h
)
set(editor_sources
    src/ConfigDocument.cpp
    src/EditorWindow.cpp
    src/JsonForm.cpp
    src/main.cpp
)

qt_add_executable(${EDITOR_PRODUCT_NAME} WIN32 ${editor_headers} ${editor_sources})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${editor_headers} ${editor_sources})

set_target_properties(${EDITOR_PRODUCT_NAME} PROPERTIES
    AUTOMOC ON
    CXX_EXTENSIONS OFF
    # Build here, then optionally deploy integrated builds to the Data root.
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_features(${EDITOR_PRODUCT_NAME} PRIVATE cxx_std_20)
target_include_directories(${EDITOR_PRODUCT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${EDITOR_PRODUCT_NAME} PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(${EDITOR_PRODUCT_NAME} PRIVATE /utf-8 /W4)
endif()

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    automaticGameFolderOutput(
        "${wildlander_output}" "${steam_owrt_output}" "${steam_mods_output}"
        TARGET "${EDITOR_PRODUCT_NAME}"
        DISPLAY_NAME "${EDITOR_BEAUTIFUL_NAME}"
        SUBDIRECTORY "."
    )
endif()

option(MS_EDITOR_BUILD_TESTS "Build the editor's document and widget tests" OFF)
if(MS_EDITOR_BUILD_TESTS)
    enable_testing()
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
        tests/ConfigDocumentTests.cpp
        tests/JsonFormTests.cpp
        tests/EditorWindowTests.cpp
    )
    add_executable(ModelSwapperEditorDocumentTests
        src/ConfigDocument.cpp
        include/ConfigDocument.h
        tests/ConfigDocumentTests.cpp
    )
    target_compile_features(ModelSwapperEditorDocumentTests PRIVATE cxx_std_20)
    target_link_libraries(ModelSwapperEditorDocumentTests PRIVATE Qt6::Core)
    if(MSVC)
        target_compile_options(ModelSwapperEditorDocumentTests PRIVATE /utf-8 /W4)
    endif()
    add_test(NAME ModelSwapperEditorDocumentTests COMMAND ModelSwapperEditorDocumentTests)

    add_executable(ModelSwapperEditorFormTests
        tests/JsonFormTests.cpp
        src/JsonForm.cpp
        include/JsonForm.h
        src/ConfigDocument.cpp
        include/ConfigDocument.h
    )
    set_target_properties(ModelSwapperEditorFormTests PROPERTIES AUTOMOC ON)
    target_compile_features(ModelSwapperEditorFormTests PRIVATE cxx_std_20)
    target_link_libraries(ModelSwapperEditorFormTests PRIVATE Qt6::Widgets)
    if(MSVC)
        target_compile_options(ModelSwapperEditorFormTests PRIVATE /utf-8 /W4)
    endif()

    add_executable(ModelSwapperEditorWindowTests
        tests/EditorWindowTests.cpp
        src/EditorWindow.cpp
        include/EditorWindow.h
        src/JsonForm.cpp
        include/JsonForm.h
        src/ConfigDocument.cpp
        include/ConfigDocument.h
    )
    set_target_properties(ModelSwapperEditorWindowTests PROPERTIES AUTOMOC ON)
    target_compile_features(ModelSwapperEditorWindowTests PRIVATE cxx_std_20)
    target_link_libraries(ModelSwapperEditorWindowTests PRIVATE Qt6::Widgets)
    if(MSVC)
        target_compile_options(ModelSwapperEditorWindowTests PRIVATE /utf-8 /W4)
    endif()

    foreach(editor_test_target IN ITEMS
        ModelSwapperEditorDocumentTests ModelSwapperEditorFormTests ModelSwapperEditorWindowTests)
        target_include_directories(${editor_test_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    endforeach()

    # Static Windows Qt distributions may omit the offscreen platform plugin.
    # The minimal platform runs the widget tests without opening desktop windows.
    set(editor_test_platform_args)
    if(TARGET Qt6::QMinimalIntegrationPlugin)
        qt_import_plugins(ModelSwapperEditorFormTests INCLUDE_BY_TYPE platforms Qt6::QMinimalIntegrationPlugin)
        if(TARGET Qt6::QWindowsIntegrationPlugin)
            # Native-platform screenshots use the same Windows fonts as the app.
            qt_import_plugins(ModelSwapperEditorWindowTests INCLUDE_BY_TYPE platforms
                Qt6::QMinimalIntegrationPlugin Qt6::QWindowsIntegrationPlugin)
        else()
            qt_import_plugins(ModelSwapperEditorWindowTests INCLUDE_BY_TYPE platforms Qt6::QMinimalIntegrationPlugin)
        endif()
        set(editor_test_platform_args -platform minimal)
    elseif(TARGET Qt6::QOffscreenIntegrationPlugin)
        foreach(test_target IN ITEMS ModelSwapperEditorFormTests ModelSwapperEditorWindowTests)
            qt_import_plugins(${test_target} INCLUDE_BY_TYPE platforms Qt6::QOffscreenIntegrationPlugin)
        endforeach()
        set(editor_test_platform_args -platform offscreen)
    endif()
    add_test(NAME ModelSwapperEditorFormTests COMMAND ModelSwapperEditorFormTests ${editor_test_platform_args})
    add_test(NAME ModelSwapperEditorWindowTests COMMAND ModelSwapperEditorWindowTests ${editor_test_platform_args})
    set_tests_properties(
        ModelSwapperEditorDocumentTests
        ModelSwapperEditorFormTests
        ModelSwapperEditorWindowTests
        PROPERTIES TIMEOUT 30
    )

    if(WIN32)
        # Keep this separate from the minimal-platform widget suites: only the
        # native Windows plugin exercises Qt's PNG-backed system cursors.
        add_executable(ModelSwapperEditorNativeStartupTests
            tests/NativeStartupTests.cpp
            src/EditorWindow.cpp
            include/EditorWindow.h
            src/JsonForm.cpp
            include/JsonForm.h
            src/ConfigDocument.cpp
            include/ConfigDocument.h
        )
        source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES tests/NativeStartupTests.cpp)
        set_target_properties(ModelSwapperEditorNativeStartupTests PROPERTIES AUTOMOC ON)
        target_compile_features(ModelSwapperEditorNativeStartupTests PRIVATE cxx_std_20)
        target_include_directories(ModelSwapperEditorNativeStartupTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
        target_link_libraries(ModelSwapperEditorNativeStartupTests PRIVATE Qt6::Widgets)
        if(MSVC)
            target_compile_options(ModelSwapperEditorNativeStartupTests PRIVATE /utf-8 /W4)
        endif()
        if(TARGET Qt6::QWindowsIntegrationPlugin)
            qt_import_plugins(ModelSwapperEditorNativeStartupTests INCLUDE_BY_TYPE platforms Qt6::QWindowsIntegrationPlugin)
        endif()
        add_test(NAME ModelSwapperEditorNativeStartupTests COMMAND ModelSwapperEditorNativeStartupTests -platform windows)
        set_tests_properties(ModelSwapperEditorNativeStartupTests PROPERTIES TIMEOUT 15 LABELS "native;windows")
    endif()
endif()
