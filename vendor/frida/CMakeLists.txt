if(NOT WIN32)
  set(FRIDA_LINKER_FLAGS
    -lresolv -lpthread -ldl
    CACHE STRING "Frida gum flags")
endif()

if (APPLE)
  set(FRIDA_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/macos/include
    CACHE STRING "Frida gum includes")
  set(FRIDA_LIBRARIES
    "${CMAKE_CURRENT_SOURCE_DIR}/macos/lib/libfrida-gum.a"
    CACHE STRING "Frida gum library")

elseif(WIN32)
  set(ARCHITECTURE_PATH x86 )
  if(CMAKE_CL_64)
     set(ARCHITECTURE_PATH x64 )
  endif()

  set(FRIDA_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/msvc/include/${ARCHITECTURE_PATH}
    CACHE STRING "Frida gum includes")
  set(FRIDA_LIBRARIES
    "${CMAKE_CURRENT_SOURCE_DIR}/msvc/${ARCHITECTURE_PATH}/frida-gum.lib"
    CACHE STRING "Frida gum library")
else()
  set(FRIDA_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/include
    CACHE STRING "Frida gum includes")
  set(FRIDA_LIBRARIES
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/lib/libfrida-gum.a"
    CACHE STRING "Frida gum library")
endif()
