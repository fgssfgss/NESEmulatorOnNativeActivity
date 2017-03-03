cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3")
set(SOURCE_FILES src/main/cpp/src/Emulator.cpp src/main/cpp/include/Emulator.h src/main/cpp/src/Memory.cpp src/main/cpp/include/Memory.h src/main/cpp/include/Common.h src/main/cpp/src/CPU.cpp src/main/cpp/include/CPU.h src/main/cpp/src/ROM.cpp src/main/cpp/include/ROM.h src/main/cpp/src/PPU.cpp src/main/cpp/include/PPU.h src/main/cpp/src/Controller.cpp src/main/cpp/include/Controller.h src/main/cpp/include/IMapper.h src/main/cpp/src/Mapper000.cpp src/main/cpp/include/Mapper000.h src/main/cpp/src/Mapper004.cpp src/main/cpp/include/Mapper004.h src/main/cpp/src/Console.cpp src/main/cpp/include/Console.h)

add_library(app-glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

add_library(emuactivity
            SHARED
            ${SOURCE_FILES}
            src/main/cpp/native-activity.cpp)

find_library(log-lib log)
find_library(android-lib android)
target_link_libraries(emuactivity ${log-lib} ${android-lib} app-glue EGL GLESv1_CM GLESv2)
