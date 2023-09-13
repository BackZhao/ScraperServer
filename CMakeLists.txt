cmake_minimum_required(VERSION 3.16)

project(
    ScraperServer
    VERSION 1.0
    LANGUAGES CXX C
)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

option(XXX_GIT_SHALLOW "Enable git shallow" ON)
option(XXX_GIT_PROGRESS "Show git progress" OFF)

include(CheckGitRemoteRoot)
check_git_remote_root(${CMAKE_CURRENT_SOURCE_DIR} git@gitee.com)

include(AddVersionMacro)
include(AddCompileFlags)

include(FetchContentImport)

set(CMAKE_CXX_STANDARD 11)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# version
include(AddVersionMacro)
add_version_macro(${PROJECT_NAME})

# AddressSanitizer requirement: Clang>=3.1 | GCC>=4.8
option(XXX_ENABLE_ASAN "Run AddressSanitizer" OFF)
add_library(asan_flags INTERFACE)
if(XXX_ENABLE_ASAN)
  if(MSVC)
    set(asan_flags /fsanitize=address)
    target_compile_options(asan_flags INTERFACE ${asan_flags})
    target_link_options(asan_flags INTERFACE ${asan_flags})
  else()
    set(asan_flags -fno-omit-frame-pointer -fsanitize=address)
    target_compile_options(
      asan_flags INTERFACE # $<$<CONFIG:Debug>:${asan_flags}>
                           ${asan_flags}
      )
    target_link_options(
      asan_flags INTERFACE
      # $<$<CONFIG:Debug>:${asan_flags}>
      ${asan_flags}
      )
  endif()
endif()

# compile flags
include(AddCompileFlags)
add_compile_flags(${PROJECT_NAME} c_std_11 cxx_std_11)

if(MSVC)
  target_compile_options(${compile_flags} INTERFACE /utf-8)
  target_compile_definitions(${compile_flags} INTERFACE _CRT_SECURE_NO_WARNINGS)

  if("${CMAKE_CL_NOLOGO}" STREQUAL "")
    target_compile_options(${compile_flags} INTERFACE /nologo)
    target_link_options(${compile_flags} INTERFACE /nologo)
  endif()
else()
  target_compile_options(
    ${compile_flags}
    INTERFACE -Wall
              -Wextra
              -Wshadow
              -Wformat=2
              -Wno-write-strings
              $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
              $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>
    )
endif()
target_compile_definitions(${compile_flags} INTERFACE _FILE_OFFSET_BITS=64)

target_include_directories(
  ${compile_flags} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/protocol"
                              "${CMAKE_CURRENT_LIST_DIR}/src"
  )
target_link_libraries(${compile_flags} INTERFACE asan_flags)

# getopt
if(NOT TARGET getopt)
if(WIN32)
  find_package(getopt QUIET)
  if(NOT TARGET getopt)
    FetchContent_Import(
      getopt
      GIT_REPOSITORY "${GIT_REMOTE_ROOT}/getopt.git"
      GIT_SHALLOW  ${XXX_GIT_SHALLOW}
      GIT_PROGRESS ${XXX_GIT_PROGRESS}
      EXCLUDE_FROM_ALL
      )
  endif()
  if(NOT TARGET getopt)
    message(FATAL_ERROR "getopt is not found")
  endif()
else()
  add_library(getopt INTERFACE)
endif()
endif()

# poco
find_package(Poco QUIET COMPONENTS Foundation)
if(NOT TARGET Poco::Foundation)
  set(DISABLE_CPP14 ON)
  foreach(
    prop
    ENABLE_JSON
    ENABLE_ZIP
    ENABLE_XML
    )
    set(${prop} ON)
  endforeach()

  foreach(
    prop
    ## apache
    ENABLE_APACHECONNECTOR
    ## encoding
    ENABLE_ENCODINGS
    ## zip
    # ENABLE_ZIP
    ## format
    # ENABLE_XML
    # ENABLE_JSON
    ## ssl
    ENABLE_NETSSL
    ENABLE_CRYPTO
    ENABLE_JWT
    ## cache
    ENABLE_REDIS
    ENABLE_MONGODB
    ## data
    ENABLE_DATA
    ENABLE_DATA_ODBC
    ENABLE_DATA_SQLITE
    ENABLE_DATA_MYSQL
    ENABLE_DATA_POSTGRESQL
    ## compiler
    ENABLE_PAGECOMPILER
    ENABLE_PAGECOMPILER_FILE2PAGE
    ENABLE_ACTIVERECORD
    ENABLE_ACTIVERECORD_COMPILER
    )
    set(${prop} OFF)
  endforeach()

  FetchContent_Import(
    poco
    GIT_REPOSITORY "${GIT_REMOTE_ROOT}/poco.git"
    GIT_TAG poco-1.12.4-release
    GIT_SHALLOW  ${XXX_GIT_SHALLOW}
    GIT_PROGRESS ${XXX_GIT_PROGRESS}
    EXCLUDE_FROM_ALL
    )
endif()

# spdlog
find_package(spdlog QUIET)
if(NOT TARGET spdlog::spdlog_header_only)
  FetchContent_Import(
    spdlog
    GIT_REPOSITORY "${GIT_REMOTE_ROOT}/spdlog.git"
    GIT_TAG v1.x
    GIT_SHALLOW  ${XXX_GIT_SHALLOW}
    GIT_PROGRESS ${XXX_GIT_PROGRESS}
    EXCLUDE_FROM_ALL 
    )
endif()

set(BUILD_ZLIB ON CACHE BOOL "Build zlib shared library")
set(BUILD_ZENLIB ON CACHE BOOL "Build ZenLib shared library")
set(ENABLE_UNICODE OFF CACHE BOOL "Enable unicode support")

set(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "Skip install libs from zlib")
set(SKIP_INSTALL_FILES ON CACHE BOOL "Skip install files from zlib")
set(SKIP_INSTALL_HEADERS ON CACHE BOOL "Skip install headers from zlib")
set(SKIP_INSTALL_ALL ON  CACHE BOOL "Skip install all from zlib")

# mediainfo
# find_package(mediainfo QUIET)
# if(NOT TARGET mediainfo)
  FetchContent_Import(
    mediainfo
    GIT_REPOSITORY "${GIT_REMOTE_ROOT}/MediaInfoLib.git"
    GIT_TAG cmake
    GIT_SHALLOW  ${XXX_GIT_SHALLOW}
    GIT_PROGRESS ${XXX_GIT_PROGRESS}
    EXCLUDE_FROM_ALL 
    )
  FetchContent_GetProperties(mediainfo)
  if(NOT mediainfo_POPULATED)
    FetchContent_Populate(mediainfo)
    add_subdirectory(${mediainfo_SOURCE_DIR}/Project/CMake ${mediainfo_BINARY_DIR})
  endif ()
# endif()

set_target_properties(
  Net
  JSON
  XML
  Foundation
  spdlog_header_only
  mediainfo
  PROPERTIES SYSTEM true
)

add_library(Logger
  STATIC
  src/Logger.cpp
  )
target_link_libraries(Logger
  PUBLIC
    spdlog::spdlog_header_only
  )

add_library(Option
  STATIC
  src/Option.cpp
  )
target_link_libraries(Option
  PRIVATE
  ${version_macro}
  getopt
  )

add_library(Config
  STATIC
  src/Config.cpp
  )
target_link_libraries(Config
  PRIVATE
  Poco::JSON
  Poco::Foundation
  Logger
  )

add_executable(Demo
  src/Demo.cpp
  src/HttpRequestHandler.cpp
  src/HttpServer.cpp
  src/ApiManager.cpp
  src/DataConvert.cpp
  src/DataSource.cpp
  src/ResourceManager.cpp
  src/TMDBAPI.cpp
  src/Utils.cpp
  )
target_link_libraries(Demo
  PRIVATE
    ${version_macro}
    ${compile_flags}
    Logger
    Config
    Option
    Poco::Net
    Poco::JSON
    Poco::XML
    Poco::Zip
    mediainfo
  )

add_executable(ScraperServer
  src/main.cpp
  src/HttpRequestHandler.cpp
  src/HttpServer.cpp
  src/ApiManager.cpp
  src/DataConvert.cpp
  src/DataSource.cpp
  src/ResourceManager.cpp
  src/TMDBAPI.cpp
  src/Utils.cpp
  )
target_link_libraries(ScraperServer
  PRIVATE
    ${version_macro}
    ${compile_flags}
    Logger
    Config
    Option
    Poco::Net
    Poco::JSON
    Poco::XML
    Poco::Zip
    mediainfo
  )

target_include_directories(Demo SYSTEM PRIVATE ${mediainfo_SOURCE_DIR}/Source)
target_include_directories(ScraperServer SYSTEM PRIVATE ${mediainfo_SOURCE_DIR}/Source)
# if(CMAKE_VERSION VERSION_LESS 3.17)
#   foreach(target Net JSON XML Foundation)
#     get_property(target_header TARGET ${target} PROPERTY PUBLIC_HEADER SET)
#     if(target_header)
#       set_property(TARGET ${target} PROPERTY PUBLIC_HEADER "")
#     endif()
#   endforeach()
#   unset(target)
#   unset(target_header)
# endif()

set(RES_DIR "${CMAKE_CURRENT_LIST_DIR}/web")
set(RES_ZIP "${CMAKE_CURRENT_BINARY_DIR}/res.zip")

file(GLOB_RECURSE RES_DEP
    LIST_DIRECTORIES false
    "${RES_DIR}/*")
add_custom_command(
    OUTPUT ${RES_ZIP}
    COMMAND ${CMAKE_COMMAND} -E tar "cfv" ${RES_ZIP} --format=zip ${RES_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    DEPENDS ${RES_DEP}
    VERBATIM
    )
target_sources(ScraperServer
  PRIVATE
    ${RES_ZIP}
  )

if (WIN32)
  target_sources(ScraperServer
  PRIVATE
    src/ScraperServer.rc
  )
else()
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
      enable_language(ASM)

      if(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
          set(_ _)
      endif()

      set(RES_ASM ${CMAKE_CURRENT_BINARY_DIR}/res.s)
      if(NOT EXISTS ${RES_ASM})
        file(WRITE ${RES_ASM} "\
    .data
    .global ${_}_binary_res_zip_start
    .global ${_}_binary_res_zip_end
${_}_binary_res_zip_start:
    .incbin \"res.zip\"
${_}_binary_res_zip_end:
${_}_binary_res_zip_size = ${_}_binary_res_zip_end - ${_}_binary_res_zip_start
")
      endif()
      target_sources(ScraperServer PRIVATE ${RES_ASM})
      target_sources(Demo PRIVATE ${RES_ASM})
  else()
    set(RES_O ${CMAKE_CURRENT_BINARY_DIR}/res.o)
    add_custom_command(
      OUTPUT ${RES_O}
      COMMAND "${CMAKE_LINKER}" -r -b binary -o res.o res.zip
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      DEPENDS ${RES_ZIP}
      VERBATIM
      )
    target_sources(ScraperServer PRIVATE ${RES_O})
    target_sources(Demo PRIVATE ${RES_O})
  endif()
endif()

# install
include(GNUInstallDirs)
install(
  TARGETS
    ScraperServer
    Demo
    Net
    JSON
    XML
    Zip
    Foundation
    mediainfo
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

set(CPACK_PACKAGE_VENDOR "backzhao@qq.com")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_GENERATOR TGZ)
set(CPACK_IGNORE_FILES
   /build
)
set(CPACK_SOURCE_GENERATOR ZIP)
set(CPACK_SOURCE_IGNORE_FILES
  /build/
  /zig-cache/
  /zig-out/
  /\\.cache/
  /\\.git
  /\\.gitattributes
  /\\.gitignore
  /\\.vs/
)
include(CPack)