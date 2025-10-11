rem Run this script to create a build directory and generate Makefiles using CMake
rem Run this script from the root of the project directory(ex where CMakeLists.txt is located)

mkdir build
cd build
cmake -G "MinGW Makefiles" ..
