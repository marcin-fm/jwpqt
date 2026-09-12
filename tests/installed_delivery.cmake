# SPDX-License-Identifier: GPL-2.0-or-later
find_program(DESKTOP_VALIDATE desktop-file-validate REQUIRED)
find_program(MIME_UPDATE update-mime-database REQUIRED)
find_program(CPACK cpack REQUIRED)
string(RANDOM LENGTH 10 ALPHABET abcdef0123456789 suffix)
set(root "${BUILD}/tests/delivery-${suffix}")
file(MAKE_DIRECTORY "${root}/installed" "${root}/unpacked")
function(run)
  execute_process(COMMAND ${ARGV} WORKING_DIRECTORY "${root}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 90)
  if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Delivery command failed: ${ARGV}\n${output}\n${errors}")
  endif()
endfunction()
run("${CMAKE_COMMAND}" --install "${BUILD}" --prefix "${root}/installed")
foreach(path bin/jwpqt share/applications/jwpqt.desktop share/icons/hicolor/scalable/apps/jwpqt.svg
               share/mime/packages/jwpqt-mime.xml share/doc/jwpqt/handbook/start.md
               share/doc/jwpqt/handbook/manifest.json
               share/doc/jwpqt/handbook/gnugpl.txt share/doc/jwpqt/handbook/_cpright.txt
              share/doc/jwpqt/RELEASE_NOTES.md
             share/jwpqt/data/edict share/jwpqt/data/edict.jdx
             share/jwpqt/data/enamdict share/jwpqt/data/enamdict.jdx
             share/jwpqt/data/kanjinfo.dat share/jwpqt/data/radical.dat
             share/jwpqt/data/stroke.dat share/jwpqt/data/radicals.bmp)
  if(NOT EXISTS "${root}/installed/${path}")
    message(FATAL_ERROR "Missing installed file: ${path}")
  endif()
endforeach()
file(GLOB installed_help_topics "${root}/installed/share/doc/jwpqt/handbook/topics/*.md")
list(LENGTH installed_help_topics installed_help_topic_count)
if(NOT installed_help_topic_count EQUAL 125)
  message(FATAL_ERROR "Installed handbook has ${installed_help_topic_count} topics instead of 125")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
                "${root}/installed/bin/jwpqt" --version
                RESULT_VARIABLE result OUTPUT_VARIABLE version_output ERROR_VARIABLE errors TIMEOUT 30)
string(STRIP "${version_output}" version_output)
if(NOT result STREQUAL "0" OR NOT version_output STREQUAL "jwpqt ${RELEASE_VERSION}")
  message(FATAL_ERROR "Installed executable has the wrong version:\n${version_output}\n${errors}")
endif()
file(READ "${root}/installed/share/icons/hicolor/scalable/apps/jwpqt.svg" icon_contents)
if(NOT icon_contents MATCHES "<svg[^>]*viewBox=\"0 0 64 64\"")
  message(FATAL_ERROR "Installed application icon is not the scalable SVG artwork")
endif()
run("${DESKTOP_VALIDATE}" "${root}/installed/share/applications/jwpqt.desktop")
run("${MIME_UPDATE}" "${root}/installed/share/mime")
file(READ "${root}/installed/share/mime/packages/jwpqt-mime.xml" mime_definition)
foreach(required "*.jce" "*.jwp" "\\147\\046\\002\\102")
  string(FIND "${mime_definition}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Installed JWP MIME definition is missing ${required}")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
    "${root}/installed/bin/jwpqt" --resource-report
    --config-dir "${root}/config" --user-data-dir "${root}/data"
    WORKING_DIRECTORY "${root}" RESULT_VARIABLE result
    OUTPUT_VARIABLE resource_report ERROR_VARIABLE errors TIMEOUT 90)
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Installed resource report failed:\n${resource_report}\n${errors}")
endif()
foreach(required "Kanji information: loaded (6398 characters)"
                 "Radical/stroke lookup: loaded" "Radical graphics: loaded"
                 "Word dictionaries: 2 loaded" "EDICT: 110425 records"
                 "ENAMDICT: 483691 records")
  string(FIND "${resource_report}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Installed resource report is missing ${required}:\n${resource_report}")
  endif()
endforeach()
if(EXISTS "${root}/config/query-history.bin" OR EXISTS "${root}/config/jwpqt.cfg")
  message(FATAL_ERROR "Installed resource report wrote user state")
endif()
run("${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
    "${root}/installed/bin/jwpqt" --handbook --smoke-test
    --config-dir "${root}/config" --user-data-dir "${root}/data")
run("${CPACK}" --config "${BUILD}/CPackConfig.cmake" -G TGZ -B "${root}/packages")
file(GLOB archives "${root}/packages/*.tar.gz")
list(LENGTH archives count)
if(NOT count EQUAL 1)
  message(FATAL_ERROR "Expected one native archive")
endif()
list(GET archives 0 archive)
get_filename_component(archive_name "${archive}" NAME)
if(NOT archive_name STREQUAL "${PACKAGE_FILE_NAME}.tar.gz")
  message(FATAL_ERROR "Native archive has the wrong release name: ${archive_name}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}" OUTPUT_VARIABLE listing RESULT_VARIABLE result)
if(NOT result STREQUAL "0" OR listing MATCHES "(/\\.serena/|/\\.git/)")
  message(FATAL_ERROR "Archive failed inspection or includes private metadata: ${listing}")
endif()
foreach(required "/share/jwpqt/data/edict" "/share/jwpqt/data/edict.jdx"
                 "/share/jwpqt/data/enamdict" "/share/jwpqt/data/enamdict.jdx"
                 "/share/jwpqt/data/kanjinfo.dat" "/share/jwpqt/data/radical.dat"
                  "/share/jwpqt/data/stroke.dat" "/share/jwpqt/data/radicals.bmp"
                   "/share/doc/jwpqt/handbook/_cpright.txt"
                   "/share/doc/jwpqt/handbook/manifest.json"
                   "/share/doc/jwpqt/RELEASE_NOTES.md")
  string(FIND "${listing}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Native archive is missing ${required}")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xzf "${archive}"
                WORKING_DIRECTORY "${root}/unpacked" RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Archive extraction failed")
endif()
file(GLOB packaged "${root}/unpacked/*/bin/jwpqt")
list(LENGTH packaged count)
if(NOT count EQUAL 1)
  message(FATAL_ERROR "Archive layout is not relocatable")
endif()
list(GET packaged 0 binary)
file(GLOB packaged_help_topics "${root}/unpacked/*/share/doc/jwpqt/handbook/topics/*.md")
list(LENGTH packaged_help_topics packaged_help_topic_count)
if(NOT packaged_help_topic_count EQUAL 125)
  message(FATAL_ERROR "Packaged handbook has ${packaged_help_topic_count} topics instead of 125")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
                "${binary}" --version RESULT_VARIABLE result
                OUTPUT_VARIABLE version_output ERROR_VARIABLE errors TIMEOUT 30)
string(STRIP "${version_output}" version_output)
if(NOT result STREQUAL "0" OR NOT version_output STREQUAL "jwpqt ${RELEASE_VERSION}")
  message(FATAL_ERROR "Packaged executable has the wrong version:\n${version_output}\n${errors}")
endif()
run("${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
    "${binary}" --handbook --smoke-test --config-dir "${root}/config" --user-data-dir "${root}/data")
run("${CPACK}" --config "${BUILD}/CPackSourceConfig.cmake" -G TGZ -B "${root}/source")
file(GLOB sources "${root}/source/*.tar.gz")
list(LENGTH sources count)
if(NOT count EQUAL 1)
  message(FATAL_ERROR "Expected one corresponding-source archive")
endif()
list(GET sources 0 source)
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${source}"
                OUTPUT_VARIABLE listing RESULT_VARIABLE result)
if(NOT result STREQUAL "0" OR listing MATCHES "/[.](git|serena)/")
  message(FATAL_ERROR "Source archive failed inspection or includes private metadata")
endif()
foreach(required "/CMakeLists.txt" "/src/qt/help_window.cpp" "/src/core/jwp_document.cpp"
                   "/RELEASE_NOTES.md"
                   "/docs/handbook/start.md" "/docs/handbook/manifest.json"
                   "/docs/legal/gnugpl.txt"
                 "/docs/legal/original-notices.txt")
  string(FIND "${listing}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Corresponding source archive is missing ${required}")
  endif()
endforeach()
string(REGEX MATCHALL "/docs/handbook/topics/[^\n]+[.]md" source_help_topics "${listing}")
list(LENGTH source_help_topics source_help_topic_count)
if(NOT source_help_topic_count EQUAL 125)
  message(FATAL_ERROR "Corresponding source has ${source_help_topic_count} handbook topics instead of 125")
endif()
foreach(forbidden "/jwpce.cpp" "/jwpce.sln" "/changes.txt" "/toolbar.bmp")
  string(FIND "${listing}" "${forbidden}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "Corresponding source archive includes obsolete historical file ${forbidden}")
  endif()
endforeach()
message(STATUS "Installed and packaged help, corresponding source, licenses, desktop/MIME and data boundary passed")
file(REMOVE_RECURSE "${root}")
