cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  project(${_name} VERSION ${_version} LANGUAGES C CXX OBJCXX)
else()
  project(${_name} VERSION ${_version})
endif()

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT WrapOpenGL_AGL)
    # obs-deps' prebuilt Qt6 (built 2025-07-11) unconditionally links "-framework AGL" via its
    # FindWrapOpenGL.cmake (see .deps/obs-deps-qt6-*/lib/cmake/Qt6/FindWrapOpenGL.cmake, lines
    # 40-49 - there is no way to opt out from our side, it always adds *some* AGL reference on
    # Apple). Recent macOS SDKs (Xcode 26+) ship an AGL.framework stub with a broken symlink and
    # no actual binary (Apple has fully removed the legacy Carbon-era library, just left an empty
    # bundle for compatibility checks), so the real "-framework AGL" fails to link. Nothing in Qt6
    # or libobs actually calls into AGL on modern macOS - it's a vestigial link requirement - so we
    # build a throwaway empty stub .framework here and point WrapOpenGL_AGL at it. Combined with
    # -dead_strip_dylibs below, the final binary ends up with zero runtime dependency on it.
    set(_agl_stub_dir "${CMAKE_BINARY_DIR}/agl-stub/AGL.framework")
    if(NOT EXISTS "${_agl_stub_dir}/AGL")
      file(MAKE_DIRECTORY "${_agl_stub_dir}")
      execute_process(
        COMMAND xcrun --sdk macosx clang -dynamiclib -x c /dev/null -o "${_agl_stub_dir}/AGL"
        RESULT_VARIABLE _agl_stub_result
      )
      if(NOT _agl_stub_result EQUAL 0)
        message(FATAL_ERROR "Failed to build the AGL.framework link stub (see comment above)")
      endif()
    endif()
    set(WrapOpenGL_AGL "${_agl_stub_dir}" CACHE FILEPATH
        "Stub AGL.framework - the real one no longer ships a linkable binary on this SDK" FORCE
    )
  endif()

  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,-dead_strip_dylibs")
  endif()
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.cpp
          src/input/GameControllerBridge.h
          src/input/GameControllerBridge.mm
          src/control/PtzTypes.h
          src/control/IViscaSink.h
          src/control/PtzController.h
          src/control/PtzController.cpp
          src/visca/ViscaConstants.h
          src/visca/ViscaTransport.h
          src/visca/UdpViscaTransport.h
          src/visca/UdpViscaTransport.cpp
          src/visca/ViscaCamera.h
          src/visca/ViscaCamera.cpp
          src/ui/PtzDockWidget.h
          src/ui/PtzDockWidget.cpp
          src/ui/PtzSettingsDialog.h
          src/ui/PtzSettingsDialog.cpp
          src/settings/PtzSettings.h
          src/settings/PtzSettings.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set_source_files_properties(src/input/GameControllerBridge.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "-framework GameController" "-framework Foundation")
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
