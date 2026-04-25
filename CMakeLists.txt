cmake_minimum_required(VERSION 3.21)
project(UVCutoutTool CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()

if(MSVC)
    add_compile_options(/utf-8 /W3 /permissive- /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

set(PROJECT_SOURCES
    src/main.cpp

    src/app/AppSettings.h
    src/app/AppSettings.cpp

    src/codec/BCDecoder.h
    src/codec/BCDecoder.cpp
    src/codec/DDSLoader.h
    src/codec/DDSLoader.cpp
    src/codec/TGAIO.h
    src/codec/TGAIO.cpp

    src/parsers/NiflyParser.h
    src/parsers/NiflyParser.cpp

    src/geometry/MeshData.h
    src/geometry/SpatialGrid.h
    src/geometry/SpatialGrid.cpp
    src/geometry/Geometry.h
    src/geometry/Geometry.cpp
    src/geometry/IslandBuilder.h
    src/geometry/IslandBuilder.cpp

    src/themes/Theme.h
    src/themes/Theme.cpp

    src/render/CanvasRenderer.h
    src/render/GpuCanvasRenderer.h
    src/render/GpuCanvasRenderer.cpp
    src/render/CpuCanvasRenderer.h
    src/render/CpuCanvasRenderer.cpp

    src/ui/UVCanvasWidget.h
    src/ui/UVCanvasWidget.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/WelcomeWidget.h
    src/ui/WelcomeWidget.cpp
    src/ui/WarmButton.h
    src/ui/WarmButton.cpp
    src/ui/ShapeTreePanel.h
    src/ui/ShapeTreePanel.cpp
    src/ui/ThemePickerDialog.h
    src/ui/ThemePickerDialog.cpp
    src/ui/InfoPopup.h
    src/ui/InfoPopup.cpp
    src/ui/ExportDialog.h
    src/ui/ExportDialog.cpp
)

qt_add_resources(PROJECT_RESOURCES resources/resources.qrc)

if(WIN32 AND MSVC)
    # app.rc is only compiled under MSVC. mingw's windres chokes on include
    # paths containing spaces.
    # The .rc only supplies the shell/taskbar icon. The window icon comes from
    # the Qt resource (:/icon.png), so dropping it on mingw is a
    # safe tradeoff.
    add_executable(UVCutoutTool WIN32 ${PROJECT_SOURCES} ${PROJECT_RESOURCES} resources/app.rc)
elseif(WIN32)
    add_executable(UVCutoutTool WIN32 ${PROJECT_SOURCES} ${PROJECT_RESOURCES})
else()
    add_executable(UVCutoutTool ${PROJECT_SOURCES} ${PROJECT_RESOURCES})
endif()

target_include_directories(UVCutoutTool PRIVATE src)

target_link_libraries(UVCutoutTool PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
)

if(WIN32)
    # Load NiflyDLL.dll dynamically at runtime from a path resolved next to the exe.
    target_compile_definitions(UVCutoutTool PRIVATE UV_PLATFORM_WIN=1)
endif()

set_target_properties(UVCutoutTool PROPERTIES
    OUTPUT_NAME "UV Cutout Tool"
)

install(TARGETS UVCutoutTool RUNTIME DESTINATION .)

# Copy runtime dependencies next to the built executable for a portable app folder.
if(WIN32)
    get_filename_component(QT_ROOT_DIR "${Qt6_DIR}/../../.." ABSOLUTE)
    set(QT_PLUGINS_DIR "${QT_ROOT_DIR}/plugins")
    set(NIFLY_SRC "${CMAKE_SOURCE_DIR}/third_party/nifly/NiflyDLL.dll")

    function(uvc_portable_runtime_file path)
        if(EXISTS "${path}")
            add_custom_command(TARGET UVCutoutTool POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${path}" "$<TARGET_FILE_DIR:UVCutoutTool>")
            install(FILES "${path}" DESTINATION .)
        endif()
    endfunction()

    function(uvc_portable_plugin_file path)
        if(EXISTS "${path}")
            get_filename_component(plugin_dir "${path}" DIRECTORY)
            get_filename_component(plugin_subdir "${plugin_dir}" NAME)
            add_custom_command(TARGET UVCutoutTool POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:UVCutoutTool>/${plugin_subdir}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${path}" "$<TARGET_FILE_DIR:UVCutoutTool>/${plugin_subdir}")
            install(FILES "${path}" DESTINATION "${plugin_subdir}")
        endif()
    endfunction()

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qt.conf"
        "[Paths]\n"
        "Plugins=.\n")

    set(PORTABLE_RUNTIME_DLLS
        "${QT_ROOT_DIR}/bin/Qt6Core.dll"
        "${QT_ROOT_DIR}/bin/Qt6Gui.dll"
        "${QT_ROOT_DIR}/bin/Qt6Widgets.dll"
        "${QT_ROOT_DIR}/bin/Qt6OpenGL.dll"
        "${QT_ROOT_DIR}/bin/Qt6OpenGLWidgets.dll"
        "${QT_ROOT_DIR}/bin/Qt6Network.dll"
        "${QT_ROOT_DIR}/bin/Qt6Svg.dll"
        "${QT_ROOT_DIR}/bin/d3dcompiler_47.dll"
        "${QT_ROOT_DIR}/bin/libgcc_s_seh-1.dll"
        "${QT_ROOT_DIR}/bin/libstdc++-6.dll"
        "${QT_ROOT_DIR}/bin/libwinpthread-1.dll"
        "${QT_ROOT_DIR}/bin/opengl32sw.dll"
        "${NIFLY_SRC}"
    )

    foreach(runtime_dll IN LISTS PORTABLE_RUNTIME_DLLS)
        uvc_portable_runtime_file("${runtime_dll}")
    endforeach()

    add_custom_command(TARGET UVCutoutTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/qt.conf" "$<TARGET_FILE_DIR:UVCutoutTool>/qt.conf")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qt.conf" DESTINATION .)

    set(PORTABLE_PLUGIN_FILES
        "${QT_PLUGINS_DIR}/platforms/qwindows.dll"
        "${QT_PLUGINS_DIR}/styles/qmodernwindowsstyle.dll"
        "${QT_PLUGINS_DIR}/generic/qtuiotouchplugin.dll"
        "${QT_PLUGINS_DIR}/imageformats/qgif.dll"
        "${QT_PLUGINS_DIR}/imageformats/qico.dll"
        "${QT_PLUGINS_DIR}/imageformats/qjpeg.dll"
        "${QT_PLUGINS_DIR}/imageformats/qsvg.dll"
        "${QT_PLUGINS_DIR}/iconengines/qsvgicon.dll"
        "${QT_PLUGINS_DIR}/networkinformation/qnetworklistmanager.dll"
        "${QT_PLUGINS_DIR}/tls/qcertonlybackend.dll"
        "${QT_PLUGINS_DIR}/tls/qschannelbackend.dll"
    )

    foreach(plugin_file IN LISTS PORTABLE_PLUGIN_FILES)
        uvc_portable_plugin_file("${plugin_file}")
    endforeach()
endif()
