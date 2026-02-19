cmake_minimum_required(VERSION 3.21)
project(Ordnung VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Concurrent Svg)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp

    src/app/Application.h
    src/app/Application.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp

    src/core/Track.h
    src/core/Playlist.h
    src/core/ConversionJob.h

    src/models/TrackModel.h
    src/models/TrackModel.cpp
    src/models/PlaylistModel.h
    src/models/PlaylistModel.cpp
    src/models/DownloadsModel.h
    src/models/DownloadsModel.cpp

    src/views/LibraryView.h
    src/views/LibraryView.cpp
    src/views/PlaylistPanel.h
    src/views/PlaylistPanel.cpp
    src/views/TrackTableView.h
    src/views/TrackTableView.cpp
    src/views/TrackDetailPanel.h
    src/views/TrackDetailPanel.cpp
    src/views/DownloadsView.h
    src/views/DownloadsView.cpp

    src/delegates/FormatDelegate.h
    src/delegates/FormatDelegate.cpp
    src/delegates/StatusDelegate.h
    src/delegates/StatusDelegate.cpp

    src/commands/UpdateFormatCommand.h
    src/commands/UpdateFormatCommand.cpp
    src/commands/BulkFormatCommand.h
    src/commands/BulkFormatCommand.cpp
    src/commands/DeleteTracksCommand.h
    src/commands/DeleteTracksCommand.cpp

    src/services/Database.h
    src/services/Database.cpp
    src/services/Converter.h
    src/services/Converter.cpp
    src/services/FolderWatcher.h
    src/services/FolderWatcher.cpp
    src/services/PlaylistImporter.h
    src/services/PlaylistImporter.cpp

    resources/resources.qrc
)

qt_add_executable(Ordnung ${SOURCES})

target_include_directories(Ordnung PRIVATE src)

target_link_libraries(Ordnung PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Svg
)

if(WIN32)
    set_target_properties(Ordnung PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(APPLE)
    set_target_properties(Ordnung PROPERTIES MACOSX_BUNDLE TRUE)
endif()

install(TARGETS Ordnung
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
