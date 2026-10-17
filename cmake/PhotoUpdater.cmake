# Set this to the app owner's repository, never the original qView repository.
set(PHOTO_UPDATE_REPOSITORY "RilakKuma2/NestView" CACHE STRING "GitHub owner/repository for NestView releases")
set(PHOTO_UPDATE_FEED "")
set(PHOTO_MANUAL_DOWNLOAD "")
if(PHOTO_UPDATE_REPOSITORY)
    if(NOT PHOTO_UPDATE_REPOSITORY MATCHES "^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$"
       OR PHOTO_UPDATE_REPOSITORY MATCHES "^[Jj]urplel/")
        message(FATAL_ERROR "Use your own GitHub owner/repository for NestView updates.")
    endif()
    set(PHOTO_UPDATE_FEED "https://github.com/${PHOTO_UPDATE_REPOSITORY}/releases/latest/download/appcast.xml")
    set(PHOTO_MANUAL_DOWNLOAD "https://github.com/${PHOTO_UPDATE_REPOSITORY}/releases/latest/download/NestView-macOS-AppleSilicon.dmg")
endif()

if(APPLE AND NOT NEST_PORTABLE_UPDATER)
    include(FetchContent)
    FetchContent_Declare(photo_sparkle
        URL "https://github.com/sparkle-project/Sparkle/releases/download/2.10.0/Sparkle-for-Swift-Package-Manager.zip"
        URL_HASH SHA256=17e28312b8e18ab7cdbbe09a6fb28cc55a5479ec6c371dbc07cdecd2a14fd959
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(photo_sparkle)
    set(PHOTO_SPARKLE_DIR "${photo_sparkle_SOURCE_DIR}")
    set(PHOTO_SPARKLE_FRAMEWORK "${PHOTO_SPARKLE_DIR}/Sparkle.xcframework/macos-arm64_x86_64/Sparkle.framework")
    add_library(photo_updater STATIC "${CMAKE_CURRENT_LIST_DIR}/../src/photoupdater.mm")
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../src/photoupdater.mm" PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(photo_updater PUBLIC "${PHOTO_SPARKLE_FRAMEWORK}" "-framework Cocoa")
else()
    add_library(photo_updater STATIC "${CMAKE_CURRENT_LIST_DIR}/../src/photoupdater.cpp")
    target_link_libraries(photo_updater PUBLIC Qt6::Widgets Qt6::Network)
    target_compile_definitions(photo_updater PRIVATE
        NEST_UPDATE_REPOSITORY="${PHOTO_UPDATE_REPOSITORY}" NEST_VERSION="${PROJECT_VERSION}")
endif()
