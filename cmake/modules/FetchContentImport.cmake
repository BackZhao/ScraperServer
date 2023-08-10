include_guard(GLOBAL)

include(FetchContent)

function(FetchContent_Import contentName)
  string(TOLOWER ${contentName} contentNameLower)
  string(TOUPPER ${contentName} contentNameUpper)

  set(options EXCLUDE_FROM_ALL)
  set(oneValueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "${options}" "${oneValueArgs}" "${multiValueArgs}")

  if(NOT FETCHCONTENT_REPO_DIR)
    if(DEFINED ENV{FETCHCONTENT_REPO_DIR})
      set(FETCHCONTENT_REPO_DIR "$ENV{FETCHCONTENT_REPO_DIR}")
    endif()
  endif()

  if($ENV{CI})
    if(NOT FETCHCONTENT_REPO_DIR)
        set(FETCHCONTENT_REPO_DIR "${FETCHCONTENT_BASE_DIR}")
    endif()
  endif()

  if(FETCHCONTENT_REPO_DIR)
    list(FIND ARG_UNPARSED_ARGUMENTS GIT_REPOSITORY indexRepo)
    if(indexRepo GREATER_EQUAL 0)
      math(EXPR indexRepo "${indexRepo}+1")
      list(GET ARG_UNPARSED_ARGUMENTS ${indexRepo} repoUrl)

      if(NOT Git_FOUND)
        find_package(Git QUIET REQUIRED)
      endif()

      # 1. mirror repo:
      set(repoDir "${FETCHCONTENT_REPO_DIR}/${contentNameLower}-repo")

      if(NOT FETCHCONTENT_FULLY_DISCONNECTED)
        if(EXISTS ${repoDir})
          if(FETCHCONTENT_UPDATES_DISCONNECTED OR
            FETCHCONTENT_UPDATES_DISCONNECTED_${contentNameUpper})
            set(disconnectUpdates True)
          else()
            set(disconnectUpdates False)
          endif()

          if(disconnectUpdates)
            message(STATUS "Update ${contentName} (disabled)")
          else()
            message(STATUS "Update ${contentName}")

            execute_process(COMMAND ${GIT_EXECUTABLE} remote set-url origin ${repoUrl}
              WORKING_DIRECTORY ${repoDir}
              RESULT_VARIABLE   commandResult)
            if(commandResult)
              message(FATAL_ERROR "Reconfigure ${contentName} failed")
            endif()

            execute_process(COMMAND ${GIT_EXECUTABLE} remote update --prune
              WORKING_DIRECTORY ${repoDir}
              RESULT_VARIABLE   commandResult)
            if(commandResult)
              message(WARNING "Update ${contentName} failed")
            endif()
          endif()
        else()
          message(STATUS "Clone ${contentName}")

          execute_process(COMMAND ${GIT_EXECUTABLE} clone --mirror ${repoUrl} ${repoDir}
            RESULT_VARIABLE   commandResult)
          if(commandResult)
            message(FATAL_ERROR "Clone repo ${repoUrl} failed")
          endif()
        endif()
      endif()

      # 2. replace repo: repoUrl <- repoDir
      list(REMOVE_AT ARG_UNPARSED_ARGUMENTS ${indexRepo})
      list(INSERT ARG_UNPARSED_ARGUMENTS ${indexRepo} ${repoDir})
    endif()
  endif()

  FetchContent_Declare(${contentNameLower} ${ARG_UNPARSED_ARGUMENTS})

  FetchContent_GetProperties(${contentNameLower})

  if(NOT ${contentNameLower}_POPULATED)
    message(STATUS "Populating ${contentNameLower}")
    FetchContent_Populate(${contentNameLower})

    if(EXISTS ${${contentNameLower}_SOURCE_DIR}/CMakeLists.txt)
      if(ARG_EXCLUDE_FROM_ALL)
        add_subdirectory(
          ${${contentNameLower}_SOURCE_DIR} ${${contentNameLower}_BINARY_DIR}
          EXCLUDE_FROM_ALL
          )

        if(NOT DEFINED FETCHCONTENT_CHECK_INCLUDE_SYSTEM)
          if(NOT DEFINED CMAKE_CXX_LINK_PIE_SUPPORTED AND NOT DEFINED CMAKE_CXX_LINK_NO_PIE_SUPPORTED)
            include(CheckPIESupported)
            check_pie_supported(LANGUAGES CXX)
          endif()
          set(_CHECK_INCLUDE_SYSTEM_DIR ${CMAKE_BINARY_DIR}/CMakeFiles/CMakeTmp)
          file(WRITE ${_CHECK_INCLUDE_SYSTEM_DIR}/CheckIncludeSystem.h   [=[
template <typename T>
T max(T &a, T &b) { return a > b ? a : b; }
]=])
          file(WRITE ${_CHECK_INCLUDE_SYSTEM_DIR}/CheckIncludeSystem.cpp [=[
#include <CheckIncludeSystem.h>
int main() { return 0; }
]=])
          try_compile(FETCHCONTENT_CHECK_INCLUDE_SYSTEM ${CMAKE_BINARY_DIR} SOURCES ${_CHECK_INCLUDE_SYSTEM_DIR}/CheckIncludeSystem.cpp
            COMPILE_DEFINITIONS ${CMAKE_INCLUDE_SYSTEM_FLAG_CXX} ${_CHECK_INCLUDE_SYSTEM_DIR} ${_CMAKE_INCLUDE_SYSTEM_FLAG_CXX_WARNING})
          mark_as_advanced(FETCHCONTENT_CHECK_INCLUDE_SYSTEM)
          unset(_CHECK_INCLUDE_SYSTEM_DIR)
        endif()
        if(NOT FETCHCONTENT_CHECK_INCLUDE_SYSTEM)
          return()
        endif()

        FetchContent_EnumerateTargets(targets ${${contentNameLower}_SOURCE_DIR} TYPE "_LIBRARY$")
        FetchContent_IgnoreWarning(${targets})
      else()
        add_subdirectory(
          ${${contentNameLower}_SOURCE_DIR} ${${contentNameLower}_BINARY_DIR}
          )
      endif()
    endif()
  endif()

endfunction()


function(FetchContent_EnumerateTargets var dir)
  set(libs)

  set(options)
  set(oneValueArgs TYPE)
  set(multiValueArgs)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "${options}" "${oneValueArgs}" "${multiValueArgs}")

  get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    if(ARG_TYPE)
      foreach(target IN LISTS targets)
          get_property(type TARGET ${target} PROPERTY TYPE)
          if(type MATCHES "${ARG_TYPE}")
            list(APPEND libs ${target})
          endif()
      endforeach()
    else()
      list(APPEND libs ${targets})
    endif()

  get_property(subDirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
  foreach(childDir IN LISTS subDirs)
    FetchContent_EnumerateTargets(childLibs ${childDir} ${ARGN})
    list(APPEND libs ${childLibs})
  endforeach()

  set(${var} ${libs} PARENT_SCOPE)

  # message(STATUS "${CMAKE_CURRENT_FUNCTION_LIST_FILE}:${CMAKE_CURRENT_LIST_LINE}")
  # cmake_print_variables(dir libs)
endfunction()


if(CMAKE_VERSION VERSION_LESS 3.25)
  # CMAKE_INCLUDE_SYSTEM_FLAG_C
  # CMAKE_INCLUDE_SYSTEM_FLAG_CXX
  function(FetchContent_IgnoreWarning)
    # https://cmake.org/cmake/help/latest/command/target_include_directories.html
    # https://cmake.org/cmake/help/latest/prop_tgt/INTERFACE_SYSTEM_INCLUDE_DIRECTORIES.html#prop_tgt:INTERFACE_SYSTEM_INCLUDE_DIRECTORIES

    foreach(target IN LISTS ARGN)
      # set_property(TARGET ${target} PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:${target},INTERFACE_INCLUDE_DIRECTORIES>)

      # message(STATUS "${CMAKE_CURRENT_FUNCTION_LIST_FILE}:${CMAKE_CURRENT_LIST_LINE}")
      # cmake_print_properties(TARGETS ${target} PROPERTIES TYPE INTERFACE_INCLUDE_DIRECTORIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)

      get_property(prop TARGET ${target} PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
      if(prop)
        return()
      endif()

      get_property(prop TARGET ${target} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
      if(prop)
        set_property(TARGET ${target} PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${prop})
      endif()
    endforeach()
  endfunction()
else()
  # https://cmake.org/cmake/help/latest/prop_tgt/SYSTEM.html
  function(FetchContent_IgnoreWarning)
    set_property(TARGET ${ARGN} PROPERTY SYSTEM Y)
  endfunction()
endif()
