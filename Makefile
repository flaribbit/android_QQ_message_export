main.exe: main.c sqlite3.dll
	gcc main.c sqlite3.dll -s -O2 -DNDEBUG -m32 -o main.exe