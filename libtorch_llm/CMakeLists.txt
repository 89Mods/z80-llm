cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(llm-app)

find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

add_executable(llm-app llm-app.cpp)
target_link_libraries(llm-app "${TORCH_LIBRARIES}")
set_property(TARGET llm-app PROPERTY CXX_STANDARD 17)
