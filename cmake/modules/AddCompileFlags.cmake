include_guard(GLOBAL)

macro(add_compile_flags name)
  if("${name}" STREQUAL "")
    set(compile_flags compile_flags)
  else()
    set(compile_flags ${name}_compile_flags)
  endif()

  add_library(${compile_flags} INTERFACE)
  target_compile_features(${compile_flags}
    INTERFACE
      ${ARGN}
      )
endmacro()
