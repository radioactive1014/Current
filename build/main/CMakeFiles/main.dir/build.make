# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/intelligentrobotics/rokon/src

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/intelligentrobotics/rokon/build

# Include any dependencies generated for this target.
include main/CMakeFiles/main.dir/depend.make

# Include the progress variables for this target.
include main/CMakeFiles/main.dir/progress.make

# Include the compile flags for this target's objects.
include main/CMakeFiles/main.dir/flags.make

main/CMakeFiles/main.dir/src/ControlPBP.cpp.o: main/CMakeFiles/main.dir/flags.make
main/CMakeFiles/main.dir/src/ControlPBP.cpp.o: /home/intelligentrobotics/rokon/src/main/src/ControlPBP.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/intelligentrobotics/rokon/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object main/CMakeFiles/main.dir/src/ControlPBP.cpp.o"
	cd /home/intelligentrobotics/rokon/build/main && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/main.dir/src/ControlPBP.cpp.o -c /home/intelligentrobotics/rokon/src/main/src/ControlPBP.cpp

main/CMakeFiles/main.dir/src/ControlPBP.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/main.dir/src/ControlPBP.cpp.i"
	cd /home/intelligentrobotics/rokon/build/main && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/intelligentrobotics/rokon/src/main/src/ControlPBP.cpp > CMakeFiles/main.dir/src/ControlPBP.cpp.i

main/CMakeFiles/main.dir/src/ControlPBP.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/main.dir/src/ControlPBP.cpp.s"
	cd /home/intelligentrobotics/rokon/build/main && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/intelligentrobotics/rokon/src/main/src/ControlPBP.cpp -o CMakeFiles/main.dir/src/ControlPBP.cpp.s

main/CMakeFiles/main.dir/src/ControlPBP.cpp.o.requires:
.PHONY : main/CMakeFiles/main.dir/src/ControlPBP.cpp.o.requires

main/CMakeFiles/main.dir/src/ControlPBP.cpp.o.provides: main/CMakeFiles/main.dir/src/ControlPBP.cpp.o.requires
	$(MAKE) -f main/CMakeFiles/main.dir/build.make main/CMakeFiles/main.dir/src/ControlPBP.cpp.o.provides.build
.PHONY : main/CMakeFiles/main.dir/src/ControlPBP.cpp.o.provides

main/CMakeFiles/main.dir/src/ControlPBP.cpp.o.provides.build: main/CMakeFiles/main.dir/src/ControlPBP.cpp.o

main/CMakeFiles/main.dir/src/UnityOde.cpp.o: main/CMakeFiles/main.dir/flags.make
main/CMakeFiles/main.dir/src/UnityOde.cpp.o: /home/intelligentrobotics/rokon/src/main/src/UnityOde.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /home/intelligentrobotics/rokon/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object main/CMakeFiles/main.dir/src/UnityOde.cpp.o"
	cd /home/intelligentrobotics/rokon/build/main && /usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/main.dir/src/UnityOde.cpp.o -c /home/intelligentrobotics/rokon/src/main/src/UnityOde.cpp

main/CMakeFiles/main.dir/src/UnityOde.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/main.dir/src/UnityOde.cpp.i"
	cd /home/intelligentrobotics/rokon/build/main && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/intelligentrobotics/rokon/src/main/src/UnityOde.cpp > CMakeFiles/main.dir/src/UnityOde.cpp.i

main/CMakeFiles/main.dir/src/UnityOde.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/main.dir/src/UnityOde.cpp.s"
	cd /home/intelligentrobotics/rokon/build/main && /usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/intelligentrobotics/rokon/src/main/src/UnityOde.cpp -o CMakeFiles/main.dir/src/UnityOde.cpp.s

main/CMakeFiles/main.dir/src/UnityOde.cpp.o.requires:
.PHONY : main/CMakeFiles/main.dir/src/UnityOde.cpp.o.requires

main/CMakeFiles/main.dir/src/UnityOde.cpp.o.provides: main/CMakeFiles/main.dir/src/UnityOde.cpp.o.requires
	$(MAKE) -f main/CMakeFiles/main.dir/build.make main/CMakeFiles/main.dir/src/UnityOde.cpp.o.provides.build
.PHONY : main/CMakeFiles/main.dir/src/UnityOde.cpp.o.provides

main/CMakeFiles/main.dir/src/UnityOde.cpp.o.provides.build: main/CMakeFiles/main.dir/src/UnityOde.cpp.o

# Object files for target main
main_OBJECTS = \
"CMakeFiles/main.dir/src/ControlPBP.cpp.o" \
"CMakeFiles/main.dir/src/UnityOde.cpp.o"

# External object files for target main
main_EXTERNAL_OBJECTS =

/home/intelligentrobotics/rokon/devel/lib/libmain.so: main/CMakeFiles/main.dir/src/ControlPBP.cpp.o
/home/intelligentrobotics/rokon/devel/lib/libmain.so: main/CMakeFiles/main.dir/src/UnityOde.cpp.o
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/libroscpp.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_signals-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_filesystem-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/librosconsole.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_regex-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/liblog4cxx.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/libroscpp_serialization.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/librostime.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_date_time-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_system-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_thread-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/x86_64-linux-gnu/libpthread.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/libxmlrpcpp.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/libcpp_common.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /home/intelligentrobotics/rokon/devel/lib/libutils.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/libroscpp.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_signals-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_filesystem-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/librosconsole.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_regex-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/liblog4cxx.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/libroscpp_serialization.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/librostime.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_date_time-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_system-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/libboost_thread-mt.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /usr/lib/x86_64-linux-gnu/libpthread.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/libxmlrpcpp.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: /opt/ros/groovy/lib/libcpp_common.so
/home/intelligentrobotics/rokon/devel/lib/libmain.so: main/CMakeFiles/main.dir/build.make
/home/intelligentrobotics/rokon/devel/lib/libmain.so: main/CMakeFiles/main.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX shared library /home/intelligentrobotics/rokon/devel/lib/libmain.so"
	cd /home/intelligentrobotics/rokon/build/main && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/main.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
main/CMakeFiles/main.dir/build: /home/intelligentrobotics/rokon/devel/lib/libmain.so
.PHONY : main/CMakeFiles/main.dir/build

main/CMakeFiles/main.dir/requires: main/CMakeFiles/main.dir/src/ControlPBP.cpp.o.requires
main/CMakeFiles/main.dir/requires: main/CMakeFiles/main.dir/src/UnityOde.cpp.o.requires
.PHONY : main/CMakeFiles/main.dir/requires

main/CMakeFiles/main.dir/clean:
	cd /home/intelligentrobotics/rokon/build/main && $(CMAKE_COMMAND) -P CMakeFiles/main.dir/cmake_clean.cmake
.PHONY : main/CMakeFiles/main.dir/clean

main/CMakeFiles/main.dir/depend:
	cd /home/intelligentrobotics/rokon/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/intelligentrobotics/rokon/src /home/intelligentrobotics/rokon/src/main /home/intelligentrobotics/rokon/build /home/intelligentrobotics/rokon/build/main /home/intelligentrobotics/rokon/build/main/CMakeFiles/main.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : main/CMakeFiles/main.dir/depend

