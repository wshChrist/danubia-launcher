# Plugin BakkesMod « Danubia » (Windows x64, MSVC). SDK : https://github.com/bakkesmodorg/BakkesModSDK
#   cmake -B build -A x64 -DBAKKESMOD_SDK=<chemin du SDK> && cmake --build build --config Release
cmake_minimum_required(VERSION 3.20)
project(DanubiaRL CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BAKKESMOD_SDK "" CACHE PATH "Dossier du BakkesModSDK (include/, lib/)")
add_library(danubiarl SHARED src/DanubiaRL.cpp)
target_include_directories(danubiarl PRIVATE ${BAKKESMOD_SDK}/include)
target_link_libraries(danubiarl PRIVATE ${BAKKESMOD_SDK}/lib/pluginsdk.lib)
target_compile_options(danubiarl PRIVATE /utf-8 /EHsc)
