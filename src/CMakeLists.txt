set(_ble_src ble.c)
set_source_files_properties(${_ble_src} PROPERTIES LANGUAGE CXX)

add_executable(ble ${_ble_src})

target_link_libraries(ble PRIVATE simpleble::simpleble)

target_compile_features(ble PRIVATE cxx_std_17)

set_target_properties(ble PROPERTIES OUTPUT_NAME "ble")

# White noise CLI
add_executable(noise noise.c)
target_link_libraries(noise PRIVATE miniaudio httplib)
set_target_properties(noise PROPERTIES OUTPUT_NAME "noise")
set_property(TARGET noise PROPERTY C_STANDARD 11)
set_property(TARGET noise PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET noise PROPERTY C_EXTENSIONS OFF)

if(APPLE)
	# Some environments may require explicit framework links for CoreAudio.
	target_link_libraries(noise PRIVATE "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreFoundation")
endif()

# Web server (Single Page App using htmx + Pico CSS)
add_executable(web web_server.cpp)
target_link_libraries(web PRIVATE httplib cjson miniaudio simpleble::simpleble cjson_headers)
target_include_directories(web PRIVATE $<TARGET_PROPERTY:cjson,INCLUDE_DIRECTORIES>)
target_compile_features(web PRIVATE cxx_std_17)
set_target_properties(web PROPERTIES OUTPUT_NAME "web")

if(APPLE)
	target_link_libraries(web PRIVATE "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreFoundation")
endif()

# Copy static_html on each build and place web binary next to it.
add_custom_target(copy_static_html ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${CMAKE_SOURCE_DIR}/static_html"
			"${CMAKE_BINARY_DIR}/static_html"
)
add_dependencies(web copy_static_html)
set_target_properties(web PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
