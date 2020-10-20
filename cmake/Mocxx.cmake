include(FetchContent)

FetchContent_Declare(Mocxx
  GIT_REPOSITORY ssh:clone git@github.com:Guardsquare/mocxx.git
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})

FetchContent_MakeAvailable(Mocxx)

add_library(Mocxx INTERFACE)
target_compile_definitions(Mocxx INTERFACE ${Mocxx_DEFINITIONS})
target_include_directories(Mocxx INTERFACE ${Mocxx_SOURCE_DIR}/include)
target_compile_options(Mocxx INTERFACE -O0 -g -fno-inline-functions -fno-inline)

# Frida Gum
target_include_directories(Mocxx INTERFACE ${FRIDA_INCLUDES})
target_link_libraries(Mocxx INTERFACE ${FRIDA_LIBRARIES})
target_link_options(Mocxx INTERFACE ${FRIDA_LINKER_FLAGS})
