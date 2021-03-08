project(TagGroupVersionBumper)
cmake_minimum_required(VERSION 3.5)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

#find_package(RvLEdgeSDKUtils REQUIRED CONFIG)

#if (RvLEdgeSDKUtils_FOUND)
#	include_directories(${RvLEdgeSDKUtils_INCLUDE_DIRS})
#endif()

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost COMPONENTS filesystem)

add_executable(TagGroupVersionBumper
    TagGroupVersionBumper.cpp
)

target_link_libraries(TagGroupVersionBumper
 #   RvLEdgeSDKUtils::RvLEdgeSDKUtils
    Boost::filesystem
)

if(DEFINED ENV{RVL_EDGESDK_COPY_DIR})
	message("Executable to be copied to $ENV{RVL_EDGESDK_COPY_DIR}")
add_custom_command(TARGET TagGroupVersionBumper 
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:TagGroupVersionBumper> $ENV{RVL_EDGESDK_COPY_DIR}
	COMMENT copying $<TARGET_FILE:TagGroupVersionBumper>
	)
else()
	message("Not copying executable; RVL_EDGESDK_COPY_DIR not set")
endif()

set_property(TARGET TagGroupVersionBumper PROPERTY CXX_STANDARD 11)
set_property(TARGET TagGroupVersionBumper PROPERTY OUTPUT_NAME "TagGroupVersionBumper")

if (Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
endif()
