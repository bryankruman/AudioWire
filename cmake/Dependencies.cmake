include(FetchContent)
function(aw_dependency name repo revision)
    if(AUDIOWIRE_VENDOR_DIR AND EXISTS "${AUDIOWIRE_VENDOR_DIR}/${name}")
        set(${name}_SOURCE_DIR "${AUDIOWIRE_VENDOR_DIR}/${name}" PARENT_SCOPE)
    else()
        FetchContent_Declare(${name} URL "https://github.com/${repo}/archive/${revision}.tar.gz"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        # FetchContent normalises names (including JUCE) to lowercase. Request
        # explicit output variables instead of assuming the caller's case.
        FetchContent_GetProperties(${name} SOURCE_DIR aw_dep_source POPULATED aw_dep_populated)
        if(NOT aw_dep_populated)
            FetchContent_Populate(${name})
            FetchContent_GetProperties(${name} SOURCE_DIR aw_dep_source)
        endif()
        set(${name}_SOURCE_DIR "${aw_dep_source}" PARENT_SCOPE)
    endif()
endfunction()
aw_dependency(opus xiph/opus ddbe48383984d56acd9e1ab6a090c54ca6b735a6)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
add_subdirectory(${opus_SOURCE_DIR} ${CMAKE_BINARY_DIR}/deps/opus EXCLUDE_FROM_ALL)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/deps/include/opus)
file(GLOB opus_headers ${opus_SOURCE_DIR}/include/*.h)
file(COPY ${opus_headers} DESTINATION ${CMAKE_BINARY_DIR}/deps/include/opus)
aw_dependency(libsodium jedisct1/libsodium 9511c982fb1d046470a8b42aa36556cdb7da15de)
set(VERSION 1.0.20)
set(SODIUM_LIBRARY_VERSION_MAJOR 26)
set(SODIUM_LIBRARY_VERSION_MINOR 2)
set(SODIUM_LIBRARY_MINIMAL_DEF "")
configure_file(${libsodium_SOURCE_DIR}/src/libsodium/include/sodium/version.h.in
    ${CMAKE_BINARY_DIR}/deps/include/sodium/version.h @ONLY)
file(GLOB_RECURSE sodium_sources CONFIGURE_DEPENDS ${libsodium_SOURCE_DIR}/src/libsodium/*.c)
add_library(sodium STATIC ${sodium_sources})
target_include_directories(sodium PUBLIC ${CMAKE_BINARY_DIR}/deps/include
    ${libsodium_SOURCE_DIR}/src/libsodium/include ${libsodium_SOURCE_DIR}/src/libsodium/include/sodium PRIVATE ${CMAKE_BINARY_DIR}/deps/include/sodium)
target_compile_definitions(sodium PUBLIC SODIUM_STATIC PRIVATE CONFIGURED=1)
if(WIN32)
    target_link_libraries(sodium PUBLIC advapi32)
else()
    target_compile_definitions(sodium PRIVATE HAVE_PTHREAD=1 HAVE_MMAP=1 HAVE_SYS_MMAN_H=1
        HAVE_POSIX_MEMALIGN=1 HAVE_GETPID=1 HAVE_UNISTD_H=1)
    find_package(Threads REQUIRED)
    target_link_libraries(sodium PUBLIC Threads::Threads)
endif()
aw_dependency(aoo essej/aoo a16c3531d119370be2de21998458e0d17c4d346f)
set(aoo_src ${aoo_SOURCE_DIR}/lib/src)
add_library(aw_aoo STATIC
    ${aoo_src}/codec_opus.cpp ${aoo_src}/codec_pcm.cpp ${aoo_src}/common.cpp
    ${aoo_src}/sink.cpp ${aoo_src}/source.cpp ${aoo_src}/sync.cpp ${aoo_src}/time.cpp
    ${aoo_SOURCE_DIR}/deps/md5/md5.c
    ${aoo_SOURCE_DIR}/deps/oscpack/osc/OscOutboundPacketStream.cpp
    ${aoo_SOURCE_DIR}/deps/oscpack/osc/OscReceivedElements.cpp
    ${aoo_SOURCE_DIR}/deps/oscpack/osc/OscTypes.cpp)
target_include_directories(aw_aoo PUBLIC ${aoo_SOURCE_DIR}/lib PRIVATE ${aoo_SOURCE_DIR}/deps)
target_compile_definitions(aw_aoo PUBLIC AOO_STATIC USE_CODEC_OPUS=1 PRIVATE AOO_TIMEFILTER_CHECK=0 LOGLEVEL=0)
target_link_libraries(aw_aoo PUBLIC opus)
target_include_directories(aw_aoo PUBLIC ${CMAKE_BINARY_DIR}/deps/include)
if(WIN32)
    target_link_libraries(aw_aoo PUBLIC ws2_32)
endif()


