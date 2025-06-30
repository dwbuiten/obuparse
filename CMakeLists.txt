cmake_minimum_required(VERSION 3.15)

project(OBUParse VERSION 1.0.0 LANGUAGES C)

# --- Set Standard Output Directories ---
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Build Options ---
# On MSVC, we only build static to avoid DLL export complexity and naming collisions.
# On other platforms (including MinGW on Windows), we can build both.
if(MSVC)
    option(BUILD_SHARED_LIBS "Build the shared library (disabled on MSVC)" OFF)
    option(BUILD_STATIC_LIBS "Build the static library" ON)
    set(BUILD_SHARED_LIBS OFF) # Force OFF for MSVC
else()
    option(BUILD_SHARED_LIBS "Build the shared library" ON)
    option(BUILD_STATIC_LIBS "Build the static library" ON)
endif()

if(NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
    message(FATAL_ERROR "You must choose to build at least one library type.")
endif()

# --- Library Targets ---

if(BUILD_SHARED_LIBS) # This block will now be skipped on MSVC
    add_library(obuparse_shared SHARED obuparse.c)
    set_target_properties(obuparse_shared PROPERTIES
        OUTPUT_NAME "obuparse"
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        EXPORT_NAME "shared"
    )
    target_include_directories(obuparse_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    add_library(OBUParse::shared ALIAS obuparse_shared)
endif()

if(BUILD_STATIC_LIBS)
    add_library(obuparse_static STATIC obuparse.c)
    # The naming collision only happens with MSVC, not MinGW.
    if(MSVC)
        set_target_properties(obuparse_static PROPERTIES OUTPUT_NAME "obuparses")
    else()
        set_target_properties(obuparse_static PROPERTIES OUTPUT_NAME "obuparse")
    endif()
    set_target_properties(obuparse_static PROPERTIES EXPORT_NAME "static")
    target_include_directories(obuparse_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    add_library(OBUParse::static ALIAS obuparse_static)
endif()

# --- Tool Target (obudump) ---
add_executable(obudump
    tools/obudump.c
    tools/json.c
)
target_include_directories(obudump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(TARGET OBUParse::shared)
    target_link_libraries(obudump PRIVATE OBUParse::shared)
else()
    target_link_libraries(obudump PRIVATE OBUParse::static)
endif()

# --- RPATH Handling for obudump tool ---
if(UNIX AND TARGET obudump AND TARGET OBUParse::shared)
    # This handles the build tree. The executable is in build/bin, the library is in build/lib.
    # The RPATH needs to point to the library directory.
    set_property(TARGET obudump PROPERTY
        BUILD_RPATH "${CMAKE_BINARY_DIR}/lib"
    )

    # This handles the install tree. The path is relative from the installed bin/ directory
    # to the lib/ directory.
    set_property(TARGET obudump PROPERTY
        INSTALL_RPATH "\$ORIGIN/../lib"
    )
endif()

# --- Installation ---
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install the public header file
install(FILES obuparse.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

set(BUILT_TARGETS "")
if(TARGET obuparse_static)
    list(APPEND BUILT_TARGETS obuparse_static)
endif()
if(TARGET obuparse_shared)
    list(APPEND BUILT_TARGETS obuparse_shared)
endif()

# Install the library targets that were built, and the tool
install(TARGETS ${BUILT_TARGETS} obudump
    EXPORT OBUParseTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

# Generate and install the obuparse-config.cmake file
# This file tells other projects where to find the targets we just installed.
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/obuparse-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "obuparse-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/obuparse-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/obuparse"
)

# Install the generated CMake package files
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/obuparse-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/obuparse-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/obuparse"
)

# Install the export set, which contains the actual target information
install(
    EXPORT OBUParseTargets
    FILE OBUParseTargets.cmake
    NAMESPACE OBUParse::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/obuparse"
)

# --- Uninstall Target ---
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
  )

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    COMMENT "Uninstalling the project..."
  )
endif()
