#pragma once

namespace conlib
{
	enum color
	{
		brighten = 8,

		black = 0,
		darkblue = 1,
		darkgreen = 2,
		darkred = 4,
		darkcyan = darkblue | darkgreen,
		darkmagenta = darkblue | darkred,
		darkyellow = darkgreen | darkred,
		darkgray = brighten,
		gray = darkblue | darkgreen | darkyellow,
		blue = brighten | darkblue,
		green = brighten | darkgreen,
		red = brighten | darkred,
		cyan = brighten | darkcyan,
		magenta = brighten | darkmagenta,
		yellow = brighten | darkyellow,
		white = brighten | darkgray,
	};
	unsigned setcolor(color fg, color bg = color::black);
	unsigned setcolor(unsigned colorbit);
	unsigned getcolor();

	class setcolor_block
	{
	public:
		setcolor_block()
		{
			m_oldcolor = getcolor();
		}
		explicit setcolor_block(color fg, color bg = color::black)
		{
			m_oldcolor = setcolor(fg, bg);
		}
		explicit setcolor_block(unsigned colorbit)
		{
			m_oldcolor = setcolor(colorbit);
		}
		void restore() const
		{
			setcolor(m_oldcolor);
		}
		~setcolor_block()
		{
			setcolor(m_oldcolor);
		}
	private:
		unsigned m_oldcolor;
	};
}