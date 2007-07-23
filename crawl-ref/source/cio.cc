/*
 *  File:       cio.cc
 *  Summary:    Platform-independent console IO functions.
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-06-14T18:14:26.828542Z $
 */

#include "AppHdr.h"

#include "cio.h"
#include "externs.h"
#include "macro.h"

#include <queue>

#ifdef UNIX
static keycode_type numpad2vi(keycode_type key)
{
    if (key >= '1' && key <= '9')
    {
        const char *vikeys = "bjnh.lyku";
        return keycode_type(vikeys[key - '1']);
    }
    return (key);
}
#endif

int unmangle_direction_keys(int keyin, int km, bool fake_ctrl, bool fake_shift)
{
    const KeymapContext keymap = static_cast<KeymapContext>(km);
#ifdef UNIX
    // Kludging running and opening as two character sequences
    // for Unix systems.  This is an easy way out... all the
    // player has to do is find a termcap and numlock setting
    // that will get curses the numbers from the keypad.  This
    // will hopefully be easy.

    /* can we say yuck? -- haranp */
    if (fake_ctrl && keyin == '*')
    {
        keyin = getchm(keymap);
        // return control-key
        keyin = CONTROL(toupper(numpad2vi(keyin)));
    }
    else if (fake_shift && keyin == '/')
    {
        keyin = getchm(keymap);
        // return shift-key
        keyin = toupper(numpad2vi(keyin));
    }
#else
    // Old DOS keypad support
    if (keyin == 0)
    {
        /* FIXME haranp - hackiness */
        const char DOSidiocy[10]     = { "OPQKSMGHI" };
        const char DOSunidiocy[10]   = { "bjnh.lyku" };
        const int DOScontrolidiocy[9] =
        {
            117, 145, 118, 115, 76, 116, 119, 141, 132
        };
        keyin = getchm(keymap);
        for (int j = 0; j < 9; ++j )
        {
            if (keyin == DOSidiocy[j])
            {
                keyin = DOSunidiocy[j];
                break;
            }
            if (keyin == DOScontrolidiocy[j])
            {
                keyin = CONTROL(toupper(DOSunidiocy[j]));
                break;
            }
        }
    }
#endif

    // [dshaligram] More lovely keypad mangling.
    switch (keyin)
    {
#ifdef UNIX
    case '1': return 'b';
    case '2': return 'j';
    case '3': return 'n';
    case '4': return 'h';
    case '6': return 'l';
    case '7': return 'y';
    case '8': return 'k';
    case '9': return 'u';
#else
    case '1': return 'B';
    case '2': return 'J';
    case '3': return 'N';
    case '4': return 'H';
    case '6': return 'L';
    case '7': return 'Y';
    case '8': return 'K';
    case '9': return 'U';
#endif
    }

    return (keyin);
}

void get_input_line( char *const buff, int len )
{
    buff[0] = 0;         // just in case

#if defined(UNIX)
    get_input_line_from_curses( buff, len ); // implemented in libunix.cc
#elif defined(WIN32CONSOLE)
    getstr( buff, len );
#else

    // [dshaligram] Turn on the cursor for DOS.
#ifdef DOS
    _setcursortype(_NORMALCURSOR);
#endif

    fgets( buff, len, stdin );  // much safer than gets()
#endif

    buff[ len - 1 ] = 0;  // just in case 

    // Removing white space from the end in order to get rid of any
    // newlines or carriage returns that any of the above might have 
    // left there (ie fgets especially).  -- bwr
    const int end = strlen( buff ); 
    int i; 

    for (i = end - 1; i >= 0; i++) 
    {
        if (isspace( buff[i] ))
            buff[i] = 0;
        else
            break;
    }
}

// Hacky wrapper around getch() that returns CK_ codes for keys
// we want to use in cancelable_get_line() and menus.
int c_getch()
{
#if defined(DOS) || defined(UNIX) || defined(WIN32CONSOLE)
    return getch_ck();
#else
    return m_getch();
#endif
}

// Wrapper around gotoxy that can draw a fake cursor for Unix terms where
// cursoring over darkgray or black causes problems.
void cursorxy(int x, int y)
{
#ifdef UNIX
    if (Options.use_fake_cursor)
        fakecursorxy(x, y);
    else
        gotoxy(x, y);
#else
    gotoxy(x, y);
#endif
}

// cprintf that knows how to wrap down lines (primitive, but what the heck)
int wrapcprintf( int wrapcol, const char *s, ... )
{
    char buf[1000]; // Hard max
    va_list args;
    va_start(args, s);

    // XXX: If snprintf isn't available, vsnprintf probably isn't, either.
    int len = vsnprintf(buf, sizeof buf, s, args);
    int olen = len;
    va_end(args);

    char *run = buf;
    while (len > 0)
    {
        int x = wherex(), y = wherey();

        if (x > wrapcol) // Somebody messed up!
            return 0;
        
        int avail = wrapcol - x + 1;
        int c = 0;
        if (len > avail)
        {
            c = run[avail];
            run[avail] = 0;
        }
        cprintf("%s", run);
        
        if (len > avail)
            run[avail] = c;

        if ((len -= avail) > 0)
            gotoxy(1, y + 1);
        run += avail;
    }
    return (olen);
}

int cancelable_get_line( char *buf, int len, int maxcol,
                         input_history *mh, int (*keyproc)(int &ch) )
{
    line_reader reader(buf, len, maxcol);
    reader.set_input_history(mh);
    reader.set_keyproc(keyproc);

    return reader.read_line();
}


/////////////////////////////////////////////////////////////
// input_history
//

input_history::input_history(size_t size)
    : history(), pos(), maxsize(size)
{
    if (maxsize < 2)
        maxsize = 2;

    pos = history.end();
}

void input_history::new_input(const std::string &s)
{
    history.remove(s);

    if (history.size() == maxsize)
        history.pop_front();

    history.push_back(s);

    // Force the iterator to the end (also revalidates it)
    go_end();
}

const std::string *input_history::prev()
{
    if (history.empty())
        return NULL;

    if (pos == history.begin())
        pos = history.end();

    return &*--pos;
}

const std::string *input_history::next()
{
    if (history.empty())
        return NULL;

    if (pos == history.end() || ++pos == history.end())
        pos = history.begin();

    return &*pos;
}

void input_history::go_end()
{
    pos = history.end();
}

void input_history::clear()
{
    history.clear();
    go_end();
}

/////////////////////////////////////////////////////////////////////////
// line_reader

line_reader::line_reader(char *buf, size_t sz, int wrap)
    : buffer(buf), bufsz(sz), history(NULL), start_x(0),
      start_y(0), keyfn(NULL), wrapcol(wrap), cur(NULL),
      length(0), pos(-1)
{
}

std::string line_reader::get_text() const
{
    return (buffer);
}

void line_reader::set_input_history(input_history *i)
{
    history = i;
}

void line_reader::set_keyproc(keyproc fn)
{
    keyfn = fn;
}

void line_reader::cursorto(int ncx)
{
    int x = (start_x + ncx - 1) % wrapcol + 1;
    int y = start_y + (start_x + ncx - 1) / wrapcol;
    ::gotoxy(x, y);
}

int line_reader::read_line(bool clear_previous)
{
    if (bufsz <= 0) return false;

    cursor_control coff(true);

    if (clear_previous)
        *buffer = 0;

    start_x = wherex();
    start_y = wherey();

    length = strlen(buffer);

    // Remember the previous cursor position, if valid.
    if (pos < 0 || pos > length)
        pos = length;

    cur = buffer + pos;

    if (length)
        wrapcprintf(wrapcol, "%s", buffer);

    if (pos != length)
        cursorto(pos);

    if (history)
        history->go_end();

    for ( ; ; )
    {
        int ch = getchm(c_getch);

        if (keyfn)
        {
            int whattodo = (*keyfn)(ch);
            if (whattodo == 0)
            {
                buffer[length] = 0;
                if (history && length)
                    history->new_input(buffer);
                return (0);
            }
            else if (whattodo == -1)
            {
                buffer[length] = 0;                
                return (ch);
            }
        }

        int ret = process_key(ch);
        if (ret != -1)
            return (ret);
    }
}

void line_reader::backspace()
{
    if (pos)
    {
        --cur;
        char *c = cur;
        while (*c)
        {
            *c = c[1];
            c++;
        }
        --pos;
        --length;

        cursorto(pos);
        buffer[length] = 0;
        wrapcprintf( wrapcol, "%s ", cur );
        cursorto(pos);
    }
}

bool line_reader::is_wordchar(int c)
{
    return isalnum(c) || c == '_' || c == '-';
}

void line_reader::kill_to_begin()
{
    if (!pos || cur == buffer)
        return;

    buffer[length] = 0;
    cursorto(0);
    wrapcprintf(wrapcol, "%s%*s", cur, cur - buffer, "");
    memmove(buffer, cur, length - pos);
    length -= pos;
    pos = 0;
    cur = buffer;
    cursorto(pos);
}

void line_reader::killword()
{
    if (!pos || cur == buffer)
        return;

    bool foundwc = false;
    while (pos)
    {
        if (is_wordchar(cur[-1]))
            foundwc = true;
        else if (foundwc)
            break;

        backspace();
    }
}

int line_reader::process_key(int ch)
{
    switch (ch)
    {
    case CONTROL('G'):
    case CK_ESCAPE:
        return (CK_ESCAPE);
    case CK_UP:
    case CK_DOWN:
    {
        if (!history)
            break;

        const std::string *text = 
                    ch == CK_UP? history->prev() : history->next();

        if (text)
        {
            int olen = length;
            length = text->length();
            if (length >= (int) bufsz)
                length = bufsz - 1;
            memcpy(buffer, text->c_str(), length);
            buffer[length] = 0;
            cursorto(0);

            int clear = length < olen? olen - length : 0;
            wrapcprintf(wrapcol, "%s%*s", buffer, clear, "");

            pos = length;
            cur = buffer + pos;
            cursorto(pos);
        }
        break;
    }
    case CK_ENTER:
        buffer[length] = 0;
        if (history && length)
            history->new_input(buffer);
        return (0);

    case CONTROL('K'):
    {
        // Kill to end of line
        int erase = length - pos;
        if (erase)
        {
            length = pos;
            buffer[length] = 0;
            wrapcprintf( wrapcol, "%*s", erase, "" );
            cursorto(pos);
        }
        break;
    }
    case CK_DELETE:
        if (pos < length)
        {
            char *c = cur;
            while (c - buffer < length)
            {
                *c = c[1];
                c++;
            }
            --length;

            cursorto(pos);
            buffer[length] = 0;
            wrapcprintf( wrapcol, "%s ", cur );
            cursorto(pos);
        }
        break;

    case CK_BKSP:
        backspace();
        break;

    case CONTROL('W'):
        killword();
        break;

    case CONTROL('U'):
        kill_to_begin();
        break;

    case CK_LEFT:
        if (pos)
        {
            --pos;
            cur = buffer + pos;
            cursorto(pos);
        }
        break;
    case CK_RIGHT:
        if (pos < length)
        {
            ++pos;
            cur = buffer + pos;
            cursorto(pos);
        }
        break;
    case CK_HOME:
    case CONTROL('A'):
        pos = 0;
        cur = buffer + pos;
        cursorto(pos);
        break;
    case CK_END:
    case CONTROL('E'):
        pos = length;
        cur = buffer + pos;
        cursorto(pos);
        break;
    default:
        if (isprint(ch) && length < (int) bufsz - 1)
        {
            if (pos < length)
            {
                char *c = buffer + length - 1;
                while (c >= cur)
                {
                    c[1] = *c;
                    c--;
                }
            }
            *cur++ = (char) ch;
            ++length;
            ++pos;
            putch(ch);
            if (pos < length)
            {
                buffer[length] = 0;
                wrapcprintf( wrapcol, "%s", cur );
            }
            cursorto(pos);
        }
        break;
    }

    return (-1);
}

/////////////////////////////////////////////////////////////////////////////
// Of mice and other mice.

static std::queue<c_mouse_event> mouse_events;

coord_def get_mouse_pos()
{
    // lib$(OS) has to maintain mousep. This function is just the messenger.
    return (crawl_view.mousep);
}

c_mouse_event get_mouse_event()
{
    if (mouse_events.empty())
        return c_mouse_event();

    c_mouse_event ce = mouse_events.front();
    mouse_events.pop();
    return (ce);
}

void new_mouse_event(const c_mouse_event &ce)
{
    mouse_events.push(ce);
}

void flush_mouse_events()
{
    while (!mouse_events.empty())
        mouse_events.pop();
}

void c_input_reset(bool enable_mouse, bool flush)
{
    crawl_state.mouse_enabled = (enable_mouse && Options.mouse_input);
    set_mouse_enabled(crawl_state.mouse_enabled);

    if (flush)
        flush_mouse_events();
}
