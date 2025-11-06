set(EXE_NAME "NeapuVideoPlayer")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets ShaderTools)

qt_standard_project_setup()

qt_add_executable(
        ${EXE_NAME}
        main.cpp
)

qt_add_shaders(${EXE_NAME} "shaders"
        FILES
        shaders/video.vert
        shaders/none.frag
        shaders/yuv420p.frag
        shaders/nv12.frag
        shaders/p010.frag
)

add_subdirectory(media)
add_subdirectory(view)

target_link_libraries(${EXE_NAME} PRIVATE Qt6::Core Qt6::Widgets)
target_link_libraries(${EXE_NAME} PRIVATE logger)
target_link_libraries(${EXE_NAME} PRIVATE view)

# Windows平台下自动部署Qt依赖库
if(WIN32)
    # 查找windeployqt.exe
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${QT_INSTALL_PATH} ENV QT6_DIR PATH_SUFFIXES bin)

    if(WINDEPLOYQT_EXECUTABLE)
        message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")

        # 添加部署目标
        add_custom_command(TARGET ${EXE_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "Deploying Qt libraries..."
                COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --$<LOWER_CASE:$<CONFIG>>
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                $<TARGET_FILE:${EXE_NAME}>
                COMMENT "Deploying Qt libraries to output directory"
                VERBATIM
        )
    else()
        message(WARNING "windeployqt not found. Qt libraries will not be automatically deployed.")
    endif()
endif()