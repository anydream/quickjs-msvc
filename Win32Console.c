#include "Win32Console.h"
#include "quickjs-defs.h"

//////////////////////////////////////////////////////////////////////////
#if defined(PLATFORM_IS_WINDOWS)
#  include <Windows.h>

static int utf16to8(const wchar_t *wstr, uint32_t szWstr, char *ustr, uint32_t szUstr)
{
	int szRealUstr = WideCharToMultiByte(
		CP_UTF8, 0,
		wstr, szWstr,
		ustr, szUstr,
		NULL, NULL);

	return szRealUstr;
}

static int utf8to16(const char *ustr, uint32_t szUstr, wchar_t *wstr, uint32_t szWstr)
{
	int szRealWstr = MultiByteToWideChar(
		CP_UTF8, 0,
		ustr, szUstr,
		wstr, szWstr);

	return szRealWstr;
}

/* Find the beginning of an escape sequence.
There are two cases:
1. If the sequence begins with an ESC character (0x1B) and a second
character X in [0x40,0x5F], returns X and stores a pointer to
the third character into *head.
2. If the sequence begins with a character X in [0x80,0x9F], returns
(X-0x40) and stores a pointer to the second character into *head.
Stores the number of ESC character(s) in *prefix_len.
Returns 0 if no such sequence can be found.  */
static int find_esc_head(int *prefix_len, const char **head, const char *str)
{
	int c;
	const char *r = str;
	int escaped = 0;

	for (;;)
	{
		c = (unsigned char)*r;
		if (c == 0)
		{
			/* Not found.  */
			return 0;
		}
		if (escaped && 0x40 <= c && c <= 0x5F)
		{
			/* Found (case 1).  */
			*prefix_len = 2;
			*head = r + 1;
			return c;
		}

		++r;
		escaped = c == 0x1B;
	}
}

/* Find the terminator of an escape sequence.
str should be the value stored in *head by a previous successful
call to find_esc_head().
Returns 0 if no such sequence can be found.  */
static int find_esc_terminator(const char **term, const char *str)
{
	int c;
	const char *r = str;

	for (;;)
	{
		c = (unsigned char)*r;
		if (c == 0)
		{
			/* Not found.  */
			return 0;
		}
		if (0x40 <= c && c <= 0x7E)
		{
			/* Found.  */
			*term = r;
			return c;
		}
		++r;
	}
}

/* Handle a sequence of codes.  Sequences that are invalid, reserved,
unrecognized or unimplemented are ignored silently.
There isn't much we can do because of lameness of Windows consoles.  */
static void
eat_esc_sequence(HANDLE h, int esc_code,
                 const char *esc_head, const char *esc_term)
{
	/* Numbers in an escape sequence cannot be negative, because
	a minus sign in the middle of it would have terminated it.  */
	long n1, n2;
	char *eptr, *delim;
	CONSOLE_SCREEN_BUFFER_INFO sb;
	COORD cr;
	/* ED and EL parameters.  */
	DWORD cnt, step;
	long rows;
	/* SGR parameters.  */
	WORD attrib_add, attrib_rm;
	const char *param;

	switch (MAKEWORD(esc_code, *esc_term))
	{
		/* ESC [ n1 'A'
		Move the cursor up by n1 characters.  */
	case MAKEWORD('[', 'A'):
		if (esc_head == esc_term)
			n1 = 1;
		else
		{
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			cr = sb.dwCursorPosition;
			/* Stop at the topmost boundary.  */
			if (cr.Y > n1)
				cr.Y -= (SHORT)n1;
			else
				cr.Y = 0;
			SetConsoleCursorPosition(h, cr);
		}
		break;

		/* ESC [ n1 'B'
		Move the cursor down by n1 characters.  */
	case MAKEWORD('[', 'B'):
		if (esc_head == esc_term)
			n1 = 1;
		else
		{
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			cr = sb.dwCursorPosition;
			/* Stop at the bottommost boundary.  */
			if (sb.dwSize.Y - cr.Y > n1)
				cr.Y += (SHORT)n1;
			else
				cr.Y = sb.dwSize.Y;
			SetConsoleCursorPosition(h, cr);
		}
		break;

		/* ESC [ n1 'C'
		Move the cursor right by n1 characters.  */
	case MAKEWORD('[', 'C'):
		if (esc_head == esc_term)
			n1 = 1;
		else
		{
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			cr = sb.dwCursorPosition;
			/* Stop at the rightmost boundary.  */
			if (sb.dwSize.X - cr.X > n1)
				cr.X += (SHORT)n1;
			else
				cr.X = sb.dwSize.X;
			SetConsoleCursorPosition(h, cr);
		}
		break;

		/* ESC [ n1 'D'
		Move the cursor left by n1 characters.  */
	case MAKEWORD('[', 'D'):
		if (esc_head == esc_term)
			n1 = 1;
		else
		{
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			cr = sb.dwCursorPosition;
			/* Stop at the leftmost boundary.  */
			if (cr.X > n1)
				cr.X -= (SHORT)n1;
			else
				cr.X = 0;
			SetConsoleCursorPosition(h, cr);
		}
		break;

		/* ESC [ n1 'E'
		Move the cursor to the beginning of the n1-th line downwards.  */
	case MAKEWORD('[', 'E'):
		if (esc_head == esc_term)
			n1 = 1;
		else
		{
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			cr = sb.dwCursorPosition;
			cr.X = 0;
			/* Stop at the bottommost boundary.  */
			if (sb.dwSize.Y - cr.Y > n1)
				cr.Y += (SHORT)n1;
			else
				cr.Y = sb.dwSize.Y;
			SetConsoleCursorPosition(h, cr);
		}
		break;

		/* ESC [ n1 'F'
		Move the cursor to the beginning of the n1-th line upwards.  */
	case MAKEWORD('[', 'F'):
		if (esc_head == esc_term)
			n1 = 1;
		else
		{
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			cr = sb.dwCursorPosition;
			cr.X = 0;
			/* Stop at the topmost boundary.  */
			if (cr.Y > n1)
				cr.Y -= (SHORT)n1;
			else
				cr.Y = 0;
			SetConsoleCursorPosition(h, cr);
		}
		break;

		/* ESC [ n1 'G'
		Move the cursor to the (1-based) n1-th column.  */
	case MAKEWORD('[', 'G'):
		if (esc_head == esc_term)
			n1 = 1;
		else
		{
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			cr = sb.dwCursorPosition;
			n1 -= 1;
			/* Stop at the leftmost or rightmost boundary.  */
			if (n1 < 0)
				cr.X = 0;
			else if (n1 > sb.dwSize.X)
				cr.X = sb.dwSize.X;
			else
				cr.X = (SHORT)n1;
			SetConsoleCursorPosition(h, cr);
		}
		break;

		/* ESC [ n1 ';' n2 'H'
		ESC [ n1 ';' n2 'f'
		Move the cursor to the (1-based) n1-th row and
		(also 1-based) n2-th column.  */
	case MAKEWORD('[', 'H'):
	case MAKEWORD('[', 'f'):
		if (esc_head == esc_term)
		{
			/* Both parameters are omitted and set to 1 by default.  */
			n1 = 1;
			n2 = 1;
		}
		else if (!(delim = (char*)memchr(esc_head, ';',
		                                 esc_term - esc_head)))
		{
			/* Only the first parameter is given.  The second one is
			set to 1 by default.  */
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
			n2 = 1;
		}
		else
		{
			/* Both parameters are given.  The first one shall be
			terminated by the semicolon.  */
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != delim)
				break;
			n2 = strtol(delim + 1, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			cr = sb.dwCursorPosition;
			n1 -= 1;
			n2 -= 1;
			/* The cursor position shall be relative to the view coord of
			the console window, which is usually smaller than the actual
			buffer.  FWIW, the 'appropriate' solution will be shrinking
			the buffer to match the size of the console window,
			destroying scrollback in the process.  */
			n1 += sb.srWindow.Top;
			n2 += sb.srWindow.Left;
			/* Stop at the topmost or bottommost boundary.  */
			if (n1 < 0)
				cr.Y = 0;
			else if (n1 > sb.dwSize.Y)
				cr.Y = sb.dwSize.Y;
			else
				cr.Y = (SHORT)n1;
			/* Stop at the leftmost or rightmost boundary.  */
			if (n2 < 0)
				cr.X = 0;
			else if (n2 > sb.dwSize.X)
				cr.X = sb.dwSize.X;
			else
				cr.X = (SHORT)n2;
			SetConsoleCursorPosition(h, cr);
		}
		break;

		/* ESC [ n1 'J'
		Erase display.  */
	case MAKEWORD('[', 'J'):
		if (esc_head == esc_term)
			/* This is one of the very few codes whose parameters have
			a default value of zero.  */
			n1 = 0;
		else
		{
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			/* The cursor is not necessarily in the console window, which
			makes the behavior of this code harder to define.  */
			switch (n1)
			{
			case 0:
				/* If the cursor is in or above the window, erase from
				it to the bottom of the window; otherwise, do nothing.  */
				cr = sb.dwCursorPosition;
				cnt = sb.dwSize.X - sb.dwCursorPosition.X;
				rows = sb.srWindow.Bottom - sb.dwCursorPosition.Y;
				break;
			case 1:
				/* If the cursor is in or under the window, erase from
				it to the top of the window; otherwise, do nothing.  */
				cr.X = 0;
				cr.Y = sb.srWindow.Top;
				cnt = sb.dwCursorPosition.X + 1;
				rows = sb.dwCursorPosition.Y - sb.srWindow.Top;
				break;
			case 2:
				/* Erase the entire window.  */
				cr.X = sb.srWindow.Left;
				cr.Y = sb.srWindow.Top;
				cnt = 0;
				rows = sb.srWindow.Bottom - sb.srWindow.Top + 1;
				break;
			default:
				/* Erase the entire buffer.  */
				cr.X = 0;
				cr.Y = 0;
				cnt = 0;
				rows = sb.dwSize.Y;
				break;
			}
			if (rows < 0)
				break;
			cnt += rows * sb.dwSize.X;
			FillConsoleOutputCharacterW(h, L' ', cnt, cr, &step);
			FillConsoleOutputAttribute(h, sb.wAttributes, cnt, cr, &step);
		}
		break;

		/* ESC [ n1 'K'
		Erase line.  */
	case MAKEWORD('[', 'K'):
		if (esc_head == esc_term)
			/* This is one of the very few codes whose parameters have
			a default value of zero.  */
			n1 = 0;
		else
		{
			n1 = strtol(esc_head, &eptr, 10);
			if (eptr != esc_term)
				break;
		}

		if (GetConsoleScreenBufferInfo(h, &sb))
		{
			switch (n1)
			{
			case 0:
				/* Erase from the cursor to the end.  */
				cr = sb.dwCursorPosition;
				cnt = sb.dwSize.X - sb.dwCursorPosition.X;
				break;
			case 1:
				/* Erase from the cursor to the beginning.  */
				cr = sb.dwCursorPosition;
				cr.X = 0;
				cnt = sb.dwCursorPosition.X + 1;
				break;
			default:
				/* Erase the entire line.  */
				cr = sb.dwCursorPosition;
				cr.X = 0;
				cnt = sb.dwSize.X;
				break;
			}
			FillConsoleOutputCharacterW(h, L' ', cnt, cr, &step);
			FillConsoleOutputAttribute(h, sb.wAttributes, cnt, cr, &step);
		}
		break;

		/* ESC [ n1 ';' n2 'm'
		Set SGR parameters.  Zero or more parameters will follow.  */
	case MAKEWORD('[', 'm'):
		attrib_add = 0;
		attrib_rm = 0;
		if (esc_head == esc_term)
		{
			/* When no parameter is given, reset the console.  */
			attrib_add |= (FOREGROUND_RED | FOREGROUND_GREEN
				| FOREGROUND_BLUE);
			attrib_rm = 0xFFFF; /* Removes everything.  */
			goto sgr_set_it;
		}
		param = esc_head;
		do
		{
			/* Parse a parameter.  */
			n1 = strtol(param, &eptr, 10);
			if (*eptr != ';' && eptr != esc_term)
				goto sgr_set_it;

			switch (n1)
			{
			case 0:
				/* Reset.  */
				attrib_add |= (FOREGROUND_RED | FOREGROUND_GREEN
					| FOREGROUND_BLUE);
				attrib_rm = 0xFFFF; /* Removes everything.  */
				break;
			case 1:
				/* Bold.  */
				attrib_add |= FOREGROUND_INTENSITY;
				break;
			case 4:
				/* Underline.  */
				attrib_add |= COMMON_LVB_UNDERSCORE;
				break;
			case 5:
				/* Blink.  */
				/* XXX: It is not BLINKING at all! */
				attrib_add |= BACKGROUND_INTENSITY;
				break;
			case 7:
				/* Reverse.  */
				attrib_add |= COMMON_LVB_REVERSE_VIDEO;
				break;
			case 22:
				/* No bold.  */
				attrib_add &= ~FOREGROUND_INTENSITY;
				attrib_rm |= FOREGROUND_INTENSITY;
				break;
			case 24:
				/* No underline.  */
				attrib_add &= ~COMMON_LVB_UNDERSCORE;
				attrib_rm |= COMMON_LVB_UNDERSCORE;
				break;
			case 25:
				/* No blink.  */
				/* XXX: It is not BLINKING at all! */
				attrib_add &= ~BACKGROUND_INTENSITY;
				attrib_rm |= BACKGROUND_INTENSITY;
				break;
			case 27:
				/* No reverse.  */
				attrib_add &= ~COMMON_LVB_REVERSE_VIDEO;
				attrib_rm |= COMMON_LVB_REVERSE_VIDEO;
				break;
			case 30:
			case 31:
			case 32:
			case 33:
			case 34:
			case 35:
			case 36:
			case 37:
				/* Foreground color.  */
				attrib_add &= ~(FOREGROUND_RED | FOREGROUND_GREEN
					| FOREGROUND_BLUE);
				n1 -= 30;
				if (n1 & 1)
					attrib_add |= FOREGROUND_RED;
				if (n1 & 2)
					attrib_add |= FOREGROUND_GREEN;
				if (n1 & 4)
					attrib_add |= FOREGROUND_BLUE;
				attrib_rm |= (FOREGROUND_RED | FOREGROUND_GREEN
					| FOREGROUND_BLUE);
				break;
			case 38:
				/* Reserved for extended foreground color.
				Don't know how to handle parameters remaining.
				Bail out.  */
				goto sgr_set_it;
			case 39:
				/* Reset foreground color.  */
				/* Set to grey.  */
				attrib_add |= (FOREGROUND_RED | FOREGROUND_GREEN
					| FOREGROUND_BLUE);
				attrib_rm |= (FOREGROUND_RED | FOREGROUND_GREEN
					| FOREGROUND_BLUE);
				break;
			case 40:
			case 41:
			case 42:
			case 43:
			case 44:
			case 45:
			case 46:
			case 47:
				/* Background color.  */
				attrib_add &= ~(BACKGROUND_RED | BACKGROUND_GREEN
					| BACKGROUND_BLUE);
				n1 -= 40;
				if (n1 & 1)
					attrib_add |= BACKGROUND_RED;
				if (n1 & 2)
					attrib_add |= BACKGROUND_GREEN;
				if (n1 & 4)
					attrib_add |= BACKGROUND_BLUE;
				attrib_rm |= (BACKGROUND_RED | BACKGROUND_GREEN
					| BACKGROUND_BLUE);
				break;
			case 48:
				/* Reserved for extended background color.
				Don't know how to handle parameters remaining.
				Bail out.  */
				goto sgr_set_it;
			case 49:
				/* Reset background color.  */
				/* Set to black.  */
				attrib_add &= ~(BACKGROUND_RED | BACKGROUND_GREEN
					| BACKGROUND_BLUE);
				attrib_rm |= (BACKGROUND_RED | BACKGROUND_GREEN
					| BACKGROUND_BLUE);
				break;
			}

			/* Prepare the next parameter.  */
			param = eptr + 1;
		} while (param != esc_term);

	sgr_set_it:
		/* 0xFFFF removes everything.  If it is not the case,
		care must be taken to preserve old attributes.  */
		if (attrib_rm != 0xFFFF && GetConsoleScreenBufferInfo(h, &sb))
		{
			attrib_add |= sb.wAttributes & ~attrib_rm;
		}
		SetConsoleTextAttribute(h, attrib_add);
		break;
	}
}

static void WriteConsoleUTF8(HANDLE h, const char *str, uint32_t sz)
{
	if (!sz)
		return;

	int szWstr = utf8to16(str, sz, NULL, 0);
	wchar_t *wstr = alloca(sizeof(wchar_t) * szWstr);
	utf8to16(str, sz, wstr, szWstr);

	uint32_t slice;
	for (int i = 0; i < szWstr; i += slice)
	{
		slice = szWstr - i;

		DWORD szWritten = 0;
		if (WriteConsoleW(h, wstr + i, slice, &szWritten, NULL))
			slice = szWritten;
	}
}

static void EscapeOutput(HANDLE h, const char *str, uint32_t sz)
{
	DWORD tmp;
	BOOL isConsoleMode = GetConsoleMode(h, &tmp);
	if (isConsoleMode)
	{
		const char *read = str;
		int esc_code, prefix_len;
		const char *esc_head, *esc_term;

		/* If it is a console, translate ANSI escape codes as needed.  */
		for (;;)
		{
			if ((esc_code = find_esc_head(&prefix_len, &esc_head, read)) == 0)
			{
				/* Write all remaining characters, then exit.  */
				WriteConsoleUTF8(h, read, (uint32_t)(sz - (read - str)));
				break;
			}
			if (find_esc_terminator(&esc_term, esc_head) == 0)
				/* Ignore incomplete escape sequences at the moment.
				FIXME: The escape state shall be cached for further calls
				to this function.  */
				break;
			WriteConsoleUTF8(h, read, (uint32_t)(esc_head - prefix_len - read));
			eat_esc_sequence(h, esc_code, esc_head, esc_term);
			read = esc_term + 1;
		}
		// SetConsoleActiveScreenBuffer(h);
	}
	else
	{
		uint32_t slice;
		for (uint32_t i = 0; i < sz; i += slice)
		{
			slice = sz - i;

			DWORD szWritten = 0;
			if (WriteFile(h, str + i, slice, &szWritten, NULL))
				slice = szWritten;
		}
	}
}

int Win32ConsoleOutput(int isErr, const char *str, uint32_t sz)
{
	EscapeOutput(GetStdHandle(isErr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE), str, sz);
	return sz;
}

int Win32ConsoleInput(char *str, uint32_t sz)
{
	wchar_t *wstr = alloca(sizeof(wchar_t) * sz);
	DWORD szWstr = 0;

	if (!ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), wstr, sz, &szWstr, NULL))
		return -1;

	return utf16to8(wstr, szWstr, str, sz);
}

#endif
