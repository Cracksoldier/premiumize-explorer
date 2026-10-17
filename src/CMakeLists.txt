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
option(PMEX_WINDEPLOYQT "Deploy Qt and MSVC runtime next to the exe on Windows" ON)
if(WIN32 AND PMEX_WINDEPLOYQT AND TARGET Qt6::windeployqt)
    # Release configs skip windeployqt's vc_redist.x64.exe installer; the
    # runtime DLLs are bundled app-locally below instead.
    add_custom_command(TARGET premiumize-explorer POST_BUILD
        COMMAND Qt6::windeployqt
            "$<IF:$<CONFIG:Debug>,--debug,--release;--no-compiler-runtime>"
            --no-translations
            --verbose 0
            $<TARGET_FILE:premiumize-explorer>
        COMMENT "Deploying Qt runtime with windeployqt"
        VERBATIM
        COMMAND_EXPAND_LISTS
    )
    set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT premiumize-explorer)
endif()

# Bundle the MSVC runtime DLLs (vcruntime140*, msvcp140*) next to non-Debug
# builds so the output folder runs on a machine without the VC++ Redistributable.
if(MSVC AND PMEX_WINDEPLOYQT)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        # Quoted so the list's semicolons don't split the generator expression;
        # COMMAND_EXPAND_LISTS turns it back into separate arguments.
        set(_pmex_crt_copy "${CMAKE_COMMAND};-E;copy_if_different;${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS};$<TARGET_FILE_DIR:premiumize-explorer>")
        add_custom_command(TARGET premiumize-explorer POST_BUILD
            COMMAND "$<IF:$<CONFIG:Debug>,${CMAKE_COMMAND};-E;echo_append,${_pmex_crt_copy}>"
            COMMENT "Bundling MSVC runtime DLLs"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()
