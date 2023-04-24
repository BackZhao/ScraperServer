cmake_minimum_required(VERSION 3.16)

project(
    ScraperServer
    VERSION 1.0
    LANGUAGES CXX
)

include(FetchContent)

# poco
find_package(Poco QUIET COMPONENTS Net Util JSON CONFIG)
if(NOT TARGET Poco::Net)
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
    ENABLE_XML
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
    GIT_REPOSITORY "https://github.com/pocoproject/poco"
    GIT_TAG poco-1.12.4-release
    EXCLUDE_FROM_ALL
    )
endif()

add_executable(ScraperServer src/main.cpp)