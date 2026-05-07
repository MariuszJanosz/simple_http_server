#!/bin/sh

if ! [ -z $2 ]; then
	echo "Usage: ./build.sh [<build_type>]"
	echo "<build_type> = \"Debug\" | \"Release\""
	exit 1
elif [ -z $1 ] || [ $1 = "Debug" ]; then
	echo "Building Debug:"
	cmake -B build -DCMAKE_BUILD_TYPE=Debug;
elif [  $1 = "Release" ]; then
	echo "Building Release:"
	cmake -B build -DCMAKE_BUILD_TYPE=Release;
else
	echo "Usage: ./build.sh [<build_type>]"
	echo "<build_type> = \"Debug\" | \"Release\""
	exit 1
fi

cmake --build build

