set(FRIDA_LINKER_FLAGS
  "-lresolv"
  CACHE STRING "Frida gum flags")

if (APPLE)
  set(FRIDA_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/macos/include
    CACHE STRING "Frida gum includes")
  set(FRIDA_LIBRARIES
    "${CMAKE_CURRENT_SOURCE_DIR}/macos/lib/libfrida-gum.a"
    CACHE STRING "Frida gum library")

else()
  set(FRIDA_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/include
    CACHE STRING "Frida gum includes")
  set(FRIDA_LIBRARIES
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/lib/libfrida-gum.a"
    CACHE STRING "Frida gum library")
endif()
