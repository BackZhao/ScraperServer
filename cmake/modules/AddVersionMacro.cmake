include_guard(GLOBAL)

macro(add_version_macro name)
  set(VERSION_H ${CMAKE_CURRENT_BINARY_DIR}/version.h)

  set(VERSION_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/version.cmake)
  set(VERSION_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/version.cmake)

  if(NOT EXISTS ${VERSION_H})
    execute_process(COMMAND ${CMAKE_COMMAND} -E touch ${VERSION_H})
    execute_process(COMMAND ${CMAKE_COMMAND} -E touch ${VERSION_SCRIPT})
  endif()

  find_package(Git QUIET MODULE)
  if(Git_FOUND AND (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git/"))

    set(VERSION_DEPENDS ${VERSION_SCRIPT})
    foreach(i ${CMAKE_CURRENT_SOURCE_DIR}/.git/HEAD ${CMAKE_CURRENT_SOURCE_DIR}/.git/objects)
      if(EXISTS ${i})
        list(APPEND VERSION_DEPENDS ${i})
      endif()
    endforeach()

    configure_file(${VERSION_TEMPLATE} ${VERSION_SCRIPT} @ONLY)

    add_custom_command(OUTPUT ${VERSION_H}
      COMMAND ${CMAKE_COMMAND} -P ${VERSION_SCRIPT}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      DEPENDS ${VERSION_DEPENDS}
      )
  endif()

  if("${name}" STREQUAL "")
    set(version_macro version_macro)
  else()
    set(version_macro ${name}_version_macro)
  endif()

  add_library(${version_macro} INTERFACE)
  target_sources(${version_macro} INTERFACE ${VERSION_H})
  target_include_directories(
    ${version_macro} INTERFACE ${CMAKE_CURRENT_BINARY_DIR}
    )
endmacro()
