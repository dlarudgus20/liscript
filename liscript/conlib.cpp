#include <Windows.h>
#include "conlib.h"

namespace conlib
{
	unsigned setcolor(color fg, color bg /* = color::black */)
	{
		return setcolor((int)bg << 4 | (int)fg);
	}

	unsigned setcolor(unsigned colorbit)
	{
		unsigned old = getcolor();
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)colorbit);
		return old;
	}

	unsigned getcolor()
	{
		CONSOLE_SCREEN_BUFFER_INFO sbi;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &sbi);
		return sbi.wAttributes;
	}
}
