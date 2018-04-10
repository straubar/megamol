message(WARNING "setting gcc compiler flags")

set(COMMON_CXX_FLAGS "-Wall -pedantic -fPIC -DUNIX -D_GNU_SOURCE -D_LIN${BITS}")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${COMMON_CXX_FLAGS} -DDEBUG -D_DEBUG -g -ggdb -ldl")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${COMMON_CXX_FLAGS} -DNDEBUG -D_NDEBUG -O3 -g0 -ldl")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${COMMON_CXX_FLAGS} -DNDEBUG -D_NDEBUG -O3 -g -ldl")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fPIC -DUNIX -pedantic -std=c99 -ldl")

#if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.7)
#    message(STATUS "Version < 4.7")
#    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++0x")
#    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++0x")
#    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -std=c++0x")
#else()
#    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11")
#    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")
#    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")
#endif()
