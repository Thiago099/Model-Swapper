cmake_minimum_required(VERSION 3.21)

# This directory can also be configured directly to build just the editor with
# an existing Qt 6 installation, without the SKSE/CommonLib dependencies.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ModelSwapperEditor VERSION 0.3.0 LANGUAGES CXX)
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

qt_add_executable(ModelSwapperEditor WIN32
    main.cpp
    EditorWindow.cpp
    EditorWindow.h
    JsonForm.cpp
    JsonForm.h
    ConfigDocument.cpp
    ConfigDocument.h
)

set_target_properties(ModelSwapperEditor PROPERTIES
    AUTOMOC ON
    CXX_EXTENSIONS OFF
    # Keep the editor in the build tree, separate from the plugin's deployment.
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_features(ModelSwapperEditor PRIVATE cxx_std_20)
target_link_libraries(ModelSwapperEditor PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(ModelSwapperEditor PRIVATE /utf-8 /W4)
endif()

option(MS_EDITOR_BUILD_TESTS "Build the editor's document and widget tests" OFF)
if(MS_EDITOR_BUILD_TESTS)
    enable_testing()
    add_executable(ModelSwapperEditorDocumentTests
        ConfigDocument.cpp
        ConfigDocument.h
        ConfigDocumentTests.cpp
    )
    target_compile_features(ModelSwapperEditorDocumentTests PRIVATE cxx_std_20)
    target_link_libraries(ModelSwapperEditorDocumentTests PRIVATE Qt6::Core)
    if(MSVC)
        target_compile_options(ModelSwapperEditorDocumentTests PRIVATE /utf-8 /W4)
    endif()
    add_test(NAME ModelSwapperEditorDocumentTests COMMAND ModelSwapperEditorDocumentTests)

    add_executable(ModelSwapperEditorFormTests
        JsonFormTests.cpp
        JsonForm.cpp
        JsonForm.h
        ConfigDocument.cpp
        ConfigDocument.h
    )
    set_target_properties(ModelSwapperEditorFormTests PROPERTIES AUTOMOC ON)
    target_compile_features(ModelSwapperEditorFormTests PRIVATE cxx_std_20)
    target_link_libraries(ModelSwapperEditorFormTests PRIVATE Qt6::Widgets)
    if(MSVC)
        target_compile_options(ModelSwapperEditorFormTests PRIVATE /utf-8 /W4)
    endif()

    add_executable(ModelSwapperEditorWindowTests
        EditorWindowTests.cpp
        EditorWindow.cpp
        EditorWindow.h
        JsonForm.cpp
        JsonForm.h
        ConfigDocument.cpp
        ConfigDocument.h
    )
    set_target_properties(ModelSwapperEditorWindowTests PROPERTIES AUTOMOC ON)
    target_compile_features(ModelSwapperEditorWindowTests PRIVATE cxx_std_20)
    target_link_libraries(ModelSwapperEditorWindowTests PRIVATE Qt6::Widgets)
    if(MSVC)
        target_compile_options(ModelSwapperEditorWindowTests PRIVATE /utf-8 /W4)
    endif()

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
endif()
