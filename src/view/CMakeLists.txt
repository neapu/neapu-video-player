set(LIB_NAME "view")

find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Widgets ShaderTools Concurrent)
if(Qt6_VERSION VERSION_GREATER_EQUAL "6.9")
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
endif()

qt_add_library(${LIB_NAME} STATIC
        MainWindow.cpp
        MainWindow.h
        AudioRenderer.cpp
        AudioRenderer.h
        ControlWidget.cpp
        ControlWidget.h
        VideoRenderer.cpp
        VideoRenderer.h
        PlayerControllor.cpp
        PlayerController.h
        Pipeline.cpp
        Pipeline.h
        YuvPipeline.cpp
        YuvPipeline.h
        D3D11VAPipeline.cpp
        D3D11VAPipeline.h
        VaapiPipeline.cpp
        VaapiPipeline.h
)

target_link_libraries(${LIB_NAME} PUBLIC Qt6::Gui Qt6::Widgets Qt6::GuiPrivate Qt6::Concurrent)
target_link_libraries(${LIB_NAME} PUBLIC logger media miniaudio)
if (WIN32)
    target_link_libraries(${LIB_NAME} PRIVATE d3d11 dxgi)
endif ()

if (LINUX)
    message("Configuring for Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBVA REQUIRED libva libva-drm)
    pkg_check_modules(LIBDRM REQUIRED libdrm)
    pkg_check_modules(EGL REQUIRED egl)
    target_include_directories(${LIB_NAME} PRIVATE ${LIBVA_INCLUDE_DIRS} ${LIBDRM_INCLUDE_DIRS} ${EGL_INCLUDE_DIRS})
    target_link_libraries(${LIB_NAME} PRIVATE ${LIBVA_LIBRARIES} ${LIBDRM_LIBRARIES} ${EGL_LIBRARIES})
endif()

if (APPLE)
    message("Configuring for macOS")
    target_sources(${LIB_NAME} PRIVATE
        MetalVTPipeline.mm
        MetalVTPipeline.h
    )
    target_link_libraries(${LIB_NAME} PRIVATE
        "-framework CoreVideo"
        "-framework Metal"
        "-framework IOSurface"
    )
endif()