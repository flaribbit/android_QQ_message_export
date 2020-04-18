main.exe: main.cpp key.h sqlite3.dll
	g++ main.cpp sqlite3.dll -s -O2 -DNDEBUG --std=c++11 -m32 -o main.exe
