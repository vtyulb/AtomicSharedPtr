cmake_minimum_required(VERSION 3.8)

project(AtomicSharedPtr LANGUAGES CXX)

cmake_minimum_required(VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(AtomicSharedPtr
    src/main.cpp
    src/lfqueue.h
    src/lfstack.h
    src/lfmap.h
    src/fast_logger.h
    src/atomic_shared_ptr.h
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(AtomicSharedPtr Threads::Threads)

option(ENABLE_FAST_LOGGING "Enables debug traces with FastLogger" ON)
if(ENABLE_FAST_LOGGING)
    target_compile_definitions(AtomicSharedPtr PRIVATE "FAST_LOGGING_ENABLED=1")
endif()

#set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fno-omit-frame-pointer -fsanitize=memory")
#set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=thread")
