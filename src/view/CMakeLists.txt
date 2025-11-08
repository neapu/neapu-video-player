set(LIB_NAME "view")

find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Widgets ShaderTools)

qt_add_library(${LIB_NAME} STATIC
        MainWindow.cpp
        MainWindow.h
        VideoRenderer.cpp
        VideoRenderer.h
        AudioRenderer.cpp
        AudioRenderer.h
        ControlWidget.cpp
        ControlWidget.h
)

target_link_libraries(${LIB_NAME} PUBLIC Qt6::Gui Qt6::Widgets Qt6::GuiPrivate)
target_link_libraries(${LIB_NAME} PUBLIC logger media miniaudio)
if (WIN32)
    target_link_libraries(${EXE_NAME} PRIVATE d3d11 dxgi)
endif ()