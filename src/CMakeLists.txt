set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(premiumize-explorer
    main.cpp

    api/ApiTypes.cpp
    api/PremiumizeApi.cpp

    config/AppConfig.cpp

    model/PremiumizeModel.cpp

    ui/ApiKeyDialog.cpp
    ui/CreateFolderDialog.cpp
    ui/FilePane.cpp
    ui/MainWindow.cpp
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
