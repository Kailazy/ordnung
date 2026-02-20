cmake_minimum_required(VERSION 3.21)
project(Ordnung VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Concurrent Svg Xml)
find_package(Qt6 OPTIONAL_COMPONENTS Multimedia)

# ── Bundled third-party libraries ─────────────────────────────────────────
# Platform triplet used to find pre-built libs in third_party/
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TP_TRIPLET "linux-x64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(TP_TRIPLET "windows-x64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    execute_process(COMMAND uname -m
        OUTPUT_VARIABLE _HOST_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_HOST_ARCH STREQUAL "arm64")
        set(TP_TRIPLET "macos-arm64")
    else()
        set(TP_TRIPLET "macos-x64")
    endif()
else()
    set(TP_TRIPLET "unknown")
endif()

set(TP_ROOT    "${CMAKE_SOURCE_DIR}/../third_party")
set(ESSENTIA_TP "${TP_ROOT}/essentia/${TP_TRIPLET}")
set(ONNXRT_TP   "${TP_ROOT}/onnxruntime/${TP_TRIPLET}")

qt_standard_project_setup()

set(SOURCES
    src/main.cpp

    src/style/Theme.h
    src/style/Theme.cpp

    src/app/Application.h
    src/app/Application.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp

    src/core/Track.h
    src/core/Playlist.h
    src/core/ConversionJob.h
    src/core/CuePoint.h
    src/core/ServiceRegistry.h

    src/models/TrackModel.h
    src/models/TrackModel.cpp
    src/models/PlaylistModel.h
    src/models/PlaylistModel.cpp
    src/models/DownloadsModel.h
    src/models/DownloadsModel.cpp

    src/views/LibraryView.h
    src/views/LibraryView.cpp
    src/views/CollectionTreePanel.h
    src/views/CollectionTreePanel.cpp
    src/views/PlaylistPanel.h
    src/views/PlaylistPanel.cpp
    src/views/FolderPanel.h
    src/views/FolderPanel.cpp
    src/views/table/LibraryTableColumn.h
    src/views/table/LibraryTableColumn.cpp
    src/views/table/LibraryTableRow.h
    src/views/table/LibraryTableRowPainter.h
    src/views/table/LibraryTableRowPainter.cpp
    src/views/TrackTableView.h
    src/views/TrackTableView.cpp
    src/views/CuePointEditor.h
    src/views/CuePointEditor.cpp
    src/views/TrackDetailPanel.h
    src/views/TrackDetailPanel.cpp
    src/views/PlayerBar.h
    src/views/PlayerBar.cpp
    src/views/ExportWizard.h
    src/views/ExportWizard.cpp
    src/views/AnalysisProgressDialog.h
    src/views/AnalysisProgressDialog.cpp
    src/views/BatchEditDialog.h
    src/views/BatchEditDialog.cpp
    src/views/MissingFilesDialog.h
    src/views/MissingFilesDialog.cpp
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
    src/services/PlaylistImporter.h
    src/services/PlaylistImporter.cpp
    src/services/LibraryScanner.h
    src/services/LibraryScanner.cpp
    src/services/Converter.h
    src/services/Converter.cpp
    src/services/FolderWatcher.h
    src/services/FolderWatcher.cpp
    src/services/ExportService.h
    src/services/ExportService.cpp
    src/services/PdbWriter.h
    src/services/PdbWriter.cpp
    src/services/AudioAnalyzer.h
    src/services/AudioAnalyzer.cpp
    src/services/EssentiaAnalyzer.h
    src/services/EssentiaAnalyzer.cpp

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
    Qt6::Xml
)

if(Qt6Multimedia_FOUND)
    target_link_libraries(Ordnung PRIVATE Qt6::Multimedia)
    target_compile_definitions(Ordnung PRIVATE HAVE_MULTIMEDIA)
endif()

# ── Essentia: check third_party/ then system pkg-config ───────────────────
set(ORDNUNG_HAVE_ESSENTIA FALSE)

if(EXISTS "${ESSENTIA_TP}/include/essentia/algorithmfactory.h")
    message(STATUS "Essentia: using bundled libs (${TP_TRIPLET})")
    set(ORDNUNG_HAVE_ESSENTIA TRUE)
    target_compile_definitions(Ordnung PRIVATE HAVE_ESSENTIA)
    target_include_directories(Ordnung PRIVATE "${ESSENTIA_TP}/include")
    if(WIN32)
        # MinGW uses a .dll.a import lib; MSVC uses a .lib. Prefer .dll.a.
        if(MINGW AND EXISTS "${ESSENTIA_TP}/lib/libessentia.dll.a")
            target_link_libraries(Ordnung PRIVATE "${ESSENTIA_TP}/lib/libessentia.dll.a")
        else()
            target_link_libraries(Ordnung PRIVATE "${ESSENTIA_TP}/lib/essentia.lib")
        endif()
        file(GLOB _ESS_DLLS "${ESSENTIA_TP}/lib/*.dll")
        foreach(_DLL ${_ESS_DLLS})
            add_custom_command(TARGET Ordnung POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_DLL}" "$<TARGET_FILE_DIR:Ordnung>")
        endforeach()
    elseif(APPLE)
        # Link against the main dylib; dylibbundler has already made all transitive
        # deps self-contained (copied alongside with @loader_path/ install names).
        file(GLOB _ESS_DYLIBS "${ESSENTIA_TP}/lib/*.dylib")
        # Find the unversioned libessentia.dylib for linking
        set(_ESS_LINK "")
        foreach(_L ${_ESS_DYLIBS})
            if(_L MATCHES "libessentia\\.dylib$")
                set(_ESS_LINK "${_L}")
            endif()
        endforeach()
        if(NOT _ESS_LINK)
            list(GET _ESS_DYLIBS 0 _ESS_LINK)
        endif()
        target_link_libraries(Ordnung PRIVATE "${_ESS_LINK}")
        # Embed rpath in the build-tree binary so macdeployqt and local runs work.
        set_target_properties(Ordnung PROPERTIES
            INSTALL_RPATH "@executable_path"
            BUILD_WITH_INSTALL_RPATH TRUE)
        # Copy ALL dylibs (essentia + bundled deps) beside the binary at build time.
        foreach(_L ${_ESS_DYLIBS})
            add_custom_command(TARGET Ordnung POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_L}" "$<TARGET_FILE_DIR:Ordnung>")
        endforeach()
    else()
        # Linux: link against .so and copy all .so* files beside the binary
        file(GLOB _ESS_LIBS "${ESSENTIA_TP}/lib/libessentia.so*")
        # Pick the unversioned .so first for linking
        set(_ESS_LINK "")
        foreach(_L ${_ESS_LIBS})
            if(_L MATCHES "libessentia\\.so$")
                set(_ESS_LINK "${_L}")
            endif()
        endforeach()
        if(NOT _ESS_LINK)
            list(GET _ESS_LIBS 0 _ESS_LINK)
        endif()
        target_link_libraries(Ordnung PRIVATE "${_ESS_LINK}")
        set_target_properties(Ordnung PROPERTIES INSTALL_RPATH "$ORIGIN")
        foreach(_L ${_ESS_LIBS})
            add_custom_command(TARGET Ordnung POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_L}" "$<TARGET_FILE_DIR:Ordnung>")
        endforeach()
    endif()
else()
    # System fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ESSENTIA_SYS QUIET essentia)
    endif()
    if(ESSENTIA_SYS_FOUND)
        message(STATUS "Essentia: found via system pkg-config")
        set(ORDNUNG_HAVE_ESSENTIA TRUE)
        target_compile_definitions(Ordnung PRIVATE HAVE_ESSENTIA)
        target_include_directories(Ordnung PRIVATE ${ESSENTIA_SYS_INCLUDE_DIRS})
        target_link_libraries(Ordnung PRIVATE ${ESSENTIA_SYS_LIBRARIES})
    else()
        message(STATUS "Essentia: not found -- ffprobe/aubio fallback active")
    endif()
endif()

# ── ONNX Runtime (only useful when Essentia is also present) ───────────────
if(ORDNUNG_HAVE_ESSENTIA)
    if(EXISTS "${ONNXRT_TP}/include/onnxruntime_cxx_api.h")
        message(STATUS "ONNX Runtime: using bundled libs (${TP_TRIPLET})")
        target_compile_definitions(Ordnung PRIVATE HAVE_ONNX)
        target_include_directories(Ordnung PRIVATE "${ONNXRT_TP}/include")
        if(WIN32)
            # MinGW uses the .dll.a import lib generated by fetch-onnxruntime.sh.
            if(MINGW AND EXISTS "${ONNXRT_TP}/lib/libonnxruntime.dll.a")
                target_link_libraries(Ordnung PRIVATE "${ONNXRT_TP}/lib/libonnxruntime.dll.a")
            else()
                target_link_libraries(Ordnung PRIVATE "${ONNXRT_TP}/lib/onnxruntime.lib")
            endif()
            add_custom_command(TARGET Ordnung POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ONNXRT_TP}/lib/onnxruntime.dll"
                    "$<TARGET_FILE_DIR:Ordnung>")
        elseif(APPLE)
            file(GLOB _ORT_DYLIBS "${ONNXRT_TP}/lib/*.dylib")
            set(_ORT_LINK "")
            foreach(_L ${_ORT_DYLIBS})
                if(_L MATCHES "libonnxruntime\\.dylib$")
                    set(_ORT_LINK "${_L}")
                endif()
            endforeach()
            if(NOT _ORT_LINK)
                list(GET _ORT_DYLIBS 0 _ORT_LINK)
            endif()
            target_link_libraries(Ordnung PRIVATE "${_ORT_LINK}")
            foreach(_L ${_ORT_DYLIBS})
                add_custom_command(TARGET Ordnung POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_L}" "$<TARGET_FILE_DIR:Ordnung>")
            endforeach()
        else()
            file(GLOB _ORT_LIBS "${ONNXRT_TP}/lib/libonnxruntime.so*")
            set(_ORT_LINK "")
            foreach(_L ${_ORT_LIBS})
                if(_L MATCHES "libonnxruntime\\.so$")
                    set(_ORT_LINK "${_L}")
                endif()
            endforeach()
            if(NOT _ORT_LINK)
                list(GET _ORT_LIBS 0 _ORT_LINK)
            endif()
            target_link_libraries(Ordnung PRIVATE "${_ORT_LINK}")
            foreach(_L ${_ORT_LIBS})
                add_custom_command(TARGET Ordnung POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_L}" "$<TARGET_FILE_DIR:Ordnung>")
            endforeach()
        endif()
        message(STATUS "ONNX Runtime: Discogs-Effnet genre/mood tagging enabled")
    else()
        message(STATUS "ONNX Runtime: not found -- genre/mood tags disabled (BPM+key still work)")
    endif()
endif()

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
