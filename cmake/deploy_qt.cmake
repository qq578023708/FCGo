# deploy_qt.cmake - Auto-deploy Qt and runtime DLLs
# Usage: cmake -DQT_DEPLOY_TARGET=<exe_path> -P deploy_qt.cmake

message(STATUS "Deploying Qt DLLs for: ${QT_DEPLOY_TARGET}")

get_filename_component(TARGET_DIR "${QT_DEPLOY_TARGET}" DIRECTORY)

# Try using windeployqt first (available in Qt6)
find_program(WINDEPLOYQT windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin" "${CMAKE_PREFIX_PATH}/../mingw_64/bin")
if(WINDEPLOYQT)
    message(STATUS "Using windeployqt: ${WINDEPLOYQT}")
    execute_process(
        COMMAND "${WINDEPLOYQT}" --no-translations --no-opengl-sw "${QT_DEPLOY_TARGET}"
        OUTPUT_VARIABLE WINDEPLOYQT_OUTPUT
        ERROR_VARIABLE WINDEPLOYQT_ERROR
        RESULT_VARIABLE WINDEPLOYQT_RESULT
    )
    if(NOT WINDEPLOYQT_RESULT EQUAL 0)
        message(WARNING "windeployqt failed: ${WINDEPLOYQT_ERROR}")
    else()
        message(STATUS "windeployqt completed successfully")
    endif()
else()
    message(STATUS "windeployqt not found, copying Qt DLLs manually")

    # Find Qt6 DLLs
    set(QT_LIBS
        Qt6Core
        Qt6Gui
        Qt6Widgets
        Qt6OpenGL
        Qt6OpenGLWidgets
        Qt6Network
    )

    # Determine Qt bin directory
    if(CMAKE_PREFIX_PATH)
        set(QT_BIN "${CMAKE_PREFIX_PATH}/bin")
    else()
        # Try to find from target
        get_filename_component(QT_BIN "${WINDEPLOYQT}" DIRECTORY)
    endif()

    foreach(QT_LIB ${QT_LIBS})
        # Find the DLL
        find_file(QT_DLL
            NAMES "${QT_LIB}.dll"
            HINTS "${QT_BIN}"
            NO_DEFAULT_PATH
        )
        if(QT_DLL)
            file(COPY "${QT_DLL}" DESTINATION "${TARGET_DIR}")
            message(STATUS "  Copied: ${QT_DLL}")
        else()
            message(WARNING "  Not found: ${QT_LIB}.dll")
        endif()
        unset(QT_DLL CACHE)
    endforeach()

    # Copy Qt platform plugin
    set(QT_PLATFORMS_DIR "")
    if(EXISTS "${CMAKE_PREFIX_PATH}/plugins/platforms")
        set(QT_PLATFORMS_DIR "${CMAKE_PREFIX_PATH}/plugins/platforms")
    elseif(EXISTS "${CMAKE_PREFIX_PATH}/../mingw_64/plugins/platforms")
        set(QT_PLATFORMS_DIR "${CMAKE_PREFIX_PATH}/../mingw_64/plugins/platforms")
    endif()

    if(QT_PLATFORMS_DIR)
        file(MAKE_DIRECTORY "${TARGET_DIR}/platforms")
        file(COPY "${QT_PLATFORMS_DIR}/qwindows.dll" DESTINATION "${TARGET_DIR}/platforms")
        message(STATUS "  Copied: platforms/qwindows.dll")
    endif()

    # Copy Qt styles plugin
    set(QT_STYLES_DIR "")
    if(EXISTS "${CMAKE_PREFIX_PATH}/plugins/styles")
        set(QT_STYLES_DIR "${CMAKE_PREFIX_PATH}/plugins/styles")
    elseif(EXISTS "${CMAKE_PREFIX_PATH}/../mingw_64/plugins/styles")
        set(QT_STYLES_DIR "${CMAKE_PREFIX_PATH}/../mingw_64/plugins/styles")
    endif()

    if(QT_STYLES_DIR)
        file(MAKE_DIRECTORY "${TARGET_DIR}/styles")
        if(EXISTS "${QT_STYLES_DIR}/qmodernwindowsstyle.dll")
            file(COPY "${QT_STYLES_DIR}/qmodernwindowsstyle.dll" DESTINATION "${TARGET_DIR}/styles")
            message(STATUS "  Copied: styles/qmodernwindowsstyle.dll")
        endif()
    endif()

    # Copy imageformats plugin
    set(QT_IMAGEFORMATS_DIR "")
    if(EXISTS "${CMAKE_PREFIX_PATH}/plugins/imageformats")
        set(QT_IMAGEFORMATS_DIR "${CMAKE_PREFIX_PATH}/plugins/imageformats")
    elseif(EXISTS "${CMAKE_PREFIX_PATH}/../mingw_64/plugins/imageformats")
        set(QT_IMAGEFORMATS_DIR "${CMAKE_PREFIX_PATH}/../mingw_64/plugins/imageformats")
    endif()

    if(QT_IMAGEFORMATS_DIR)
        file(MAKE_DIRECTORY "${TARGET_DIR}/imageformats")
        file(GLOB QT_IMG_PLUGINS "${QT_IMAGEFORMATS_DIR}/*.dll")
        foreach(IMG_PLUGIN ${QT_IMG_PLUGINS})
            file(COPY "${IMG_PLUGIN}" DESTINATION "${TARGET_DIR}/imageformats")
            get_filename_component(IMG_NAME "${IMG_PLUGIN}" NAME)
            message(STATUS "  Copied: imageformats/${IMG_NAME}")
        endforeach()
    endif()
endif()

message(STATUS "Qt deployment complete")
