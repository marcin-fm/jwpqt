cmake_minimum_required(VERSION 3.21)

project(jwpqt VERSION 2.0.3 LANGUAGES CXX)

set(JWPQT_RELEASE_VERSION "2.03")

if(WIN32)
  enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)

option(JWPQT_BUNDLE_LEGACY_DATA
       "Package the separately licensed noncommercial JWPce lookup data" ON)
set(JWPQT_LEGACY_DATA_FILES)
if(JWPQT_BUNDLE_LEGACY_DATA)
  set(jwpqt_legacy_data_specs
    "edict|84fd27bf20fce1c923d68dcecbc873bdfcd25b8db2c214717375630e91904650"
    "edict.jdx|eb0bcb67c37816f220d3d45e82cb0b61680c5180ce2baa628d21b7846c01c50a"
    "enamdict|fab01f4a633a4959126180804f344f734e3cf434dc419a2e98c999ea23bd90e8"
    "enamdict.jdx|4f12b1dcb2d211f2fb0b086f551182484c68c64640d6f0561800d4c941942162"
    "kanjinfo.dat|310f5e12e924add9ed20c6977f8b7e98efacfafdef0cf36ea724c2d2e96e78e1"
    "radical.dat|2109454fd2bc184f99a67fedbed1fde66132b6c7b5f17c8fdcd0de8a8c017102"
    "stroke.dat|21a787ce9f7b392b820a878fcc7d3bcf7c1764f162b5c944539ba9746e1afef4"
    "radicals.bmp|7480588873b241a873944a4a1b9e9b1d8380e11277b95f22f9fa1cc25f4236dd"
  )
  foreach(spec IN LISTS jwpqt_legacy_data_specs)
    string(REPLACE "|" ";" fields "${spec}")
    list(GET fields 0 name)
    list(GET fields 1 expected_sha256)
    if(name STREQUAL "radicals.bmp")
      set(path "${PROJECT_SOURCE_DIR}/assets/data/kanji-radicals.bmp")
    else()
      set(path "${PROJECT_SOURCE_DIR}/assets/data/${name}")
    endif()
    if(NOT EXISTS "${path}")
      message(FATAL_ERROR "Required JWPce data file is missing: ${path}")
    endif()
    file(SHA256 "${path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
      message(FATAL_ERROR "JWPce data hash mismatch for ${path}")
    endif()
    list(APPEND JWPQT_LEGACY_DATA_FILES "${path}")
    list(APPEND JWPQT_LEGACY_DATA_NAMES "${name}")
  endforeach()
endif()

set(jwpqt_package_suffix "")
if(JWPQT_BUNDLE_LEGACY_DATA)
  set(jwpqt_package_suffix "-noncommercial-data")
endif()
set(JWPQT_PACKAGE_FILE_NAME
    "jwpqt-${JWPQT_RELEASE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}${jwpqt_package_suffix}")

if(MSVC)
  add_compile_options(/utf-8)
endif()

add_subdirectory(src/core)
add_subdirectory(src/qt)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(DIRECTORY docs/handbook/ DESTINATION "${CMAKE_INSTALL_DOCDIR}/handbook")
install(FILES README.md PORTING.md RELEASE_NOTES.md
        DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES docs/legal/gnugpl.txt DESTINATION "${CMAKE_INSTALL_DOCDIR}/handbook")
install(FILES docs/legal/original-notices.txt
        DESTINATION "${CMAKE_INSTALL_DOCDIR}/handbook" RENAME "_cpright.txt")
if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  install(FILES packaging/jwpqt.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
  install(FILES packaging/jwpqt-mime.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages")
  install(FILES assets/icons/jwpqt.svg
          DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
endif()

set(CPACK_PACKAGE_NAME "jwpqt")
set(CPACK_PACKAGE_VENDOR "jwpqt contributors")
if(JWPQT_BUNDLE_LEGACY_DATA)
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
      "Native Qt Japanese word processor with separately licensed noncommercial lookup data")
else()
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Qt Japanese word processor")
endif()
set(CPACK_PACKAGE_VERSION "${JWPQT_RELEASE_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/docs/legal/gnugpl.txt")
if(WIN32)
  set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
set(CPACK_PACKAGE_FILE_NAME "${JWPQT_PACKAGE_FILE_NAME}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_SOURCE_GENERATOR "TGZ")
if(JWPQT_BUNDLE_LEGACY_DATA)
  set(CPACK_SOURCE_PACKAGE_FILE_NAME
      "jwpqt-${JWPQT_RELEASE_VERSION}-Source-Noncommercial-Data")
else()
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "jwpqt-${JWPQT_RELEASE_VERSION}-Source")
endif()
set(CPACK_SOURCE_IGNORE_FILES "/[.]git/;/[.]serena/;/[.]local/;/[.]playwright-mcp/;/node_modules/;/_CPack_Packages/;/CMakeFiles/;CMakeCache[.]txt$;[.]swp$;~$")
if(NOT JWPQT_BUNDLE_LEGACY_DATA)
  list(APPEND CPACK_SOURCE_IGNORE_FILES
       "/assets/data/(edict([.]jdx)?|enamdict([.]jdx)?|kanjinfo[.]dat|radical[.]dat|stroke[.]dat|kanji-radicals[.]bmp)$")
endif()
include(CPack)
