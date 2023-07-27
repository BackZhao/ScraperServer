cmake_minimum_required(VERSION 3.16)

project(
    ScraperServer
    VERSION 1.0
    LANGUAGES CXX C
)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake/modules)

set(CMAKE_CXX_STANDARD 11)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# fetch content
include(FetchContent)

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
    FetchContent_Declare(
      getopt
      GIT_REPOSITORY "https://gitee.com/BackZhao97/getopt.git"
      GIT_SHALLOW ON
      GIT_PROGRESS ON
      )
    FetchContent_GetProperties(getopt)
    if(NOT getopt_POPULATED)
      message(STATUS "Configuring getopt")
      FetchContent_Populate(getopt)
      add_subdirectory(
        ${getopt_SOURCE_DIR} ${getopt_BINARY_DIR} EXCLUDE_FROM_ALL
        )
    endif()
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
    ## apache
    ENABLE_APACHECONNECTOR
    ## encoding
    ENABLE_ENCODINGS
    ## zip
    ENABLE_ZIP
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
    if(NOT DEFINED CACHE[${prop}])
      set(${prop}
        OFF
        CACHE BOOL ""
        )
    endif()
    set(${prop} OFF)
  endforeach()

  FetchContent_Declare(
    poco
    GIT_REPOSITORY "https://gitee.com/BackZhao97/poco.git"
    GIT_TAG poco-1.12.4-release
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    )
  FetchContent_GetProperties(poco)
  if(NOT poco_POPULATED)
    FetchContent_Populate(poco)
    add_subdirectory(${poco_SOURCE_DIR} ${poco_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif ()
endif()

# spdlog
find_package(spdlog QUIET)
if(NOT TARGET spdlog::spdlog_header_only)
  FetchContent_Declare(
    spdlog
    GIT_REPOSITORY "https://gitee.com/BackZhao97/spdlog.git"
    GIT_TAG v1.x
    EXCLUDE_FROM_ALL
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    )
  FetchContent_GetProperties(spdlog)
  if(NOT spdlog_POPULATED)
    FetchContent_Populate(spdlog)
    add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
  endif ()
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
  FetchContent_Declare(
    mediainfo
    GIT_REPOSITORY "https://gitee.com/BackZhao97/MediaInfoLib"
    GIT_TAG cmake
    EXCLUDE_FROM_ALL
    GIT_SHALLOW ON
    GIT_PROGRESS ON
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
  src/Tmdb.cpp
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
    mediainfo
  )

add_executable(ScraperServer
  src/main.cpp
  src/HttpRequestHandler.cpp
  src/HttpServer.cpp
  src/ApiManager.cpp
  src/DataConvert.cpp
  src/DataSource.cpp
  src/Tmdb.cpp
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

# install
include(GNUInstallDirs)
install(
  TARGETS
    ScraperServer
    Demo
    Net
    JSON
    XML
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