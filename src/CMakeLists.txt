set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(premiumize-explorer
    main.cpp

    api/ApiTypes.cpp
    api/PremiumizeApi.cpp

    config/AppConfig.cpp

    model/PremiumizeModel.cpp
    model/UpEntryProxyModel.cpp

    resources/resources.qrc

    ui/ApiKeyDialog.cpp
    ui/BatchDownloadWizard.cpp
    ui/CloudTransfersWindow.cpp
    ui/CreateFolderDialog.cpp
    ui/FilePane.cpp
    ui/MainWindow.cpp
    ui/LogWindow.cpp
    ui/TransferProgressWindow.cpp

    transfer/DownloadJob.cpp
    transfer/TransferManager.cpp
    transfer/UploadJob.cpp
)

target_include_directories(premiumize-explorer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(premiumize-explorer
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
        qlementine
        pmex_warnings
)

set_target_properties(premiumize-explorer PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE    TRUE
)

# On Windows, copy Qt DLLs and plugins next to the exe after each build so it
# runs straight from the build directory (and from the VS debugger).
option(PMEX_WINDEPLOYQT "Run windeployqt after building on Windows" ON)
if(WIN32 AND PMEX_WINDEPLOYQT AND TARGET Qt6::windeployqt)
    add_custom_command(TARGET premiumize-explorer POST_BUILD
        COMMAND Qt6::windeployqt
            $<IF:$<CONFIG:Debug>,--debug,--release>
            --no-translations
            --verbose 0
            $<TARGET_FILE:premiumize-explorer>
        COMMENT "Deploying Qt runtime with windeployqt"
        VERBATIM
    )
    set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT premiumize-explorer)
endif()
