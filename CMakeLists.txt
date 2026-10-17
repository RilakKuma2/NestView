cmake_minimum_required(VERSION 3.21)

project(NestView VERSION 0.1 LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()
set(PHOTO_BUILD_NUMBER 8)
if(APPLE)
    set(NEST_PORTABLE_DEFAULT OFF)
else()
    set(NEST_PORTABLE_DEFAULT ON)
endif()
option(NEST_PORTABLE_MEDIA "Use the Qt/Exiv2/LibRaw media backend" ${NEST_PORTABLE_DEFAULT})
option(NEST_PORTABLE_UPDATER "Use the portable release checker" ${NEST_PORTABLE_DEFAULT})

# Optional test building
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
endif()

# Generate a compile_commands.json file for clang-tidy and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the application type for platform-specific executables
if(WIN32)
    set(EXECUTABLE_TYPE WIN32)
elseif(APPLE)
    set(EXECUTABLE_TYPE MACOSX_BUNDLE)
endif()

# Enable C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt's automatic tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find required Qt components
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Widgets Svg PrintSupport Concurrent)
set(QT_VERSION_MAJOR 6)
find_package(Qt6 QUIET COMPONENTS LinguistTools)
include(cmake/PhotoUpdater.cmake)

find_package(LibArchive REQUIRED)
target_link_libraries(photo_updater PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
if(NEST_PORTABLE_MEDIA)
    find_package(exiv2 CONFIG REQUIRED)
    if(TARGET Exiv2::exiv2lib)
        set(NEST_EXIV2_TARGET Exiv2::exiv2lib)
    elseif(TARGET Exiv2::exiv2)
        set(NEST_EXIV2_TARGET Exiv2::exiv2)
    else()
        set(NEST_EXIV2_TARGET exiv2lib)
    endif()
    find_path(NEST_LIBRAW_INCLUDE_DIR libraw/libraw.h REQUIRED)
    find_library(NEST_LIBRAW_LIBRARY NAMES raw_r libraw_r raw libraw REQUIRED)
    add_library(nest_raw UNKNOWN IMPORTED)
    set_target_properties(nest_raw PROPERTIES IMPORTED_LOCATION "${NEST_LIBRAW_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${NEST_LIBRAW_INCLUDE_DIR}")
    set(NEST_MEDIA_SOURCE src/nestmedia.cpp)
    set(NEST_MEDIA_LIBRARIES ${NEST_EXIV2_TARGET} nest_raw)
else()
    set(NEST_MEDIA_SOURCE src/nestmedia.mm)
    set(NEST_MEDIA_LIBRARIES "-framework ImageIO" "-framework CoreGraphics")
endif()
if(UNIX AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    list(APPEND NEST_MEDIA_LIBRARIES Qt6::DBus)
endif()

# Source files
set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/nestshell.cpp
    src/nestchrome.cpp
    src/nestcontext.cpp
    src/nestsettings.cpp
    src/nestreadingview.cpp
    ${NEST_MEDIA_SOURCE}
    src/nestarchive.cpp
    src/nestconvert.cpp
    src/nesteffects.cpp
    src/nestplatform.cpp
    src/openwith.cpp
    src/qvgraphicsview.cpp
    src/qvoptionsdialog.cpp
    src/qvapplication.cpp
    src/qvaboutdialog.cpp
    src/qvrenamedialog.cpp
    src/qvwelcomedialog.cpp
    src/qvinfodialog.cpp
    src/qvimagecore.cpp
    src/qvshortcutdialog.cpp
    src/actionmanager.cpp
    src/settingsmanager.cpp
    src/shortcutmanager.cpp
)

# Header files
set(PROJECT_HEADERS
    src/mainwindow.h
    src/openwith.h
    src/qvgraphicsview.h
    src/qvoptionsdialog.h
    src/qvapplication.h
    src/qvaboutdialog.h
    src/qvrenamedialog.h
    src/qvwelcomedialog.h
    src/qvinfodialog.h
    src/qvimagecore.h
    src/qvshortcutdialog.h
    src/actionmanager.h
    src/settingsmanager.h
    src/shortcutmanager.h
)

# UI form files
set(PROJECT_FORMS
    src/mainwindow.ui
    src/qvopenwithdialog.ui
    src/qvoptionsdialog.ui
    src/qvaboutdialog.ui
    src/qvwelcomedialog.ui
    src/qvinfodialog.ui
    src/qvshortcutdialog.ui
)

# To disables both manual and automatic checking for updates
# The upstream checker targets qView, not this fork. Never enable it here.
set(QV_DISABLE_ONLINE_VERSION_CHECK ON)
if(NOT QV_DISABLE_ONLINE_VERSION_CHECK)
    list(APPEND PROJECT_SOURCES src/updatechecker.cpp)
    list(APPEND PROJECT_HEADERS src/updatechecker.h)
endif()

# Platform-specific sources
# Add source files required for each operating system.
if(WIN32)
    # For Windows, add Win32-specific functions and headers.
    option(NO_WIN32 "Build without Win32 API" OFF)
    if(NOT NO_WIN32)
        list(APPEND PROJECT_SOURCES src/qvwin32functions.cpp)
        list(APPEND PROJECT_HEADERS src/qvwin32functions.h)
    endif()
elseif(APPLE)
    # For macOS, add Cocoa-specific functions and headers.
    option(NO_COCOA "Build without Cocoa framework" OFF)
    if(NOT NO_COCOA)
        list(APPEND PROJECT_SOURCES src/qvcocoafunctions.mm)
        list(APPEND PROJECT_HEADERS src/qvcocoafunctions.h)
    endif()
elseif(UNIX AND NOT APPLE)
    # For Linux, add X11-specific functions and headers.
    option(NO_X11 "Build without X11" OFF)
    if(NOT NO_X11)
        list(APPEND PROJECT_SOURCES src/qvlinuxx11functions.cpp)
        list(APPEND PROJECT_HEADERS src/qvlinuxx11functions.h)
    endif()
endif()

if(APPLE)
    set_source_files_properties(src/nestmedia.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

# Add the executable and link Qt libraries
set(PROJECT_RESOURCES resources/resources.qrc)
if(WIN32)
    set(ICON_PATH_RAW "${CMAKE_CURRENT_SOURCE_DIR}/dist/win/qView.ico")
    string(REPLACE "\\" "\\\\" ICON_PATH "${ICON_PATH_RAW}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/win/qView.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/qView.rc"
    )
    list(APPEND PROJECT_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/qView.rc")
endif()
add_executable(${PROJECT_NAME} ${EXECUTABLE_TYPE} ${PROJECT_SOURCES} ${PROJECT_HEADERS} ${PROJECT_FORMS} ${PROJECT_RESOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE photo_updater)
if(APPLE AND NOT NEST_PORTABLE_UPDATER)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND /usr/bin/ditto "${PHOTO_SPARKLE_FRAMEWORK}"
            "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks/Sparkle.framework"
        VERBATIM)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::Widgets Qt6::Svg Qt6::PrintSupport Qt6::Concurrent LibArchive::LibArchive ${NEST_MEDIA_LIBRARIES})
if(MSVC)
    add_compile_options(/utf-8)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# Set the executable name to lowercase on Linux
if(UNIX AND NOT APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "nestview" INSTALL_RPATH "$ORIGIN/../lib")
endif()

# Add compile definitions based on options
if(QV_DISABLE_ONLINE_VERSION_CHECK)
    target_compile_definitions(${PROJECT_NAME} PRIVATE QV_DISABLE_ONLINE_VERSION_CHECK)
endif()

# Platform-specific configurations
# Apply settings, link libraries, and define properties for each platform.
if(WIN32)
    # Windows specific: link to Win32 libraries, set icon, and define as a Windows executable.
    if(NOT NO_WIN32)
        target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LOADED)
        set(PLATFORM_SPECIFIC_LIBS shell32 user32 ole32 shlwapi gdi32)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${PLATFORM_SPECIFIC_LIBS})
    endif()
elseif(APPLE)
    # macOS specific: link to Cocoa framework and configure the app bundle.
    if(NOT NO_COCOA)
        target_compile_definitions(${PROJECT_NAME} PRIVATE COCOA_LOADED)
        set(PLATFORM_SPECIFIC_LIBS "-framework Cocoa")
        target_link_libraries(${PROJECT_NAME} PRIVATE ${PLATFORM_SPECIFIC_LIBS})
    endif()

    # Set the icon based on the Qt version, as per the Qt documentation.
    if(QT_VERSION_MAJOR LESS 6)
        set(MACOSX_BUNDLE_ICON_FILE "qView_legacy.icns")
    else()
        set(MACOSX_BUNDLE_ICON_FILE "NestView.icns")
    endif()

    set(app_icon_macos "${CMAKE_CURRENT_SOURCE_DIR}/dist/mac/${MACOSX_BUNDLE_ICON_FILE}")
    set_source_files_properties(${app_icon_macos} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(${PROJECT_NAME} PRIVATE ${app_icon_macos})

    # The Info.plist.in file contains placeholders that will be replaced by
    # CMake. We set MACOSX_ICON_FILENAME here so that configure_file can
    # substitute it in the plist.
    set(MACOSX_ICON_FILENAME ${MACOSX_BUNDLE_ICON_FILE})
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/dist/mac/Info.plist.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        @ONLY
    )

    # Set bundle properties. MACOSX_BUNDLE_INFO_PLIST tells CMake to use our
    # generated Info.plist instead of the default one.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    )
elseif(UNIX AND NOT APPLE)
    if(NOT NO_X11)
        target_compile_definitions(${PROJECT_NAME} PRIVATE X11_LOADED)
        find_package(X11 REQUIRED)
        set(PLATFORM_SPECIFIC_LIBS ${X11_LIBRARIES})
        if(NOT Qt6_FOUND)
            list(APPEND PLATFORM_SPECIFIC_LIBS Qt5::X11Extras)
        endif()
        target_link_libraries(${PROJECT_NAME} PRIVATE ${PLATFORM_SPECIFIC_LIBS})
    endif()
endif()

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "VERSION=${PROJECT_VERSION}"
    QT_DEPRECATED_WARNINGS
    QT_NO_FOREACH
)

# Check for nightly builds, which are identified by the NIGHTLY preprocessor definition.
if(DEFINED NIGHTLY)
    message(STATUS "This is nightly ${NIGHTLY}")
    target_compile_definitions(${PROJECT_NAME} PRIVATE "NIGHTLY=\"${NIGHTLY}\"")
endif()

# Compile translations explicitly: Qt 6.11 deferred helper mishandles spaces in paths.
if(TARGET Qt6::lrelease)
    file(GLOB TRANSLATIONS "i18n/qview_*.ts")
    set(QM_FILES "")
    set(QM_RESOURCE "<RCC><qresource prefix=\"/i18n\">\n")
    foreach(TS_FILE IN LISTS TRANSLATIONS)
        get_filename_component(TS_NAME "${TS_FILE}" NAME_WE)
        set(QM_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TS_NAME}.qm")
        add_custom_command(OUTPUT "${QM_FILE}"
            COMMAND Qt6::lrelease "${TS_FILE}" -qm "${QM_FILE}"
            DEPENDS "${TS_FILE}" VERBATIM)
        list(APPEND QM_FILES "${QM_FILE}")
        string(APPEND QM_RESOURCE "<file alias=\"${TS_NAME}.qm\">${QM_FILE}</file>\n")
    endforeach()
    string(APPEND QM_RESOURCE "</qresource></RCC>")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc" "${QM_RESOURCE}")
    add_custom_target(nest_translations DEPENDS ${QM_FILES})
    add_dependencies(${PROJECT_NAME} nest_translations)
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc")
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES dist/linux/org.nestview.viewer.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/icons/nestview.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME org.nestview.viewer.svg)
    install(FILES dist/linux/org.nestview.viewer.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()
install(FILES LICENSE docs/THIRD_PARTY.md DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/nestview)

# Include tests if requested
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR DEB)
    set(CPACK_PACKAGE_NAME nestview)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_CONTACT "https://github.com/RilakKuma2/NestView/issues")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Photo and comic archive viewer")
    set(CPACK_PACKAGE_FILE_NAME "NestView-Linux-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-image-formats-plugins, qt6-wayland, xdg-desktop-portal")
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
    include(CPack)
endif()
