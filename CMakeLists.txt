cmake_minimum_required(VERSION 3.21)

project(jwpqt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(GNUInstallDirs)

add_subdirectory(src/core)
add_subdirectory(src/qt)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(DIRECTORY docs/handbook/ DESTINATION "${CMAKE_INSTALL_DOCDIR}/handbook")
install(FILES README.md PORTING.md DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES gnugpl.txt _cpright.txt DESTINATION "${CMAKE_INSTALL_DOCDIR}/handbook")
install(FILES packaging/jwpqt.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES packaging/jwpqt-mime.xml DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages")
install(FILES assets/icons/jwpqt.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")

set(CPACK_PACKAGE_NAME "jwpqt")
set(CPACK_PACKAGE_VENDOR "jwpqt contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Qt Japanese word processor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/gnugpl.txt")
set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_FILE_NAME "jwpqt-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGING_INSTALL_PREFIX "/")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "jwpqt-${PROJECT_VERSION}-Source")
set(CPACK_SOURCE_IGNORE_FILES "/[.]git/;/[.]serena/;/_CPack_Packages/;/CMakeFiles/;CMakeCache[.]txt$;[.]swp$;~$")
include(CPack)
