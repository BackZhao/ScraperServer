include_guard(GLOBAL)

macro(CHECK_GIT_REMOTE_ROOT _DIRECTORY _DEFAULT)
  if(NOT EXISTS ${_DIRECTORY})
    message(FATAL_ERROR "Git: working directory not found")
  endif()

  if(NOT DEFINED GIT_REMOTE_NAME)
    if(DEFINED ENV{GIT_REMOTE_NAME})
      set(GIT_REMOTE_NAME "$ENV{GIT_REMOTE_NAME}")
    else()
      set(GIT_REMOTE_NAME "origin")
    endif()
  endif()

  if(NOT DEFINED GIT_REMOTE_ROOT)
    find_package(Git MODULE)

    if(DEFINED ENV{GIT_REMOTE_ROOT})
      set(GIT_REMOTE_ROOT "$ENV{GIT_REMOTE_ROOT}")
    elseif(Git_FOUND)
      execute_process(
        COMMAND ${GIT_EXECUTABLE} config --get "remote.${GIT_REMOTE_NAME}.url"
        OUTPUT_VARIABLE GIT_REMOTE_URL
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${_DIRECTORY}
      )

      if(NOT GIT_REMOTE_URL STREQUAL "")
        if(GIT_REMOTE_URL MATCHES "^(https?://[^/]+)(/[^/]+)")
          if(CMAKE_MATCH_1 STREQUAL "https://github.com")
            set(GIT_REMOTE_ROOT "${CMAKE_MATCH_1}${CMAKE_MATCH_2}")
          elseif(CMAKE_MATCH_2 STREQUAL "/gitlab")
            set(GIT_REMOTE_ROOT "${CMAKE_MATCH_1}${CMAKE_MATCH_2}")
          else()
            set(GIT_REMOTE_ROOT "${CMAKE_MATCH_1}")
          endif()
        elseif(GIT_REMOTE_URL MATCHES "^((ssh|git)://[^/]+)")
          set(GIT_REMOTE_ROOT "${CMAKE_MATCH_1}")
        elseif(GIT_REMOTE_URL MATCHES "^([^/]+)")
          set(GIT_REMOTE_ROOT "${CMAKE_MATCH_1}")
        endif()

        unset(CMAKE_MATCH_0)
        unset(CMAKE_MATCH_1)
        unset(CMAKE_MATCH_2)

        unset(GIT_REMOTE_URL)
      endif()
    endif()
  endif()

  if(NOT DEFINED GIT_REMOTE_ROOT)
    set(GIT_REMOTE_ROOT "${_DEFAULT}")
  endif()
endmacro()
