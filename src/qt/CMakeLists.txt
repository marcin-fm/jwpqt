set(jwpqt_qt_components Svg Widgets)
if(NOT EMSCRIPTEN)
  list(APPEND jwpqt_qt_components PrintSupport)
endif()
find_package(Qt6 REQUIRED COMPONENTS ${jwpqt_qt_components})

add_library(jwpqt_qt_support STATIC
  application_settings.cpp
  application_settings.h
  clipboard_mime.cpp
  clipboard_mime.h
  edict_lookup_options.h
  edict_resource_search.cpp
  edict_resource_search.h
  edict_resources.cpp
  edict_resources.h
  file_io.cpp
  file_io.h
  kanji_color_settings.cpp
  kanji_color_settings.h
  kanji_info_options.cpp
  kanji_info_options.h
  project_workspace.cpp
  project_workspace.h
  query_history_io.cpp
  query_history_io.h
  session_io.cpp
  session_io.h
  recent_files.cpp
  recent_files.h
  text_bridge.cpp
  text_bridge.h
)

target_include_directories(jwpqt_qt_support
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(jwpqt_qt_support
  PUBLIC
    jwpqt_core
    Qt6::Core
)

if(MSVC)
  target_compile_options(jwpqt_qt_support PRIVATE /W4 /permissive-)
else()
  target_compile_options(jwpqt_qt_support PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(EMSCRIPTEN)
  target_compile_options(jwpqt_qt_support PRIVATE -fexceptions)
endif()

set(jwpqt_qt_ui_sources
  window_geometry.cpp
  window_geometry.h
  help_window.cpp
  help_window.h
  application_settings_dialog.cpp
  application_settings_dialog.h
  japanese_fonts.cpp
  japanese_fonts.h
  character_context_menu.cpp
  character_context_menu.h
  edict_lookup_dialog.cpp
  edict_lookup_dialog.h
  find_replace_dialog.cpp
  find_replace_dialog.h
  auxiliary_find.cpp
  auxiliary_find.h
  edict_results_window.cpp
  edict_results_window.h
  edict_user_dictionary_dialog.cpp
  edict_registry_dialog.cpp
  edict_registry_dialog.h
  edict_user_dictionary_dialog.h
  jis_table_dialog.cpp
  jis_table_dialog.h
  jwp_editor.cpp
  jwp_editor.h
  input_mode.h
  kana_input_field.cpp
  kana_input_field.h
  kanji_code_lookup_dialog.cpp
  kanji_code_lookup_dialog.h
  kanji_count_dialog.cpp
  kanji_count_dialog.h
  kanji_info_dialog.cpp
  kanji_info_dialog.h
  kanji_info_options_dialog.cpp
  kanji_info_options_dialog.h
  kanji_lookup_dialog.cpp
  kanji_lookup_dialog.h
  kanji_result_keys.h
  kanji_reading_lookup_dialog.cpp
  kanji_reading_lookup_dialog.h
  lookup_artwork.cpp
  lookup_artwork.h
  main_window.cpp
  main_window.h
  toolbar_settings.h
  toolbar_dialog.cpp
  toolbar_dialog.h
  vector_artwork.cpp
  vector_artwork.h
  page_layout_dialog.cpp
  page_layout_dialog.h
  jwp_text_drawing.cpp
  jwp_text_drawing.h
  wnn_user_dictionary_dialog.cpp
  wnn_user_dictionary_dialog.h
)

if(NOT EMSCRIPTEN)
  list(APPEND jwpqt_qt_ui_sources
    print_document.cpp
    print_document.h
  )
endif()

add_library(jwpqt_qt_ui STATIC ${jwpqt_qt_ui_sources})

target_compile_definitions(jwpqt_qt_ui PUBLIC JWPQT_VERSION="${PROJECT_VERSION}")
qt_add_resources(jwpqt_qt_ui handbook_assets
  PREFIX "/jwpqt/help"
  BASE "${PROJECT_SOURCE_DIR}/docs/handbook"
  FILES
    "${PROJECT_SOURCE_DIR}/docs/handbook/start.md"
    "${PROJECT_SOURCE_DIR}/docs/handbook/files.md"
    "${PROJECT_SOURCE_DIR}/docs/handbook/editing.md"
    "${PROJECT_SOURCE_DIR}/docs/handbook/dictionary.md"
    "${PROJECT_SOURCE_DIR}/docs/handbook/kanji.md"
    "${PROJECT_SOURCE_DIR}/docs/handbook/printing.md"
    "${PROJECT_SOURCE_DIR}/docs/handbook/settings.md"
    "${PROJECT_SOURCE_DIR}/docs/handbook/installation.md"
    "${PROJECT_SOURCE_DIR}/docs/handbook/about.md"
)
set_source_files_properties("${PROJECT_SOURCE_DIR}/docs/legal/gnugpl.txt"
  PROPERTIES QT_RESOURCE_ALIAS "gnugpl.txt")
set_source_files_properties("${PROJECT_SOURCE_DIR}/docs/legal/original-notices.txt"
  PROPERTIES QT_RESOURCE_ALIAS "_cpright.txt")
qt_add_resources(jwpqt_qt_ui notice_assets PREFIX "/jwpqt/help"
  FILES "${PROJECT_SOURCE_DIR}/docs/legal/gnugpl.txt"
        "${PROJECT_SOURCE_DIR}/docs/legal/original-notices.txt")

qt_add_resources(jwpqt_qt_ui toolbar_assets
  PREFIX "/jwpqt"
  BASE "${PROJECT_SOURCE_DIR}"
  FILES "${PROJECT_SOURCE_DIR}/assets/icons/new.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/open.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/save.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/delete.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/print.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/cut.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/copy.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/paste.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/undo.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/redo.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/find.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/find-next.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/find-previous.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/replace.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/options.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/info.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/jis-table.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/count.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/kana.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/ascii.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/jascii.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/convert.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/dictionary.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/color-list.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/file-format.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/paragraph-format.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/page-layout.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/user-dictionary.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/kanji-lookup.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/bushu.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/stroke-bushu.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/skip.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/spahn.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/four-corner.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/reading.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/index.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/skip-diagram.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/four-corner-diagram.svg"
        "${PROJECT_SOURCE_DIR}/assets/icons/jwpqt.svg"
        "${PROJECT_SOURCE_DIR}/assets/data/spahn-radicals.bmp"
        "${PROJECT_SOURCE_DIR}/assets/data/wnn.dat"
        "${PROJECT_SOURCE_DIR}/assets/data/wnn.dix"
)

if(EMSCRIPTEN)
  qt_add_resources(jwpqt_qt_ui web_font_assets
    PREFIX "/jwpqt"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES "${PROJECT_SOURCE_DIR}/assets/fonts/NotoSansJP-wght.ttf"
  )
endif()

target_link_libraries(jwpqt_qt_ui
  PUBLIC
    jwpqt_qt_support
    Qt6::Svg
    Qt6::Widgets
)
if(NOT EMSCRIPTEN)
  target_link_libraries(jwpqt_qt_ui PUBLIC Qt6::PrintSupport)
endif()

if(MSVC)
  target_compile_options(jwpqt_qt_ui PRIVATE /W4 /permissive-)
else()
  target_compile_options(jwpqt_qt_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(EMSCRIPTEN)
  target_compile_options(jwpqt_qt_ui PRIVATE -fexceptions)
endif()

qt_add_executable(jwpqt
  WIN32
  MACOSX_BUNDLE
  main.cpp
)

if(WIN32)
  configure_file("${PROJECT_SOURCE_DIR}/packaging/jwpqt.rc.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/jwpqt.rc" @ONLY)
  target_sources(jwpqt PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/jwpqt.rc")
elseif(APPLE AND NOT EMSCRIPTEN)
  set_source_files_properties("${PROJECT_SOURCE_DIR}/packaging/jwpqt.icns"
    PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  target_sources(jwpqt PRIVATE "${PROJECT_SOURCE_DIR}/packaging/jwpqt.icns")
endif()

target_link_libraries(jwpqt PRIVATE jwpqt_qt_ui)
if(MSVC)
  target_compile_options(jwpqt PRIVATE /W4 /permissive-)
else()
  target_compile_options(jwpqt PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(EMSCRIPTEN)
  target_compile_options(jwpqt PRIVATE -fexceptions)
  target_link_options(jwpqt PRIVATE
    -fexceptions
    "SHELL:-sASYNCIFY"
    "SHELL:-sDISABLE_EXCEPTION_CATCHING=0"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-lidbfs.js"
    -Os
  )
  if(JWPQT_BUNDLE_LEGACY_DATA)
    foreach(index RANGE 0 7)
      list(GET JWPQT_LEGACY_DATA_FILES ${index} source)
      list(GET JWPQT_LEGACY_DATA_NAMES ${index} name)
      target_link_options(jwpqt PRIVATE
        "SHELL:--preload-file ${source}@/jwpqt-data/${name}")
    endforeach()
  endif()
endif()

if(JWPQT_BUNDLE_LEGACY_DATA AND NOT EMSCRIPTEN)
  foreach(index RANGE 0 7)
    list(GET JWPQT_LEGACY_DATA_FILES ${index} source)
    list(GET JWPQT_LEGACY_DATA_NAMES ${index} name)
    add_custom_command(TARGET jwpqt POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:jwpqt>/data"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${source}" "$<TARGET_FILE_DIR:jwpqt>/data/${name}"
      VERBATIM)
  endforeach()
endif()

set_target_properties(jwpqt PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "JWPqt"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.jwpqt.JWPqt"
  MACOSX_BUNDLE_ICON_FILE "jwpqt.icns"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

if(EMSCRIPTEN)
  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/jwpqt.js"
    "${CMAKE_CURRENT_BINARY_DIR}/jwpqt.wasm"
    "${CMAKE_CURRENT_BINARY_DIR}/qtloader.js"
    DESTINATION "."
  )
  if(JWPQT_BUNDLE_LEGACY_DATA)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/jwpqt.data" DESTINATION ".")
  endif()
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/jwpqt.html"
          DESTINATION "." RENAME "index.html")
  install(FILES "${PROJECT_SOURCE_DIR}/assets/icons/jwpqt.svg"
          DESTINATION "." RENAME "qtlogo.svg")
  install(FILES "${PROJECT_SOURCE_DIR}/assets/fonts/NotoSansJP-OFL.txt"
          DESTINATION "licenses")
  install(FILES
    "${PROJECT_SOURCE_DIR}/docs/legal/gnugpl.txt"
    DESTINATION "licenses")
  install(FILES "${PROJECT_SOURCE_DIR}/docs/legal/original-notices.txt"
          DESTINATION "licenses" RENAME "_cpright.txt")
else()
  install(TARGETS jwpqt
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
  if(JWPQT_BUNDLE_LEGACY_DATA)
    if(APPLE)
      set(jwpqt_data_destination "jwpqt.app/Contents/Resources/data")
    else()
      set(jwpqt_data_destination "${CMAKE_INSTALL_DATADIR}/jwpqt/data")
    endif()
    foreach(index RANGE 0 7)
      list(GET JWPQT_LEGACY_DATA_FILES ${index} source)
      list(GET JWPQT_LEGACY_DATA_NAMES ${index} name)
      install(FILES "${source}" DESTINATION "${jwpqt_data_destination}"
              RENAME "${name}")
    endforeach()
  endif()
endif()

if((WIN32 OR APPLE) AND NOT EMSCRIPTEN)
  qt_generate_deploy_app_script(
    TARGET jwpqt
    OUTPUT_SCRIPT jwpqt_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
  )
  install(SCRIPT "${jwpqt_deploy_script}")
endif()
