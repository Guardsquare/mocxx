add_compile_options(-O0 -g -fno-inline-functions -fno-inline)

file(GLOB_RECURSE TEST_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

include_directories(${CATCH_INCLUDES})

include_directories(${FRIDA_INCLUDES})
add_link_options(${FRIDA_LINKER_FLAGS})

add_executable(test_mocxx ${TEST_FILES})

target_link_libraries(test_mocxx ${FRIDA_LIBRARIES})

add_test(NAME test_mocxx COMMAND test_mocxx)
