cmake_minimum_required(VERSION 3.6)
project(WasmTest)
set(CMAKE_CXX_STANDARD  20)


if(true)
    set(CMAKE_CXX_FLAGS "-s ASYNCIFY=1 -s USE_SDL=2 -s USE_SDL_TTF=2 -O2 --preload-file ../Assets/@Assets/")

    include_directories(D:/SDKs/emsdk/upstream/emscripten/cache/sysroot/include ${SDL2_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})

    add_executable(RogueLikeGame wasm.cpp)
    set_target_properties(RogueLikeGame PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
else()
    add_executable(packer packer.cpp)
    set_target_properties(packer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()
