cmake_minimum_required(VERSION 3.16)

find_package(Git QUIET)

set(VERSION_H @VERSION_H@)

find_package(Git QUIET)
if(Git_FOUND AND (EXISTS "@CMAKE_CURRENT_SOURCE_DIR@/.git"))
  set(HAVE_GIT 1)
else()
  set(HAVE_GIT 0)
endif()

if(HAVE_GIT)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} diff --quiet HEAD
      RESULT_VARIABLE WORKING_COPY_STATUS
      )
  if("${WORKING_COPY_STATUS}" STREQUAL "0")
    set(WORKING_COPY_DIRTY  "")
  else()
    set(WORKING_COPY_DIRTY "+")
  endif()

  execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%ad --date=format:%Y%m%d
    OUTPUT_VARIABLE PROJECT_VERSION_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(NOT PROJECT_VERSION_DATE)
    message(FATAL_ERROR "unknown git commit date")
  endif()

  execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%h
    OUTPUT_VARIABLE PROJECT_VERSION_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(NOT PROJECT_VERSION_HASH)
    message(FATAL_ERROR "unknown git commit hash")
  endif()

else()
    string(TIMESTAMP PROJECT_VERSION_DATE %Y%m%d)
    set(PROJECT_VERSION_HASH 0000000)
endif()

set(VERSION_NEW "#define VERSION \"@PROJECT_VERSION_MAJOR@.@PROJECT_VERSION_MINOR@-${PROJECT_VERSION_DATE}-${PROJECT_VERSION_HASH}${WORKING_COPY_DIRTY}\"\n")

if(EXISTS ${VERSION_H})
  file(READ ${VERSION_H} VERSION_OLD)
  if("${VERSION_OLD}" STREQUAL "${VERSION_NEW}")
    return()
  endif()
endif()

message(STATUS "VERSION: ${VERSION_NEW}")

file(WRITE ${VERSION_H} "${VERSION_NEW}")
