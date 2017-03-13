// See LICENSE for license details.
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <fontconfig/fontconfig.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#if !defined(CLOCK_MONOTONIC) && defined(__MACH__)
#include <mach/clock.h>
#include <mach/mach.h>
#endif

// Needed for openpty.
#if defined(__linux)
#include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <util.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <libutil.h>
#endif

#ifdef __GNUC__
#define NORETURN __attribute__((noreturn))
#define UNUSED __attribute__((unused))
#else
#define NORETURN
#define UNUSED
#endif

#define Glyph Glyph_
#define Font Font_

// See the XEmbed Protocol Specification
// <https://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html> for
// more info.
#define XEMBED_FOCUS_IN 4
#define XEMBED_FOCUS_OUT 5

// Arbitrary sizes.
#define ESC_BUF_SIZ 65536
#define ESC_ARG_SIZ 16
#define STR_BUF_SIZ ESC_BUF_SIZ
#define STR_ARG_SIZ ESC_ARG_SIZ
#define XK_ANY_MOD UINT_MAX
#define XK_SWITCH_MOD (1 << 13)

// Macros.
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a) / sizeof(a)[0])
#define DEFAULT(a, b) (a) = (a) ? (a) : (b)
#define BETWEEN(x, a, b) ((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d) (((n) + ((d)-1)) / (d))
#define ISCONTROLC0(c) (BETWEEN(c, 0, 0x1F) || (c) == 0x7F)
#define ISCONTROLC1(c) (BETWEEN(c, 0x80, 0x9F))
#define ISCONTROL(c) (ISCONTROLC0(c) || ISCONTROLC1(c))
#define ISDELIM(u) (utf8strchr(worddelimiters, u) != NULL)
#define LIMIT(x, a, b) (x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define ATTRCMP(a, b) \
	((a).mode != (b).mode || (a).fg != (b).fg || (a).bg != (b).bg)
#define IS_SET(flag) ((term.mode & (flag)) != 0)
#define TIMEDIFF(t1, t2)                      \
	(((t1).tv_sec - (t2).tv_sec) * 1000 + \
	 ((t1).tv_nsec - (t2).tv_nsec) / 1E6)
#define MODBIT(x, set, bit) ((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))

#define TRUECOLOR(r, g, b) (1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x) (1 << 24 & (x))
#define TRUERED(x) (((x)&0xFF0000) >> 8)
#define TRUEGREEN(x) (((x)&0xFF00))
#define TRUEBLUE(x) (((x)&0xFF) << 8)
#define XA_CLIPBOARD XInternAtom(xw.dpy, "CLIPBOARD", 0)

// Form of C1 controls accepted in UTF-8 mode.
enum c1utf8_form {
	C1UTF8_AS_BYTE = 1 << 1,  // Single bytes, e.g. "\x90" for DCS.
	C1UTF8_AS_UTF8 = 1 << 2,  // UTF-8 sequences, e.g. "\xC2\x90" for DCS.
};

enum glyph_attribute {
	ATTR_NULL = 0,
	ATTR_BOLD = 1 << 0,
	ATTR_FAINT = 1 << 1,
	ATTR_ITALIC = 1 << 2,
	ATTR_UNDERLINE = 1 << 3,
	ATTR_BLINK = 1 << 4,
	ATTR_REVERSE = 1 << 5,
	ATTR_INVISIBLE = 1 << 6,
	ATTR_STRUCK = 1 << 7,
	ATTR_WRAP = 1 << 8,
	ATTR_WIDE = 1 << 9,
	ATTR_WDUMMY = 1 << 10,
	ATTR_BOLD_FAINT = ATTR_BOLD | ATTR_FAINT,
};

enum cursor_movement {
	CURSOR_SAVE,  // Save the current cursor.
	CURSOR_LOAD   // Restore the cursor.
};

enum cursor_state {
	CURSOR_DEFAULT = 0,
	CURSOR_WRAPNEXT = 1,
	CURSOR_ORIGIN = 2
};

enum term_mode {
	MODE_WRAP = 1 << 0,
	MODE_INSERT = 1 << 1,
	MODE_APPKEYPAD = 1 << 2,
	MODE_ALTSCREEN = 1 << 3,
	MODE_CRLF = 1 << 4,
	MODE_MOUSEBTN = 1 << 5,
	MODE_MOUSEMOTION = 1 << 6,
	MODE_REVERSE = 1 << 7,
	MODE_KBDLOCK = 1 << 8,
	MODE_HIDE = 1 << 9,
	MODE_ECHO = 1 << 10,
	MODE_APPCURSOR = 1 << 11,
	MODE_MOUSESGR = 1 << 12,
	MODE_8BIT = 1 << 13,
	MODE_BLINK = 1 << 14,
	MODE_FBLINK = 1 << 15,
	MODE_FOCUS = 1 << 16,
	MODE_MOUSEX10 = 1 << 17,
	MODE_MOUSEMANY = 1 << 18,
	MODE_BRCKTPASTE = 1 << 19,
	MODE_PRINT = 1 << 20,
	MODE_UTF8 = 1 << 21,
	MODE_ENABLE_COLUMN_CHANGE = 1 << 22,
	MODE_CLEAR_ON_DECCOLM = 1 << 23,
	MODE_WRITABLE_STATUS_LINE = 1 << 24,
	MODE_MOUSE =
	    MODE_MOUSEBTN | MODE_MOUSEMOTION | MODE_MOUSEX10 | MODE_MOUSEMANY,
};

enum charset {
	CS_SPECIAL_GRAPHIC,
	CS_TECHNICAL,
	CS_US_ASCII,
	CS_CURSES  // Extension: A special character set to support the Curses
	           // alternate character set.
};

enum escape_state {
	ESC_START = 1,
	ESC_CSI = 2,
	ESC_STR = 4,  // OSC, PM, APC
	ESC_ALTCHARSET = 8,
	ESC_STR_END = 16,  // a final string was encountered
	ESC_TEST = 32,     // Enter in test mode
	ESC_UTF8 = 64,
	ESC_DCS = 128,
};

enum window_state {
	WIN_VISIBLE = 1,  // Window is visible.
	WIN_FOCUSED = 2,  // Window is focused.
};

enum selection_mode {
	SEL_IDLE = 0,   // We may or may not have a selection.
	SEL_EMPTY = 1,  // We do not have a selection.
	SEL_READY = 2   // The user is in the process of selecting.
};

enum selection_type {
	SEL_REGULAR = 0,     // Standard sequential selection.
	SEL_RECTANGULAR = 1  // Rectangular selection.
};

enum selection_snap {
	SNAP_WORD = 1,  // Snap around if the word wraps.
	SNAP_LINE = 2   // Snap around if the line wraps.
};

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;

typedef uint_least32_t Rune;  // Needs at least 21 bits.
static const Rune replacement_rune = 0xFFFD;
static const size_t max_utf8_bytes = 4;

typedef XftDraw *Draw;
typedef XftColor Color;

typedef struct {
	Rune u;       // character code
	ushort mode;  // attribute flags
	uint32_t fg;  // foreground
	uint32_t bg;  // background
} Glyph;

typedef Glyph *Line;

typedef struct {
	Glyph attr;  // current char attributes
	int x;
	int y;
	char state;
} TCursor;

// CSI Escape sequence structs
// ESC '[' [[ [<interm>] <arg> [;]] <mode> [<mode>]]
typedef struct {
	char buf[ESC_BUF_SIZ];  // raw string
	size_t len;             // raw string length
	char interm;
	int arg[ESC_ARG_SIZ];
	int narg;  // nb of args
	char mode[2];
} CSIEscape;

// STR Escape sequence structs
// ESC type [[ [<priv>] <arg> [;]] <mode>] ESC '\'
typedef struct {
	char type;              // ESC type ...
	char buf[STR_BUF_SIZ];  // raw string
	int len;                // raw string length
	char *args[STR_ARG_SIZ];
	int narg;  // nb of args
} STREscape;

// Internal representation of the screen
typedef struct {
	ushort row;                 // nb row
	ushort col;                 // nb col
	Line *line;                 // screen
	Line *alt;                  // alternate screen
	int *dirty;                 // dirtyness of lines
	XftGlyphFontSpec *specbuf;  // font spec buffer used for rendering
	TCursor c;                  // cursor
	int top;                    // top    scroll limit
	int bot;                    // bottom scroll limit
	int mode;                   // terminal mode flags
	int esc;                    // escape state flags
	enum charset trantbl[4];    // charset table translation
	int charset;                // current charset
	int icharset;               // selected charset for sequence
	int *tabs;
} Term;

// Purely graphic info
typedef struct {
	Display *dpy;
	Colormap cmap;
	Window win;
	Drawable buf;
	Atom xembed, wmdeletewin, netwmname, netwmpid;
	XIM xim;
	XIC xic;
	Draw draw;
	Visual *vis;
	XSetWindowAttributes attrs;
	int scr;
	int isfixed;  // is fixed geometry?
	int l, t;     // left and top offset
	int gm;       // geometry mask
	int tw, th;   // tty width and height
	int w, h;     // window width and height
	int ch;       // char height
	int cw;       // char width
	char state;   // focus, redraw, visible
	int cursor;   // cursor style
} XWindow;

typedef struct {
	uint b;
	uint mask;
	const char *s;
} MouseShortcut;

typedef struct {
	KeySym k;
	uint mask;
	const char *s;
	// three valued logic variables: 0 indifferent, 1 on, -1 off
	signed char appkey;     // application keypad
	signed char appcursor;  // application cursor
	signed char crlf;       // crlf mode
} Key;

typedef struct {
	int mode;
	int type;
	int snap;
	/*
	 * Selection variables:
	 * nb - normalized coordinates of the beginning of the selection
	 * ne - normalized coordinates of the end of the selection
	 * ob - original coordinates of the beginning of the selection
	 * oe - original coordinates of the end of the selection
	 */
	struct {
		int x, y;
	} nb, ne, ob, oe;

	char *primary, *clipboard;
	Atom xtarget;
	int alt;
	struct timespec tclick1;
	struct timespec tclick2;
} Selection;

typedef struct {
	uint mod;
	KeySym keysym;
	void (*func)(int);
	int arg;
} Shortcut;

const char *argv0;

// function definitions used in config.h
static void clipcopy(int /*unused*/);
static void clippaste(int /*unused*/);
static void selpaste(int /*unused*/);
static void xzoom(int /*increase*/);
static void xzoomabs(int /*fontsize*/);
static void xzoomreset(int /*unused*/);
static void printsel(int /*unused*/);
static void printscreen(int /*unused*/);
static void iso14755(int /*unused*/);
static void toggleprinter(int /*unused*/);
static void sendbreak(int /*unused*/);
static void reset(int /*unused*/);

// Config.h for applying patches and the configuration.
#include "config.h"

// Font structure
typedef struct {
	int height;
	int width;
	int ascent;
	int descent;
	int badslant;
	int badweight;
	short lbearing;
	short rbearing;
	XftFont *match;
	FcFontSet *set;
	FcPattern *pattern;
} Font;

// Drawing Context
typedef struct {
	Color col[MAX(LEN(colorname), 256)];
	Font font, bfont, ifont, ibfont;
	GC gc;
} DC;

NORETURN static void die(const char * /*errstr*/, ...);
static void draw(void);
static void redraw(void);
static void drawregion(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/);
static void execsh(void);
static void stty(void);
static void sigchld_handler(UNUSED int /*unused*/);
static void sigsegv_handler(UNUSED int /*sig*/);
static int run(void);

static void chardump(char c);
static void csidump(void);
static void csihandle(void);
static void csiparse(void);
static void csireset(void);
static int eschandle(uchar /*ascii*/);
static void strdump(void);
static void strhandle(void);
static void strparse(void);
static void strreset(void);

static int tattrset(int /*attr*/);
static void tprinter(const char * /*s*/, size_t /*len*/);
static void tdumpsel(void);
static void tdumpline(int /*n*/);
static void tdump(void);
static void tclearregion(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/);
static void tcursor(enum cursor_movement /*mode*/);
static void tdeletechar(int /*n*/);
static void tdeleteline(int /*n*/);
static void tinsertblank(int /*n*/);
static void tinsertblankline(int /*n*/);
static int tlinelen(int /*y*/);
static void tmoveto(int /*x*/, int /*y*/);
static void tmoveato(int /*x*/, int /*y*/);
static void tnew(int /*col*/, int /*row*/);
static void tnewline(int /*first_col*/);
static void tputtab(int /*n*/);
static void tputc(Rune /*u*/);
static void treset(void);
static void tresize(int /*col*/, int /*row*/);
static void tscrollup(int /*orig*/, int /*n*/);
static void tscrolldown(int /*orig*/, int /*n*/);
static void tsetattr(int * /*attr*/, int /*l*/);
static void tsetchar(Rune /*u*/, const Glyph * /*attr*/, int /*x*/, int /*y*/);
static void tsetscroll(int /*t*/, int /*b*/);
static void tswapscreen(void);
static void tsetdirt(int /*top*/, int /*bot*/);
static void tsetdirtattr(int /*attr*/);
static void tsetmode(char /*interm*/, int /*set*/, const int * /*args*/,
                     int /*narg*/);
static void tfulldirt(void);
static void techo(Rune /*u*/);
static void tcontrolcode(uchar /*ascii*/);
static void tdectest(char /*c*/);
static void tdefutf8(char /*ascii*/);
static int32_t tdefcolor(const int * /*attr*/, int * /*npar*/, int /*l*/);
static void tdeftran(char /*ascii*/);
static inline int modifiers_match(uint /*mask*/, uint /*state*/);
static void ttynew(void);
static size_t ttyread(void);
static void ttyresize(void);
static void ttysend(const char * /*s*/, size_t /*n*/);
static void ttywrite(const char * /*s*/, size_t /*n*/);
static void tstrsequence(uchar /*c*/);

static inline ushort sixd_to_16bit(int /*x*/);
static int xmakeglyphfontspecs(XftGlyphFontSpec * /*specs*/,
                               const Glyph * /*glyphs*/, int /*len*/, int /*x*/,
                               int /*y*/);
static Color *xgetcolor(uint32_t /*basecol*/, XRenderColor * /*rendercol*/,
                        Color * /*truecol*/);
static void xdrawglyphfontspecs(const XftGlyphFontSpec * /*specs*/, Glyph,
                                int /*len*/, int /*x*/, int /*y*/);
static void xdrawglyph(Glyph, int /*x*/, int /*y*/);
static void xhints(void);
static void xclear(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/);
static void xdrawcursor(void);
static const char *xgetresstr(XrmDatabase /*xrmdb*/, const char * /*name*/,
                              const char * /*xclass*/, const char * /*def*/);
static Bool xgetresbool(XrmDatabase /*xrmdb*/, const char * /*name*/,
                        const char * /*xclass*/, Bool);
static void xinit(int argc, char *argv[]);
static int xloadcolor(int /*i*/, const char * /*name*/, Color * /*ncolor*/);
static void xloadcols(void);
static int xsetcolorname(int /*x*/, const char * /*name*/);
static int xgeommasktogravity(int /*mask*/);
static int xloadfont(Font *, const FcPattern * /*pattern*/);
static void xloadfonts(const char * /*fontstr*/, double /*fontsize*/);
static void xsettitle(const char * /*p*/);
static void xresettitle(void);
static void xsetpointermotion(int /*set*/);
static void xseturgency(int /*add*/);
static void xsetsel(char * /*str*/, Bool, Time /*t*/);
static void xunloadfont(Font *);
static void xunloadfonts(void);
static void xresize(int /*col*/, int /*row*/);

static void expose(XEvent * /*unused*/);
static void visibility(XEvent * /*ev*/);
static void unmap(XEvent * /*unused*/);
static const char *kmap(KeySym /*k*/, uint /*state*/);
static void kpress(XEvent * /*ev*/);
static void cmessage(XEvent * /*e*/);
static void cresize(int /*width*/, int /*height*/);
static void resize(XEvent * /*e*/);
static void focus(XEvent * /*ev*/);
static void brelease(XEvent * /*e*/);
static void bpress(XEvent * /*e*/);
static void bmotion(XEvent * /*e*/);
static void propnotify(XEvent * /*e*/);
static void selnotify(XEvent * /*e*/);
static void selclear(XEvent * /*unused*/);
static void selrequest(XEvent * /*e*/);

static void selinit(void);
static void selnormalize(void);
static inline int selected(int /*x*/, int /*y*/);
static char *getsel(void);
static void selcopy(Time /*t*/);
static void selscroll(int /*orig*/, int /*n*/);
static void selsnap(int * /*x*/, int * /*y*/, int /*direction*/);
static int x2col(int /*x*/);
static int y2row(int /*y*/);
static void getbuttoninfo(const XEvent * /*e*/);
static void mousereport(const XEvent * /*e*/);

static size_t utf8decode(const char * /*c*/, size_t /*clen*/, Rune * /*u*/);
static Rune utf8decodebyte(uchar /*c*/, size_t * /*i*/);
static size_t utf8encode(Rune /*u*/, char * /*c*/);
static char utf8encodebyte(Rune /*u*/, size_t /*i*/);
static const char *utf8strchr(const char *s, Rune u);
static size_t utf8validate(Rune * /*u*/, size_t /*i*/);

static size_t base64decode(const char * /*enc*/, uchar ** /*out*/,
                           size_t * /*outlen*/);

static ssize_t xwrite(int /*fd*/, const char * /*s*/, size_t /*len*/);
static void *xmalloc(size_t /*len*/);
static void *xrealloc(void * /*p*/, size_t /*len*/);
static char *xstrdup(const char * /*s*/);

static void usage(void);

// clang-format off
static void (*handler[LASTEvent])(XEvent *) = {
	[KeyPress] = kpress,
	[ClientMessage] = cmessage,
	[ConfigureNotify] = resize,
	[VisibilityNotify] = visibility,
	[UnmapNotify] = unmap,
	[Expose] = expose,
	[FocusIn] = focus,
	[FocusOut] = focus,
	[MotionNotify] = bmotion,
	[ButtonPress] = bpress,
	[ButtonRelease] = brelease,
	/*
	 * Uncomment if you want the selection to disappear when you select
	 * something different in another window.
	 */
	// [SelectionClear] = selclear,
	[SelectionNotify] = selnotify,
	/*
	 * PropertyNotify is only turned on when there is some INCR transfer
	 * happening for the selection retrieval.
	 */
	[PropertyNotify] = propnotify,
	[SelectionRequest] = selrequest,
};
// clang-format on

// Globals
static DC dc;
static XWindow xw;
static Term term;
static CSIEscape csiescseq;
static STREscape strescseq;
static int cmdfd;
static pid_t pid;
static Selection sel;
static int iofd = 1;
static int opt_allowaltscreen;
static const char **opt_cmd = NULL;
static const char *opt_class = NULL;
static unsigned int opt_cols = cols;
static const char *opt_embed = NULL;
static const char *opt_font = NULL;
static const char *opt_io = NULL;
static const char *opt_iso14755_cmd = NULL;
static const char *opt_line = NULL;
static const char *opt_name = NULL;
static unsigned int opt_rows = rows;
static const char *opt_title = NULL;
static int oldbutton = 3;  // button event on startup: 3 = release

static const char *usedfont = NULL;
static double usedfontsize = 0;
static double defaultfontsize = 0;
static int exit_with_code = -1;

static uchar utfbyte[] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static uchar utfmask[] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
static Rune utfmin[] = {0, 0, 0x80, 0x800, 0x10000};
static Rune utfmax[] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

static const uchar base64decode_table[256] =
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@>@@@?456789:;<=@@@@@@"
    "@\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E"
    "\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19@@@@@"
    "@\x1A\x1B\x1C\x1D\x1E\x1F !\"#$%&'()*+,-./0123@@@@@"
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@";

// Font Ring Cache
enum frc_style {
	FRC_NORMAL,     // Regular.
	FRC_ITALIC,     // Italic.
	FRC_BOLD,       // Bold.
	FRC_ITALICBOLD  // Bold and italic.
};

typedef struct {
	XftFont *font;
	int flags;
	Rune unicodep;
} Fontcache;

// New fonts will be appended to the array.
static Fontcache frc[16];
static size_t frclen = 0;

#if !defined(CLOCK_MONOTONIC) && defined(__MACH__)
#define CLOCK_MONOTONIC 1
static int
clock_gettime(int clk_id, struct timespec *ts)
{
	assert(clk_id == CLOCK_MONOTONIC);
	clock_serv_t clock_serv;
	mach_timespec_t cur_time;
	if (host_get_clock_service(mach_host_self(), SYSTEM_CLOCK,
	                           &clock_serv) != KERN_SUCCESS) {
		errno = EPERM;
		return -1;
	}
	if (clock_get_time(clock_serv, &cur_time) != KERN_SUCCESS) {
		errno = EPERM;
		return -1;
	}
	if (mach_port_deallocate(mach_task_self(), clock_serv) !=
	    KERN_SUCCESS) {
		errno = EPERM;
		return -1;
	}
	ts->tv_sec = cur_time.tv_sec;
	ts->tv_nsec = cur_time.tv_nsec;
	return 0;
}
#endif

ssize_t
xwrite(int fd, const char *s, size_t len)
{
	size_t aux = len;
	ssize_t r;

	while (len > 0) {
		r = write(fd, s, len);
		if (r < 0) {
			return r;
		}
		len -= r;
		s += r;
	}

	return aux;
}

void *
xmalloc(size_t len)
{
	void *p = malloc(len);

	if (!p) {
		die("Out of memory\n");
	}

	return p;
}

void *
xrealloc(void *p, size_t len)
{
	if ((p = realloc(p, len)) == NULL) {
		die("Out of memory\n");
	}

	return p;
}

char *
xstrdup(const char *s)
{
	char *dup = strdup(s);
	if (dup == NULL) {
		die("Out of memory\n");
	}

	return dup;
}

size_t
utf8decode(const char *c, size_t clen, Rune *u)
{
	size_t i, j, len, type;
	Rune udecoded;

	*u = replacement_rune;
	if (!clen) {
		return 0;
	}
	udecoded = utf8decodebyte(c[0], &len);
	if (!BETWEEN(len, 1, max_utf8_bytes)) {
		return 1;
	}
	for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
		udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
		if (type != 0) {
			return j;
		}
	}
	if (j < len) {
		return 0;
	}
	*u = udecoded;
	utf8validate(u, len);

	return len;
}

Rune
utf8decodebyte(uchar c, size_t *i)
{
	for (*i = 0; *i < LEN(utfmask); ++(*i)) {
		if ((c & utfmask[*i]) == utfbyte[*i]) {
			return c & ~utfmask[*i];
		}
	}

	return 0;
}

size_t
utf8encode(Rune u, char *c)
{
	size_t len, i;

	len = utf8validate(&u, 0);
	if (len > max_utf8_bytes) {
		return 0;
	}

	for (i = len - 1; i != 0; --i) {
		c[i] = utf8encodebyte(u, 0);
		u >>= 6;
	}
	c[0] = utf8encodebyte(u, len);

	return len;
}

char
utf8encodebyte(Rune u, size_t i)
{
	return utfbyte[i] | (u & ~utfmask[i]);
}

const char *
utf8strchr(const char *s, Rune u)
{
	Rune r;
	size_t i, j, len;

	len = strlen(s);
	for (i = 0, j = 0; i < len; i += j) {
		j = utf8decode(&s[i], len - i, &r);
		if (!j) {
			break;
		}
		if (r == u) {
			return &(s[i]);
		}
	}

	return NULL;
}

size_t
utf8validate(Rune *u, size_t i)
{
	if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF)) {
		*u = replacement_rune;
	}
	for (i = 1; *u > utfmax[i]; ++i) {
	}

	return i;
}

size_t
base64decode(const char *enc, uchar **out, size_t *outlen)
{
	size_t i, enclen = strlen(enc);
	uchar *current;

	*out = (uchar *)xmalloc((enclen * 3) / 4 + 1);
	current = *out;
	*outlen = 0;

	for (i = 0; i < enclen; ++i) {
		uchar x = base64decode_table[(uchar)enc[i]];
		if (x >= '@') {
			break;
		}
		switch (i % 4) {
		case 0:
			*current = x << 2;
			break;
		case 1:
			*current++ |= (x >> 4);
			*current = x << 4;
			(*outlen)++;
			break;
		case 2:
			*current++ |= (x >> 2);
			*current = x << 6;
			(*outlen)++;
			break;
		case 3:
			*current++ |= x;
			*current = 0;
			(*outlen)++;
			break;
		}
	}
	while (enc[i] == '=') {
		++i;
	}
	*current = 0;
	*outlen = current - *out;
	return i;
}

void
selinit(void)
{
	clock_gettime(CLOCK_MONOTONIC, &sel.tclick1);
	clock_gettime(CLOCK_MONOTONIC, &sel.tclick2);
	sel.mode = SEL_IDLE;
	sel.snap = 0;
	sel.ob.x = -1;
	sel.primary = NULL;
	sel.clipboard = NULL;
	sel.xtarget = XInternAtom(xw.dpy, "UTF8_STRING", 0);
	if (sel.xtarget == None) {
		sel.xtarget = XA_STRING;
	}
}

int
x2col(int x)
{
	x -= borderpx;
	x /= xw.cw;

	return LIMIT(x, 0, term.col - 1);
}

int
y2row(int y)
{
	y -= borderpx;
	y /= xw.ch;

	return LIMIT(y, 0, term.row - 1);
}

int
tlinelen(int y)
{
	int i = term.col;

	if (term.line[y][i - 1].mode & ATTR_WRAP) {
		return i;
	}

	while (i > 0 && term.line[y][i - 1].u == ' ') {
		--i;
	}

	return i;
}

void
selnormalize(void)
{
	int i;

	if (sel.type == SEL_REGULAR && sel.ob.y != sel.oe.y) {
		sel.nb.x = sel.ob.y < sel.oe.y ? sel.ob.x : sel.oe.x;
		sel.ne.x = sel.ob.y < sel.oe.y ? sel.oe.x : sel.ob.x;
	} else {
		sel.nb.x = MIN(sel.ob.x, sel.oe.x);
		sel.ne.x = MAX(sel.ob.x, sel.oe.x);
	}
	sel.nb.y = MIN(sel.ob.y, sel.oe.y);
	sel.ne.y = MAX(sel.ob.y, sel.oe.y);

	selsnap(&sel.nb.x, &sel.nb.y, -1);
	selsnap(&sel.ne.x, &sel.ne.y, +1);

	// expand selection over line breaks
	if (sel.type == SEL_RECTANGULAR) {
		return;
	}
	i = tlinelen(sel.nb.y);
	if (i < sel.nb.x) {
		sel.nb.x = i;
	}
	if (tlinelen(sel.ne.y) <= sel.ne.x) {
		sel.ne.x = term.col - 1;
	}
}

int
selected(int x, int y)
{
	if (sel.mode == SEL_EMPTY) {
		return 0;
	}

	if (sel.type == SEL_RECTANGULAR) {
		return BETWEEN(y, sel.nb.y, sel.ne.y) &&
		       BETWEEN(x, sel.nb.x, sel.ne.x);
	}

	return BETWEEN(y, sel.nb.y, sel.ne.y) &&
	       (y != sel.nb.y || x >= sel.nb.x) &&
	       (y != sel.ne.y || x <= sel.ne.x);
}

void
selsnap(int *x, int *y, int direction)
{
	int newx, newy, xt, yt;
	int delim, prevdelim;
	Glyph *gp, *prevgp;

	switch (sel.snap) {
	case SNAP_WORD:
		/*
		 * Snap around if the word wraps around at the end or
		 * beginning of a line.
		 */
		prevgp = &term.line[*y][*x];
		prevdelim = ISDELIM(prevgp->u);
		for (;;) {
			newx = *x + direction;
			newy = *y;
			if (!BETWEEN(newx, 0, term.col - 1)) {
				newy += direction;
				newx = (newx + term.col) % term.col;
				if (!BETWEEN(newy, 0, term.row - 1)) {
					break;
				}

				if (direction > 0) {
					yt = *y, xt = *x;
				} else {
					yt = newy, xt = newx;
				}
				if (!(term.line[yt][xt].mode & ATTR_WRAP)) {
					break;
				}
			}

			if (newx >= tlinelen(newy)) {
				break;
			}

			gp = &term.line[newy][newx];
			delim = ISDELIM(gp->u);
			if (!(gp->mode & ATTR_WDUMMY) &&
			    (delim != prevdelim ||
			     (delim && gp->u != prevgp->u))) {
				break;
			}

			*x = newx;
			*y = newy;
			prevgp = gp;
			prevdelim = delim;
		}
		break;
	case SNAP_LINE:
		/*
		 * Snap around if the the previous line or the current one
		 * has set ATTR_WRAP at its end. Then the whole next or
		 * previous line will be selected.
		 */
		*x = (direction < 0) ? 0 : term.col - 1;
		if (direction < 0) {
			for (; *y > 0; *y += direction) {
				if (!(term.line[*y - 1][term.col - 1].mode &
				      ATTR_WRAP)) {
					break;
				}
			}
		} else if (direction > 0) {
			for (; *y < term.row - 1; *y += direction) {
				if (!(term.line[*y][term.col - 1].mode &
				      ATTR_WRAP)) {
					break;
				}
			}
		}
		break;
	}
}

void
getbuttoninfo(const XEvent *e)
{
	uint state = e->xbutton.state & ~(Button1Mask | forceselmod);

	sel.alt = IS_SET(MODE_ALTSCREEN);

	sel.oe.x = x2col(e->xbutton.x);
	sel.oe.y = y2row(e->xbutton.y);
	selnormalize();

	sel.type = SEL_REGULAR;
	for (size_t type = 0; type < LEN(selmasks); ++type) {
		if (modifiers_match(selmasks[type], state)) {
			sel.type = type;
			break;
		}
	}
}

void
mousereport(const XEvent *e)
{
	int x = x2col(e->xbutton.x), y = y2row(e->xbutton.y),
	    button = e->xbutton.button, state = e->xbutton.state, len;
	char buf[40];
	static int ox, oy;

	// from urxvt
	if (e->xbutton.type == MotionNotify) {
		if (x == ox && y == oy) {
			return;
		}
		if (!IS_SET(MODE_MOUSEMOTION) && !IS_SET(MODE_MOUSEMANY)) {
			return;
		}
		// MOUSE_MOTION: no reporting if no button is pressed
		if (IS_SET(MODE_MOUSEMOTION) && oldbutton == 3) {
			return;
		}

		button = oldbutton + 32;
		ox = x;
		oy = y;
	} else {
		if (!IS_SET(MODE_MOUSESGR) &&
		    e->xbutton.type == ButtonRelease) {
			button = 3;
		} else {
			button -= Button1;
			if (button >= 3) {
				button += 64 - 3;
			}
		}
		if (e->xbutton.type == ButtonPress) {
			oldbutton = button;
			ox = x;
			oy = y;
		} else if (e->xbutton.type == ButtonRelease) {
			oldbutton = 3;
			// MODE_MOUSEX10: no button release reporting
			if (IS_SET(MODE_MOUSEX10)) {
				return;
			}
			if (button == 64 || button == 65) {
				return;
			}
		}
	}

	if (!IS_SET(MODE_MOUSEX10)) {
		button += ((state & ShiftMask) ? 4 : 0) +
		          ((state & Mod4Mask) ? 8 : 0) +
		          ((state & ControlMask) ? 16 : 0);
	}

	if (IS_SET(MODE_MOUSESGR)) {
		len = snprintf(buf, sizeof(buf), "\x1B[<%d;%d;%d%c", button,
		               x + 1, y + 1,
		               e->xbutton.type == ButtonRelease ? 'm' : 'M');
	} else if (x < 223 && y < 223) {
		len = snprintf(buf, sizeof(buf), "\x1B[M%c%c%c", 32 + button,
		               32 + x + 1, 32 + y + 1);
	} else {
		return;
	}

	ttywrite(buf, len);
}

void
bpress(XEvent *e)
{
	struct timespec now;
	const MouseShortcut *ms;

	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forceselmod)) {
		mousereport(e);
		return;
	}

	for (ms = mshortcuts; ms < mshortcuts + LEN(mshortcuts); ms++) {
		if (e->xbutton.button == ms->b &&
		    modifiers_match(ms->mask, e->xbutton.state)) {
			ttysend(ms->s, strlen(ms->s));
			return;
		}
	}

	if (e->xbutton.button == Button1) {
		clock_gettime(CLOCK_MONOTONIC, &now);

		// Clear previous selection, logically and visually.
		selclear(NULL);
		sel.mode = SEL_EMPTY;
		sel.type = SEL_REGULAR;
		sel.oe.x = sel.ob.x = x2col(e->xbutton.x);
		sel.oe.y = sel.ob.y = y2row(e->xbutton.y);

		/*
		 * If the user clicks below predefined timeouts specific
		 * snapping behaviour is exposed.
		 */
		if (TIMEDIFF(now, sel.tclick2) <= tripleclicktimeout) {
			sel.snap = SNAP_LINE;
		} else if (TIMEDIFF(now, sel.tclick1) <= doubleclicktimeout) {
			sel.snap = SNAP_WORD;
		} else {
			sel.snap = 0;
		}
		selnormalize();

		if (sel.snap != 0) {
			sel.mode = SEL_READY;
		}
		tsetdirt(sel.nb.y, sel.ne.y);
		sel.tclick2 = sel.tclick1;
		sel.tclick1 = now;
	}
}

char *
getsel(void)
{
	char *str, *ptr;
	int y, bufsize, lastx, linelen;
	Glyph *gp, *last;

	if (sel.ob.x == -1) {
		return NULL;
	}

	bufsize = (term.col + 1) * (sel.ne.y - sel.nb.y + 1) * max_utf8_bytes;
	ptr = str = (char *)xmalloc(bufsize);

	// append every set & selected glyph to the selection
	for (y = sel.nb.y; y <= sel.ne.y; y++) {
		if ((linelen = tlinelen(y)) == 0) {
			*ptr++ = '\n';
			continue;
		}

		if (sel.type == SEL_RECTANGULAR) {
			gp = &term.line[y][sel.nb.x];
			lastx = sel.ne.x;
		} else {
			gp = &term.line[y][sel.nb.y == y ? sel.nb.x : 0];
			lastx = (sel.ne.y == y) ? sel.ne.x : term.col - 1;
		}
		last = &term.line[y][MIN(lastx, linelen - 1)];
		while (last >= gp && last->u == ' ') {
			--last;
		}

		for (; gp <= last; ++gp) {
			if (gp->mode & ATTR_WDUMMY) {
				continue;
			}
			ptr += utf8encode(gp->u, ptr);
		}

		/*
		 * Copy and pasting of line endings is inconsistent
		 * in the inconsistent terminal and GUI world.
		 * The best solution seems like to produce '\n' when
		 * something is copied from st and convert '\n' to
		 * '\r', when something to be pasted is received by
		 * st.
		 * FIXME: Fix the computer world.
		 */
		if ((y < sel.ne.y || lastx >= linelen) &&
		    !(last->mode & ATTR_WRAP)) {
			*ptr++ = '\n';
		}
	}
	*ptr = 0;
	return str;
}

void
selcopy(Time t)
{
	xsetsel(getsel(), False, t);
}

void
propnotify(XEvent *e)
{
	XPropertyEvent *xpev = &e->xproperty;
	if (xpev->state == PropertyNewValue &&
	    (xpev->atom == XA_PRIMARY || xpev->atom == XA_CLIPBOARD)) {
		selnotify(e);
	}
}

void
selnotify(XEvent *e)
{
	ulong nitems_return, long_offset, bytes_after_return;
	int actual_format_return;
	uchar *prop_return, *last, *repl;
	Atom actual_type_return, incratom, property;

	incratom = XInternAtom(xw.dpy, "INCR", 0);

	long_offset = 0;
	if (e->type == SelectionNotify) {
		property = e->xselection.property;
	} else if (e->type == PropertyNotify) {
		property = e->xproperty.atom;
	} else {
		return;
	}
	if (property == None) {
		return;
	}

	do {
		if (XGetWindowProperty(xw.dpy, xw.win, property, long_offset,
		                       BUFSIZ / 4, False, AnyPropertyType,
		                       &actual_type_return,
		                       &actual_format_return, &nitems_return,
		                       &bytes_after_return, &prop_return)) {
			fprintf(stderr, "Clipboard allocation failed\n");
			return;
		}

		if (e->type == PropertyNotify && nitems_return == 0 &&
		    bytes_after_return == 0) {
			/*
			 * If there is some PropertyNotify with no data, then
			 * this is the signal of the selection owner that all
			 * data has been transferred. We won't need to receive
			 * PropertyNotify events anymore.
			 */
			MODBIT(xw.attrs.event_mask, 0, PropertyChangeMask);
			XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask,
			                        &xw.attrs);
		}

		if (actual_type_return == incratom) {
			/*
			 * Activate the PropertyNotify events so we receive
			 * when the selection owner does send us the next
			 * chunk of data.
			 */
			MODBIT(xw.attrs.event_mask, 1, PropertyChangeMask);
			XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask,
			                        &xw.attrs);

			// Deleting the property is the transfer start signal.
			XDeleteProperty(xw.dpy, xw.win, property);
			continue;
		}

		/*
		 * As seen in getsel:
		 * Line endings are inconsistent in the terminal and GUI world
		 * copy and pasting. When receiving some selection data,
		 * replace all '\n' with '\r'.
		 * FIXME: Fix the computer world.
		 */
		repl = prop_return;
		last = prop_return + nitems_return * actual_format_return / 8;
		while ((repl = (uchar *)memchr(repl, '\n', last - repl))) {
			*repl++ = '\r';
		}

		if (IS_SET(MODE_BRCKTPASTE) && long_offset == 0) {
			ttywrite("\x1B[200~", 6);
		}
		ttysend((const char *)prop_return,
		        nitems_return * actual_format_return / 8);
		if (IS_SET(MODE_BRCKTPASTE) && bytes_after_return == 0) {
			ttywrite("\x1B[201~", 6);
		}
		XFree(prop_return);
		// number of 32-bit chunks returned
		long_offset += nitems_return * actual_format_return / 32;
	} while (bytes_after_return > 0);

	/*
	 * Deleting the property again tells the selection owner to send the
	 * next data chunk in the property.
	 */
	XDeleteProperty(xw.dpy, xw.win, (int)property);
}

void
selpaste(UNUSED int unused)
{
	XConvertSelection(xw.dpy, XA_PRIMARY, sel.xtarget, XA_PRIMARY, xw.win,
	                  CurrentTime);
}

void
clipcopy(UNUSED int unused)
{
	if (sel.clipboard != NULL) {
		free(sel.clipboard);
	}

	if (sel.primary != NULL) {
		sel.clipboard = xstrdup(sel.primary);
		XSetSelectionOwner(xw.dpy, XA_CLIPBOARD, xw.win, CurrentTime);
	}
}

void
clippaste(UNUSED int unused)
{
	XConvertSelection(xw.dpy, XA_CLIPBOARD, sel.xtarget, XA_CLIPBOARD,
	                  xw.win, CurrentTime);
}

void
selclear(UNUSED XEvent *unused)
{
	if (sel.ob.x == -1) {
		return;
	}
	sel.mode = SEL_IDLE;
	sel.ob.x = -1;
	tsetdirt(sel.nb.y, sel.ne.y);
}

void
selrequest(XEvent *e)
{
	XSelectionRequestEvent *xsre;
	XSelectionEvent xev;
	Atom xa_targets, string;
	char *seltext;

	xsre = (XSelectionRequestEvent *)e;
	xev.type = SelectionNotify;
	xev.requestor = xsre->requestor;
	xev.selection = xsre->selection;
	xev.target = xsre->target;
	xev.time = xsre->time;
	if (xsre->property == None) {
		xsre->property = xsre->target;
	}

	// reject
	xev.property = None;

	xa_targets = XInternAtom(xw.dpy, "TARGETS", 0);
	if (xsre->target == xa_targets) {
		// respond with the supported type
		string = sel.xtarget;
		XChangeProperty(xsre->display, xsre->requestor, xsre->property,
		                XA_ATOM, 32, PropModeReplace, (uchar *)&string,
		                1);
		xev.property = xsre->property;
	} else if (xsre->target == sel.xtarget || xsre->target == XA_STRING) {
		/*
		 * with XA_STRING non ascii characters may be incorrect in the
		 * requestor. It is not our problem, use utf8.
		 */
		if (xsre->selection == XA_PRIMARY) {
			seltext = sel.primary;
		} else if (xsre->selection == XA_CLIPBOARD) {
			seltext = sel.clipboard;
		} else {
			fprintf(stderr, "Unhandled clipboard selection 0x%lx\n",
			        xsre->selection);
			return;
		}
		if (seltext != NULL) {
			XChangeProperty(xsre->display, xsre->requestor,
			                xsre->property, xsre->target, 8,
			                PropModeReplace, (uchar *)seltext,
			                strlen(seltext));
			xev.property = xsre->property;
		}
	}

	// all done, send a notification to the listener
	if (!XSendEvent(xsre->display, xsre->requestor, 1, 0, (XEvent *)&xev)) {
		fprintf(stderr, "Error sending SelectionNotify event\n");
	}
}

void
xsetsel(char *str, Bool clipboard, Time t)
{
	Atom selection;
	free(sel.clipboard);
	free(sel.primary);
	if (clipboard) {
		selection = XA_CLIPBOARD;
		sel.clipboard = str;
		sel.primary = NULL;
	} else {
		selection = XA_PRIMARY;
		sel.clipboard = NULL;
		sel.primary = str;
	}

	XSetSelectionOwner(xw.dpy, selection, xw.win, t);
	if (XGetSelectionOwner(xw.dpy, selection) != xw.win) {
		selclear(NULL);
	}
}

void
brelease(XEvent *e)
{
	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forceselmod)) {
		mousereport(e);
		return;
	}

	if (e->xbutton.button == Button2) {
		selpaste(0);
	} else if (e->xbutton.button == Button1) {
		if (sel.mode == SEL_READY) {
			getbuttoninfo(e);
			selcopy(e->xbutton.time);
		} else {
			selclear(NULL);
		}
		sel.mode = SEL_IDLE;
		tsetdirt(sel.nb.y, sel.ne.y);
	}
}

void
bmotion(XEvent *e)
{
	int oldey, oldex, oldsby, oldsey;

	if (IS_SET(MODE_MOUSE) && !(e->xbutton.state & forceselmod)) {
		mousereport(e);
		return;
	}

	if (!sel.mode) {
		return;
	}

	sel.mode = SEL_READY;
	oldey = sel.oe.y;
	oldex = sel.oe.x;
	oldsby = sel.nb.y;
	oldsey = sel.ne.y;
	getbuttoninfo(e);

	if (oldey != sel.oe.y || oldex != sel.oe.x) {
		tsetdirt(MIN(sel.nb.y, oldsby), MAX(sel.ne.y, oldsey));
	}
}

NORETURN void
die(const char *errstr, ...)
{
	va_list ap;
	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	raise(SIGTRAP);
	exit(1);
}

void
execsh(void)
{
	const char **args, *sh, *prog;
	const struct passwd *pw;
	char buf[sizeof(long) * 8 + 1];

	errno = 0;
	if ((pw = getpwuid(getuid())) == NULL) {
		if (errno) {
			die("getpwuid:%s\n", strerror(errno));
		} else {
			die("who are you?\n");
		}
	}

	if ((sh = getenv("SHELL")) == NULL) {
		sh = (pw->pw_shell[0]) ? pw->pw_shell : shell;
	}

	if (opt_cmd) {
		prog = opt_cmd[0];
	} else if (utmp) {
		prog = utmp;
	} else {
		prog = sh;
	}
	args = (opt_cmd) ? opt_cmd : (const char *[]){prog, NULL};

	snprintf(buf, sizeof(buf), "%lu", xw.win);

	unsetenv("COLUMNS");
	unsetenv("LINES");
	unsetenv("TERMCAP");
	setenv("LOGNAME", pw->pw_name, 1);
	setenv("USER", pw->pw_name, 1);
	setenv("SHELL", sh, 1);
	setenv("HOME", pw->pw_dir, 1);
	setenv("TERM", termname, 1);
	setenv("WINDOWID", buf, 1);

	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGALRM, SIG_DFL);

	execvp(prog, (char *const *)args);
	exit(1);
}

void
sigchld_handler(UNUSED int unused)
{
	int stat;
	pid_t p = waitpid(pid, &stat, WNOHANG);
	if (p < 0) {
		die("Waiting for process ID %d failed: %s\n", pid,
		    strerror(errno));
	}
	if (pid != p) {
		return;
	}

	if (!WIFEXITED(stat) || WEXITSTATUS(stat)) {
		die("child finished with error '%d'\n", stat);
	}
	exit_with_code = 0;
}

void
sigsegv_handler(int sig)
{
	void *stack_entries[16];
	int size = backtrace(stack_entries, LEN(stack_entries));

	fprintf(stderr, "Exiting due to signal %d\nStack:\n", sig);
	backtrace_symbols_fd(stack_entries, size, STDERR_FILENO);
	exit(1);
}

void
stty(void)
{
	char cmd[_POSIX_ARG_MAX], *q;
	const char **p, *s;
	size_t n, siz;

	if ((n = strlen(stty_args)) > sizeof(cmd) - 1) {
		die("incorrect stty parameters\n");
	}
	memcpy(cmd, stty_args, n);
	q = cmd + n;
	siz = sizeof(cmd) - n;
	for (p = opt_cmd; p && (s = *p); ++p) {
		if ((n = strlen(s)) > siz - 1) {
			die("stty parameter length too long\n");
		}
		*q++ = ' ';
		memcpy(q, s, n);
		q += n;
		siz -= n + 1;
	}
	*q = 0;
	if (system(cmd) != 0) {
		perror("Couldn't call stty");
	}
}

void
ttynew(void)
{
	int m, s;
	struct winsize w = {term.row, term.col, 0, 0};

	if (opt_io) {
		term.mode |= MODE_PRINT;
		iofd = (strcmp(opt_io, "-") == 0)
		           ? 1
		           : open(opt_io, O_WRONLY | O_CREAT, 0666);
		if (iofd < 0) {
			fprintf(stderr, "Error opening %s:%s\n", opt_io,
			        strerror(errno));
		}
	}

	if (opt_line) {
		if ((cmdfd = open(opt_line, O_RDWR)) < 0) {
			die("open line failed: %s\n", strerror(errno));
		}
		dup2(cmdfd, 0);
		stty();
		return;
	}

	// seems to work fine on linux, openbsd and freebsd
	if (openpty(&m, &s, NULL, NULL, &w) < 0) {
		die("openpty failed: %s\n", strerror(errno));
	}

	switch (pid = fork()) {
	case -1:
		die("fork failed\n");
		break;
	case 0:  // We're the child process.
		close(iofd);
		setsid();  // create a new process group
		dup2(s, 0);
		dup2(s, 1);
		dup2(s, 2);
		if (ioctl(s, TIOCSCTTY, NULL) < 0) {
			die("ioctl TIOCSCTTY failed: %s\n", strerror(errno));
		}
		close(s);
		close(m);
		execsh();
		break;
	default:  // We're the parent process.
		close(s);
		cmdfd = m;
		signal(SIGCHLD, sigchld_handler);
		break;
	}
}

size_t
ttyread(void)
{
	static char buf[BUFSIZ];
	static int buflen = 0;
	char *ptr;
	int charsize;  // size of utf8 char in bytes
	Rune unicodep;
	int ret;

	// append read bytes to unprocessed bytes
	if ((ret = read(cmdfd, buf + buflen, LEN(buf) - buflen)) < 0) {
		// The process is probably done.
		// TODO(townba): Check errno.
		die("Couldn't read from shell: %s\n", strerror(errno));
	}

	buflen += ret;
	ptr = buf;

	for (;;) {
		if (IS_SET(MODE_UTF8)) {
			if (buflen == 0) {
				break;
			}
			if ((c1utf8_as & C1UTF8_AS_BYTE) &&
			    ISCONTROLC1((uchar)(ptr[0]))) {
				// We aren't able to decode as UTF-8 because
				// it's actually a control code.
				unicodep = ptr[0] & 0xFF;
				charsize = 1;
			} else {
				// process a complete utf8 char
				charsize = utf8decode(ptr, buflen, &unicodep);
				if (charsize == 0) {
					break;
				}
				if (!(c1utf8_as & C1UTF8_AS_UTF8) &&
				    ISCONTROLC1(unicodep)) {
					// We don't accept C1 controls in UTF-8
					// form.
					unicodep = replacement_rune;
				}
			}
			tputc(unicodep);
			ptr += charsize;
			buflen -= charsize;
		} else {
			if (buflen <= 0) {
				break;
			}
			tputc(*ptr++ & 0xFF);
			buflen--;
		}
	}
	// keep any uncomplete utf8 char for the next call
	if (buflen > 0) {
		memmove(buf, ptr, buflen);
	}

	return ret;
}

void
ttywrite(const char *s, size_t n)
{
	fd_set wfd, rfd;
	ssize_t r;
	size_t lim = 256;

	/*
	 * Remember that we are using a pty, which might be a modem line.
	 * Writing too much will clog the line. That's why we are doing this
	 * dance.
	 * FIXME: Migrate the world to Plan 9.
	 */
	while (n > 0) {
		FD_ZERO(&wfd);
		FD_ZERO(&rfd);
		FD_SET(cmdfd, &wfd);
		FD_SET(cmdfd, &rfd);

		// Check if we can write.
		if (pselect(cmdfd + 1, &rfd, &wfd, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) {
				continue;
			}
			die("select failed: %s\n", strerror(errno));
		}
		if (FD_ISSET(cmdfd, &wfd)) {
			/*
			 * Only write the bytes written by ttywrite() or the
			 * default of 256. This seems to be a reasonable value
			 * for a serial line. Bigger values might clog the I/O.
			 */
			if ((r = write(cmdfd, s, (n < lim) ? n : lim)) < 0) {
				goto write_error;
			}
			if ((size_t)(r) < n) {
				/*
				 * We weren't able to write out everything.
				 * This means the buffer is getting full
				 * again. Empty it.
				 */
				if (n < lim) {
					lim = ttyread();
				}
				n -= r;
				s += r;
			} else {
				// All bytes have been written.
				break;
			}
		}
		if (FD_ISSET(cmdfd, &rfd)) {
			lim = ttyread();
		}
	}
	return;

write_error:
	die("write error on tty: %s\n", strerror(errno));
}

void
ttysend(const char *s, size_t n)
{
	int len;
	const char *t, *lim;
	Rune u;

	ttywrite(s, n);
	if (!IS_SET(MODE_ECHO)) {
		return;
	}

	lim = &s[n];
	for (t = s; t < lim; t += len) {
		if (IS_SET(MODE_UTF8)) {
			len = utf8decode(t, n, &u);
		} else {
			u = *t & 0xFF;
			len = 1;
		}
		if (len <= 0) {
			break;
		}
		techo(u);
		n -= len;
	}
}

void
ttyresize(void)
{
	struct winsize w;

	w.ws_row = term.row;
	w.ws_col = term.col;
	w.ws_xpixel = xw.tw;
	w.ws_ypixel = xw.th;
	if (ioctl(cmdfd, TIOCSWINSZ, &w) < 0) {
		fprintf(stderr, "Couldn't set window size: %s\n",
		        strerror(errno));
	}
}

int
tattrset(int attr)
{
	int i, j;

	for (i = 0; i < term.row - 1; i++) {
		for (j = 0; j < term.col - 1; j++) {
			if (term.line[i][j].mode & attr) {
				return 1;
			}
		}
	}

	return 0;
}

void
tsetdirt(int top, int bot)
{
	int i;

	LIMIT(top, 0, term.row - 1);
	LIMIT(bot, 0, term.row - 1);

	for (i = top; i <= bot; i++) {
		term.dirty[i] = 1;
	}
}

void
tsetdirtattr(int attr)
{
	int i, j;

	for (i = 0; i < term.row - 1; i++) {
		for (j = 0; j < term.col - 1; j++) {
			if (term.line[i][j].mode & attr) {
				tsetdirt(i, i);
				break;
			}
		}
	}
}

void
tfulldirt(void)
{
	tsetdirt(0, term.row - 1);
}

void
tcursor(enum cursor_movement mode)
{
	static TCursor c[2];
	int alt = IS_SET(MODE_ALTSCREEN);

	if (mode == CURSOR_SAVE) {
		c[alt] = term.c;
	} else if (mode == CURSOR_LOAD) {
		term.c = c[alt];
		tmoveto(c[alt].x, c[alt].y);
	}
}

void
treset(void)
{
	uint i;

	term.c =
	    (TCursor){{.mode = ATTR_NULL, .fg = defaultfg, .bg = defaultbg},
	              .x = 0,
	              .y = 0,
	              .state = CURSOR_DEFAULT};

	memset(term.tabs, 0, term.col * sizeof(*term.tabs));
	for (i = tabspaces; i < term.col; i += tabspaces) {
		term.tabs[i] = 1;
	}
	term.top = 0;
	term.bot = term.row - 1;
	term.mode = MODE_WRAP | MODE_UTF8;
	assert(LEN(term.trantbl) == 4);
	term.trantbl[0] = CS_US_ASCII;
	term.trantbl[1] = CS_US_ASCII;
	term.trantbl[2] = CS_SPECIAL_GRAPHIC;
	term.trantbl[3] = CS_SPECIAL_GRAPHIC;
	term.charset = 0;

	for (i = 0; i < 2; i++) {
		tmoveto(0, 0);
		tcursor(CURSOR_SAVE);
		tclearregion(0, 0, term.col - 1, term.row - 1);
		tswapscreen();
	}
}

void
tnew(int col, int row)
{
	term = (Term){.c = {.attr = {.fg = defaultfg, .bg = defaultbg}}};
	tresize(col, row);

	treset();
}

void
tswapscreen(void)
{
	Line *tmp = term.line;

	term.line = term.alt;
	term.alt = tmp;
	term.mode ^= MODE_ALTSCREEN;
	tfulldirt();
}

void
tscrolldown(int orig, int n)
{
	int i;
	Line temp;

	LIMIT(n, 0, term.bot - orig + 1);

	tsetdirt(orig, term.bot - n);
	tclearregion(0, term.bot - n + 1, term.col - 1, term.bot);

	for (i = term.bot; i >= orig + n; i--) {
		temp = term.line[i];
		term.line[i] = term.line[i - n];
		term.line[i - n] = temp;
	}

	selscroll(orig, n);
}

void
tscrollup(int orig, int n)
{
	int i;
	Line temp;

	LIMIT(n, 0, term.bot - orig + 1);

	tclearregion(0, orig, term.col - 1, orig + n - 1);
	tsetdirt(orig + n, term.bot);

	for (i = orig; i <= term.bot - n; i++) {
		temp = term.line[i];
		term.line[i] = term.line[i + n];
		term.line[i + n] = temp;
	}

	selscroll(orig, -n);
}

void
selscroll(int orig, int n)
{
	if (sel.ob.x == -1) {
		return;
	}

	if (BETWEEN(sel.ob.y, orig, term.bot) ||
	    BETWEEN(sel.oe.y, orig, term.bot)) {
		if ((sel.ob.y += n) > term.bot || (sel.oe.y += n) < term.top) {
			selclear(NULL);
			return;
		}
		if (sel.type == SEL_RECTANGULAR) {
			if (sel.ob.y < term.top) {
				sel.ob.y = term.top;
			}
			if (sel.oe.y > term.bot) {
				sel.oe.y = term.bot;
			}
		} else {
			if (sel.ob.y < term.top) {
				sel.ob.y = term.top;
				sel.ob.x = 0;
			}
			if (sel.oe.y > term.bot) {
				sel.oe.y = term.bot;
				sel.oe.x = term.col;
			}
		}
		selnormalize();
	}
}

void
tnewline(int first_col)
{
	int y = term.c.y;

	if (y == term.bot) {
		tscrollup(term.top, 1);
	} else {
		y++;
	}
	tmoveto(first_col ? 0 : term.c.x, y);
}

void
csiparse(void)
{
	char *p = csiescseq.buf, *np;
	long int v;

	csiescseq.narg = 0;
	if (BETWEEN(*p, 0x3C, 0x3F)) {
		csiescseq.interm = *p;
		p++;
	}

	csiescseq.buf[csiescseq.len] = 0;
	while (p < csiescseq.buf + csiescseq.len) {
		np = NULL;
		v = strtol(p, &np, 10);
		if (np == p) {
			v = 0;
		}
		if (v == LONG_MAX || v == LONG_MIN) {
			v = -1;
		}
		csiescseq.arg[csiescseq.narg++] = v;
		p = np;
		if (*p != ';' || csiescseq.narg == ESC_ARG_SIZ) {
			break;
		}
		p++;
	}
	csiescseq.mode[0] = *p++;
	csiescseq.mode[1] = (p < csiescseq.buf + csiescseq.len) ? *p : 0;
}

// for absolute user moves, when decom is set
void
tmoveato(int x, int y)
{
	tmoveto(x, y + ((term.c.state & CURSOR_ORIGIN) ? term.top : 0));
}

void
tmoveto(int x, int y)
{
	int miny, maxy;

	if (term.c.state & CURSOR_ORIGIN) {
		miny = term.top;
		maxy = term.bot;
	} else {
		miny = 0;
		maxy = term.row - 1;
	}
	term.c.state &= ~CURSOR_WRAPNEXT;
	term.c.x = LIMIT(x, 0, term.col - 1);
	term.c.y = LIMIT(y, miny, maxy);
}

void
tsetchar(Rune u, const Glyph *attr, int x, int y)
{
	static struct {
		enum charset charset;
		const Rune table[96];
	} charset_to_table[] = {
	    {CS_SPECIAL_GRAPHIC,
	     {0,      0,      0,      0,      0,      0,      0,      0,
	      0,      0,      0,      0,      0,      0,      0,      0,
	      0,      0,      0,      0,      0,      0,      0,      0,
	      0,      0,      0,      0,      0,      0,      0,      0,
	      0,      0,      0,      0,      0,      0,      0,      0,
	      0,      0,      0,      0,      0,      0,      0,      0,
	      0,      0,      0,      0,      0,      0,      0,      0,
	      0,      0,      0,      0,      0,      0,      0,      ' ',
	      0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0xB0,   0xB1,
	      0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C, 0x23BA,
	      0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534, 0x252C,
	      0x2502, 0x2264, 0x2265, 0x3C0,  0x2260, 0xA3,   0xB7,   0}},
	    {CS_TECHNICAL,
	     // NOTE: 0x2B to 0x2E had been 0x239B, 0x239D, 0x239E, and 0x23A0,
	     // but the new code points look closer to the original. 0x44 had
	     // been 0x0394, but 0x2206 makes more sense.
	     {0,      0x23B7, 0x250C, 0x2500, 0x2320, 0x2321, 0x2502, 0x23A1,
	      0x23A3, 0x23A4, 0x23A6, 0x23A7, 0x23A9, 0x23AB, 0x23AD, 0x23A8,
	      0x23AC, '<',    '<',    0x2572, 0x2571, '-',    '-',    '>',
	      0x2426, 0x2426, 0x2426, 0x2426, 0x2264, 0x2260, 0x2265, 0x222B,
	      0x2234, 0x221D, 0x221E, 0xF7,   0x2206, 0x2207, 0x3A6,  0x393,
	      0x223C, 0x2243, 0x398,  0xD7,   0x39B,  0x21D4, 0x21D2, 0x2261,
	      0x3A0,  0x3A8,  0x2426, 0x3A3,  0x2426, 0x2426, 0x221A, 0x3A9,
	      0x39E,  0x3A5,  0x2282, 0x2283, 0x2229, 0x222A, 0x2227, 0x2228,
	      0xAC,   0x3B1,  0x3B2,  0x3C7,  0x3B4,  0x3B5,  0x3C6,  0x3B3,
	      0x3B7,  0x3B9,  0x3B8,  0x3BA,  0x3BB,  0x2426, 0x3BD,  0x2202,
	      0x3C0,  0x3C8,  0x3C1,  0x3C3,  0x3C4,  0x2426, 0x192,  0x3C9,
	      0x3BE,  0x3C5,  0x3B6,  0x2190, 0x2191, 0x2192, 0x2193, 0}},
	    {CS_CURSES,
	     // These are similar to special_graphics but with previously
	     // unchanged characters changed to either blanks or new characters,
	     // particularly for use in curses.
	     {0,      ' ',    ' ',    0x25A0, 0xA7,   ' ',    0x2603, ' ',
	      ' ',    ' ',    ' ',    0x2192, 0x2190, 0x2191, 0x2193, ' ',
	      0x2588, ' ',    ' ',    ' ',    ' ',    ' ',    ' ',    ' ',
	      ' ',    ' ',    ' ',    ' ',    0x2591, ' ',    0x2593, ' ',
	      ' ',    0x255D, 0x2557, 0x2554, 0x255A, 0x256C, 0x2560, 0x2563,
	      0x2569, 0x2566, 0x251B, 0x2513, 0x250F, 0x2517, 0x254B, ' ',
	      ' ',    0x2501, 0x2550, ' ',    0x2523, 0x252B, 0x253B, 0x2533,
	      0x2503, 0x2551, ' ',    ' ',    ' ',    ' ',    ' ',    ' ',
	      0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0xB0,   0xB1,
	      0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C, 0x23BA,
	      0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534, 0x252C,
	      0x2502, 0x2264, 0x2265, 0x3C0,  0x2260, 0xA3,   0xB7,   0}},
	};

	const Rune *trantbl = NULL;
	for (size_t i = 0; i < LEN(charset_to_table); ++i) {
		if (term.trantbl[term.charset] == charset_to_table[i].charset) {
			trantbl = charset_to_table[i].table;
		}
	}
	if (trantbl && BETWEEN(u, ' ', 0x7F) && trantbl[u - ' ']) {
		u = trantbl[u - ' '];
	}

	if (term.line[y][x].mode & ATTR_WIDE) {
		if (x + 1 < term.col) {
			term.line[y][x + 1].u = ' ';
			term.line[y][x + 1].mode &= ~ATTR_WDUMMY;
		}
	} else if (term.line[y][x].mode & ATTR_WDUMMY) {
		term.line[y][x - 1].u = ' ';
		term.line[y][x - 1].mode &= ~ATTR_WIDE;
	}

	term.dirty[y] = 1;
	term.line[y][x] = *attr;
	term.line[y][x].u = u;
}

void
tclearregion(int x1, int y1, int x2, int y2)
{
	int x, y, temp;
	Glyph *gp;

	if (x1 > x2) {
		temp = x1, x1 = x2, x2 = temp;
	}
	if (y1 > y2) {
		temp = y1, y1 = y2, y2 = temp;
	}

	LIMIT(x1, 0, term.col - 1);
	LIMIT(x2, 0, term.col - 1);
	LIMIT(y1, 0, term.row - 1);
	LIMIT(y2, 0, term.row - 1);

	for (y = y1; y <= y2; y++) {
		term.dirty[y] = 1;
		for (x = x1; x <= x2; x++) {
			gp = &term.line[y][x];
			if (selected(x, y)) {
				selclear(NULL);
			}
			gp->fg = term.c.attr.fg;
			gp->bg = term.c.attr.bg;
			gp->mode = 0;
			gp->u = ' ';
		}
	}
}

void
tdeletechar(int n)
{
	int dst, src, size;
	Glyph *line;

	LIMIT(n, 0, term.col - term.c.x);

	dst = term.c.x;
	src = term.c.x + n;
	size = term.col - src;
	line = term.line[term.c.y];

	memmove(&line[dst], &line[src], size * sizeof(Glyph));
	tclearregion(term.col - n, term.c.y, term.col - 1, term.c.y);
}

void
tinsertblank(int n)
{
	int dst, src, size;
	Glyph *line;

	LIMIT(n, 0, term.col - term.c.x);

	dst = term.c.x + n;
	src = term.c.x;
	size = term.col - dst;
	line = term.line[term.c.y];

	memmove(&line[dst], &line[src], size * sizeof(Glyph));
	tclearregion(src, term.c.y, dst - 1, term.c.y);
}

void
tinsertblankline(int n)
{
	if (BETWEEN(term.c.y, term.top, term.bot)) {
		tscrolldown(term.c.y, n);
	}
}

void
tdeleteline(int n)
{
	if (BETWEEN(term.c.y, term.top, term.bot)) {
		tscrollup(term.c.y, n);
	}
}

int32_t
tdefcolor(const int *attr, int *npar, int l)
{
	int32_t idx = -1;
	uint r, g, b;

	switch (attr[*npar + 1]) {
	case 2:  // direct color in RGB space
		if (*npar + 4 >= l) {
			fprintf(
			    stderr,
			    "erresc(38): Incorrect number of parameters (%d)\n",
			    *npar);
			break;
		}
		r = attr[*npar + 2];
		g = attr[*npar + 3];
		b = attr[*npar + 4];
		*npar += 4;
		if (!BETWEEN(r, 0, 255) || !BETWEEN(g, 0, 255) ||
		    !BETWEEN(b, 0, 255)) {
			fprintf(stderr, "erresc: bad rgb color (%u,%u,%u)\n", r,
			        g, b);
		} else {
			idx = TRUECOLOR(r, g, b);
		}
		break;
	case 5:  // indexed color
		if (*npar + 2 >= l) {
			fprintf(
			    stderr,
			    "erresc(38): Incorrect number of parameters (%d)\n",
			    *npar);
			break;
		}
		*npar += 2;
		if (!BETWEEN(attr[*npar], 0, 255)) {
			fprintf(stderr, "erresc: bad fgcolor %d\n",
			        attr[*npar]);
		} else {
			idx = attr[*npar];
		}
		break;
	case 0:  // implemented defined (only foreground)
	case 1:  // transparent
	case 3:  // direct color in CMY space
	case 4:  // direct color in CMYK space
	default:
		fprintf(stderr, "erresc(38): gfx attr %d unknown\n",
		        attr[*npar]);
		break;
	}

	return idx;
}

void
tsetattr(int *attr, int l)
{
	int i;
	int32_t idx;

	for (i = 0; i < l; i++) {
		switch (attr[i]) {
		case 0:
			term.c.attr.mode &=
			    ~(ATTR_BOLD | ATTR_FAINT | ATTR_ITALIC |
			      ATTR_UNDERLINE | ATTR_BLINK | ATTR_REVERSE |
			      ATTR_INVISIBLE | ATTR_STRUCK);
			term.c.attr.fg = defaultfg;
			term.c.attr.bg = defaultbg;
			break;
		case 1:
			term.c.attr.mode |= ATTR_BOLD;
			break;
		case 2:
			term.c.attr.mode |= ATTR_FAINT;
			break;
		case 3:
			term.c.attr.mode |= ATTR_ITALIC;
			break;
		case 4:
			term.c.attr.mode |= ATTR_UNDERLINE;
			break;
		case 5:  // slow blink
		// FALLTHROUGH
		case 6:  // rapid blink
			term.c.attr.mode |= ATTR_BLINK;
			break;
		case 7:
			term.c.attr.mode |= ATTR_REVERSE;
			break;
		case 8:
			term.c.attr.mode |= ATTR_INVISIBLE;
			break;
		case 9:
			term.c.attr.mode |= ATTR_STRUCK;
			break;
		case 22:
			term.c.attr.mode &= ~(ATTR_BOLD | ATTR_FAINT);
			break;
		case 23:
			term.c.attr.mode &= ~ATTR_ITALIC;
			break;
		case 24:
			term.c.attr.mode &= ~ATTR_UNDERLINE;
			break;
		case 25:
			term.c.attr.mode &= ~ATTR_BLINK;
			break;
		case 27:
			term.c.attr.mode &= ~ATTR_REVERSE;
			break;
		case 28:
			term.c.attr.mode &= ~ATTR_INVISIBLE;
			break;
		case 29:
			term.c.attr.mode &= ~ATTR_STRUCK;
			break;
		case 38:
			if ((idx = tdefcolor(attr, &i, l)) >= 0) {
				term.c.attr.fg = idx;
			}
			break;
		case 39:
			term.c.attr.fg = defaultfg;
			break;
		case 48:
			if ((idx = tdefcolor(attr, &i, l)) >= 0) {
				term.c.attr.bg = idx;
			}
			break;
		case 49:
			term.c.attr.bg = defaultbg;
			break;
		default:
			if (BETWEEN(attr[i], 30, 37)) {
				term.c.attr.fg = attr[i] - 30;
			} else if (BETWEEN(attr[i], 40, 47)) {
				term.c.attr.bg = attr[i] - 40;
			} else if (BETWEEN(attr[i], 90, 97)) {
				term.c.attr.fg = attr[i] - 90 + 8;
			} else if (BETWEEN(attr[i], 100, 107)) {
				term.c.attr.bg = attr[i] - 100 + 8;
			} else {
				fprintf(
				    stderr,
				    "erresc(default): gfx attr %d unknown\n",
				    attr[i]),
				    csidump();
			}
			break;
		}
	}
}

void
tsetscroll(int t, int b)
{
	int temp;

	LIMIT(t, 0, term.row - 1);
	LIMIT(b, 0, term.row - 1);
	if (t > b) {
		temp = t;
		t = b;
		b = temp;
	}
	term.top = t;
	term.bot = b;
}

void
tsetmode(char interm, int set, const int *args, int narg)
{
	const int *lim;
	int mode;
	int alt;

	for (lim = args + narg; args < lim; ++args) {
		switch (interm) {
		case '?':
			switch (*args) {
			case 1:  // DECCKM -- Cursor key
				MODBIT(term.mode, set, MODE_APPCURSOR);
				break;
			case 3:  // DECCOLM -- Column  (IGNORED)
				// Accept "unsetting" (selecting 80-column mode)
				// even when DECCOLM is off because that's
				// considered the default setting (even when
				// it's not really the default).
				if (!set || IS_SET(MODE_ENABLE_COLUMN_CHANGE)) {
					cresize((set ? 132 : 80) * xw.cw +
					            2 * borderpx,
					        0);
					ttyresize();
					XResizeWindow(xw.dpy, xw.win, xw.w,
					              xw.h);
					if (IS_SET(MODE_CLEAR_ON_DECCOLM)) {
						tclearregion(0, 0, term.col - 1,
						             term.row - 1);
					}
					tmoveto(0, 0);
				}
				break;
			case 5:  // DECSCNM -- Reverse video
				mode = term.mode;
				MODBIT(term.mode, set, MODE_REVERSE);
				if (mode != term.mode) {
					redraw();
				}
				break;
			case 6:  // DECOM -- Origin
				MODBIT(term.c.state, set, CURSOR_ORIGIN);
				tmoveato(0, 0);
				break;
			case 7:  // DECAWM -- Auto wrap
				MODBIT(term.mode, set, MODE_WRAP);
				break;
			case 0:   // Error (IGNORED)
			case 2:   // DECANM -- ANSI/VT52 (IGNORED)
			case 4:   // DECSCLM -- Scroll (IGNORED)
			case 8:   // DECARM -- Auto repeat (IGNORED)
			case 18:  // DECPFF -- Printer feed (IGNORED)
			case 19:  // DECPEX -- Printer extent (IGNORED)
			case 42:  // DECNRCM -- National characters (IGNORED)
			case 12:  // att610 -- Start blinking cursor (IGNORED)
				break;
			case 25:  // DECTCEM -- Text Cursor Enable Mode
				MODBIT(term.mode, !set, MODE_HIDE);
				break;
			case 40:  // Enable DECCOLM
				MODBIT(term.mode, set,
				       MODE_ENABLE_COLUMN_CHANGE);
				break;
			case 66:  // DECNKM -- Numeric Keypad Mode
				MODBIT(term.mode, set, MODE_APPKEYPAD);
				break;
			case 95:  // DECNCSM -- No Clearing Screen On Column
			          // Change Mode
				MODBIT(term.mode, !set, MODE_CLEAR_ON_DECCOLM);
				break;
			case 9:  // X10 mouse compatibility mode
				xsetpointermotion(0);
				MODBIT(term.mode, 0, MODE_MOUSE);
				MODBIT(term.mode, set, MODE_MOUSEX10);
				break;
			case 1000:  // 1000: report button press
				xsetpointermotion(0);
				MODBIT(term.mode, 0, MODE_MOUSE);
				MODBIT(term.mode, set, MODE_MOUSEBTN);
				break;
			case 1002:  // 1002: report motion on button press
				xsetpointermotion(0);
				MODBIT(term.mode, 0, MODE_MOUSE);
				MODBIT(term.mode, set, MODE_MOUSEMOTION);
				break;
			case 1003:  // 1003: enable all mouse motions
				xsetpointermotion(set);
				MODBIT(term.mode, 0, MODE_MOUSE);
				MODBIT(term.mode, set, MODE_MOUSEMANY);
				break;
			case 1004:  // 1004: send focus events to tty
				MODBIT(term.mode, set, MODE_FOCUS);
				break;
			case 1006:  // 1006: extended reporting mode
				MODBIT(term.mode, set, MODE_MOUSESGR);
				break;
			case 1034:
				MODBIT(term.mode, set, MODE_8BIT);
				break;
			case 1049:  // swap screen & set/restore cursor as xterm
				if (!opt_allowaltscreen) {
					break;
				}
				tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
			// FALLTHROUGH
			case 47:  // swap screen
			case 1047:
				if (!opt_allowaltscreen) {
					break;
				}
				alt = IS_SET(MODE_ALTSCREEN);
				if (alt) {
					tclearregion(0, 0, term.col - 1,
					             term.row - 1);
				}
				if (set ^ alt) {  // set is always 1 or 0
					tswapscreen();
				}
				if (*args != 1049) {
					break;
				}
			// FALLTHROUGH
			case 1048:
				tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
				break;
			case 2004:  // 2004: bracketed paste mode
				MODBIT(term.mode, set, MODE_BRCKTPASTE);
				break;
			// Not implemented mouse modes. See comments there.
			case 1001: /* mouse highlight mode; can hang the
			              terminal by design when implemented. */
			case 1005: /* UTF-8 mouse mode; will confuse
			              applications not supporting UTF-8
			              and luit. */
			case 1015: /* urxvt mangled mouse mode; incompatible
			              and can be mistaken for other control
			              codes. */
				if (!set) {
					break;
				}
				fprintf(stderr,
				        "erresc: mouse mode %d not supported\n",
				        *args);
				break;
			default:
				fprintf(stderr,
				        "erresc: unknown private %s mode %d\n",
				        set ? "set" : "reset", *args);
				break;
			}
			break;
		case 0:
			switch (*args) {
			case 0:  // Error (IGNORED)
				break;
			case 2:  // KAM -- keyboard action
				MODBIT(term.mode, set, MODE_KBDLOCK);
				break;
			case 4:  // IRM -- Insertion-replacement
				MODBIT(term.mode, set, MODE_INSERT);
				break;
			case 12:  // SRM -- Send/Receive
				MODBIT(term.mode, !set, MODE_ECHO);
				break;
			case 20:  // LNM -- Linefeed/new line
				MODBIT(term.mode, set, MODE_CRLF);
				break;
			case 34:  // Wyse underline cursor mode (IGNORED)
				break;
			default:
				fprintf(stderr,
				        "erresc: unknown set/reset mode %d\n",
				        *args);
				break;
			}
			break;
		}
	}
}

void
csihandle(void)
{
	char buf[40];
	int len;

	switch (csiescseq.interm) {
	case 0:
	case '?':
		switch (csiescseq.mode[0]) {
		default:
		unknown:
			fprintf(stderr, "erresc: unknown csi ");
			csidump();
			break;
		case '@':  // ICH -- Insert <n> blank char
			DEFAULT(csiescseq.arg[0], 1);
			tinsertblank(csiescseq.arg[0]);
			break;
		case 'A':  // CUU -- Cursor <n> Up
			DEFAULT(csiescseq.arg[0], 1);
			tmoveto(term.c.x, term.c.y - csiescseq.arg[0]);
			break;
		case 'B':  // CUD -- Cursor <n> Down
		case 'e':  // VPR --Cursor <n> Down
			DEFAULT(csiescseq.arg[0], 1);
			tmoveto(term.c.x, term.c.y + csiescseq.arg[0]);
			break;
		case 'i':  // MC -- Media Copy
			switch (csiescseq.arg[0]) {
			case 0:
				tdump();
				break;
			case 1:
				tdumpline(term.c.y);
				break;
			case 2:
				tdumpsel();
				break;
			case 4:
				term.mode &= ~MODE_PRINT;
				break;
			case 5:
				term.mode |= MODE_PRINT;
				break;
			}
			break;
		case 'c':  // DA -- Device Attributes
			if (csiescseq.arg[0] == 0) {
				ttywrite(da1_response, LEN(da1_response) - 1);
			}
			break;
		case 'C':  // CUF -- Cursor <n> Forward
		case 'a':  // HPR -- Cursor <n> Forward
			DEFAULT(csiescseq.arg[0], 1);
			tmoveto(term.c.x + csiescseq.arg[0], term.c.y);
			break;
		case 'D':  // CUB -- Cursor <n> Backward
			DEFAULT(csiescseq.arg[0], 1);
			tmoveto(term.c.x - csiescseq.arg[0], term.c.y);
			break;
		case 'E':  // CNL -- Cursor <n> Down and first col
			DEFAULT(csiescseq.arg[0], 1);
			tmoveto(0, term.c.y + csiescseq.arg[0]);
			break;
		case 'F':  // CPL -- Cursor <n> Up and first col
			DEFAULT(csiescseq.arg[0], 1);
			tmoveto(0, term.c.y - csiescseq.arg[0]);
			break;
		case 'g':  // TBC -- Tabulation clear
			switch (csiescseq.arg[0]) {
			case 0:  // clear current tab stop
				term.tabs[term.c.x] = 0;
				break;
			case 3:  // clear all the tabs
				memset(term.tabs, 0,
				       term.col * sizeof(*term.tabs));
				break;
			default:
				goto unknown;
			}
			break;
		case 'G':  // CHA -- Move to <col>
		case '`':  // HPA
			DEFAULT(csiescseq.arg[0], 1);
			tmoveto(csiescseq.arg[0] - 1, term.c.y);
			break;
		case 'H':  // CUP -- Move to <row> <col>
		case 'f':  // HVP
			DEFAULT(csiescseq.arg[0], 1);
			DEFAULT(csiescseq.arg[1], 1);
			tmoveato(csiescseq.arg[1] - 1, csiescseq.arg[0] - 1);
			break;
		case 'I':  // CHT -- Cursor Forward Tabulation <n> tab stops
			DEFAULT(csiescseq.arg[0], 1);
			tputtab(csiescseq.arg[0]);
			break;
		case 'J':  // ED -- Clear screen
			selclear(NULL);
			switch (csiescseq.arg[0]) {
			case 0:  // below
				tclearregion(term.c.x, term.c.y, term.col - 1,
				             term.c.y);
				if (term.c.y < term.row - 1) {
					tclearregion(0, term.c.y + 1,
					             term.col - 1,
					             term.row - 1);
				}
				break;
			case 1:  // above
				if (term.c.y > 1) {
					tclearregion(0, 0, term.col - 1,
					             term.c.y - 1);
				}
				tclearregion(0, term.c.y, term.c.x, term.c.y);
				break;
			case 2:  // all
				tclearregion(0, 0, term.col - 1, term.row - 1);
				break;
			default:
				goto unknown;
			}
			break;
		case 'K':  // EL -- Clear line
			switch (csiescseq.arg[0]) {
			case 0:  // right
				tclearregion(term.c.x, term.c.y, term.col - 1,
				             term.c.y);
				break;
			case 1:  // left
				tclearregion(0, term.c.y, term.c.x, term.c.y);
				break;
			case 2:  // all
				tclearregion(0, term.c.y, term.col - 1,
				             term.c.y);
				break;
			}
			break;
		case 'S':  // SU -- Scroll <n> line up
			DEFAULT(csiescseq.arg[0], 1);
			tscrollup(term.top, csiescseq.arg[0]);
			break;
		case 'T':  // SD -- Scroll <n> line down
			DEFAULT(csiescseq.arg[0], 1);
			tscrolldown(term.top, csiescseq.arg[0]);
			break;
		case 'L':  // IL -- Insert <n> blank lines
			DEFAULT(csiescseq.arg[0], 1);
			tinsertblankline(csiescseq.arg[0]);
			break;
		case 'l':  // RM -- Reset Mode
			tsetmode(csiescseq.interm, 0, csiescseq.arg,
			         csiescseq.narg);
			break;
		case 'M':  // DL -- Delete <n> lines
			DEFAULT(csiescseq.arg[0], 1);
			tdeleteline(csiescseq.arg[0]);
			break;
		case 'X':  // ECH -- Erase <n> char
			DEFAULT(csiescseq.arg[0], 1);
			tclearregion(term.c.x, term.c.y,
			             term.c.x + csiescseq.arg[0] - 1, term.c.y);
			break;
		case 'P':  // DCH -- Delete <n> char
			DEFAULT(csiescseq.arg[0], 1);
			tdeletechar(csiescseq.arg[0]);
			break;
		case 'Z':  // CBT -- Cursor Backward Tabulation <n> tab stops
			DEFAULT(csiescseq.arg[0], 1);
			tputtab(-csiescseq.arg[0]);
			break;
		case 'd':  // VPA -- Move to <row>
			DEFAULT(csiescseq.arg[0], 1);
			tmoveato(term.c.x, csiescseq.arg[0] - 1);
			break;
		case 'h':  // SM -- Set terminal mode
			tsetmode(csiescseq.interm, 1, csiescseq.arg,
			         csiescseq.narg);
			break;
		case 'm':  // SGR -- Terminal attribute (color)
			tsetattr(csiescseq.arg, csiescseq.narg);
			break;
		case 'n':  // DSR - Device Status Report (cursor position)
			if (csiescseq.arg[0] == 6) {
				len = snprintf(buf, sizeof(buf), "\x1B[%i;%iR",
				               term.c.y + 1, term.c.x + 1);
				ttywrite(buf, len);
			}
			break;
		case 'r':  // DECSTBM -- Set Scrolling Region
			if (csiescseq.interm) {
				goto unknown;
			} else {
				DEFAULT(csiescseq.arg[0], 1);
				DEFAULT(csiescseq.arg[1], term.row);
				tsetscroll(csiescseq.arg[0] - 1,
				           csiescseq.arg[1] - 1);
				tmoveato(0, 0);
			}
			break;
		case 's':  // DECSC -- Save cursor position (ANSI.SYS)
			tcursor(CURSOR_SAVE);
			break;
		case 'u':  // DECRC -- Restore cursor position (ANSI.SYS)
			tcursor(CURSOR_LOAD);
			break;
		case ' ':
			switch (csiescseq.mode[1]) {
			case 'q':  // DECSCUSR -- Set Cursor Style
				DEFAULT(csiescseq.arg[0], 1);
				if (!BETWEEN(csiescseq.arg[0], 0, 7)) {
					goto unknown;
				}
				xw.cursor = csiescseq.arg[0];
				break;
			default:
				goto unknown;
			}
			break;
		case '$':  // DECSCPP -- Select Columns Per Page
			switch (csiescseq.mode[1]) {
			case '|':
				if (IS_SET(MODE_ENABLE_COLUMN_CHANGE)) {
					DEFAULT(csiescseq.arg[0],
					        (int)opt_cols);
					cresize(csiescseq.arg[0] * xw.cw +
					            2 * borderpx,
					        0);
					ttyresize();
					XResizeWindow(xw.dpy, xw.win, xw.w,
					              xw.h);
				}
				break;
			case '~':  // DECSSDT -- Select Status Display (Line)
			           // Type
				switch (csiescseq.arg[0]) {
				case 0:  // No status line
				case 1:  // Indicator status line
					xresettitle();
					MODBIT(term.mode, 0,
					       MODE_WRITABLE_STATUS_LINE);
					break;
				case 2:  // Host-writable status line
					if (!IS_SET(
					        MODE_WRITABLE_STATUS_LINE)) {
						xsettitle("");
						MODBIT(
						    term.mode, 1,
						    MODE_WRITABLE_STATUS_LINE);
					}
					break;
				default:
					goto unknown;
				}

			default:
				goto unknown;
			}
			break;
		}
		break;
	case '>':
		switch (csiescseq.mode[0]) {
		case 'c':  // Send device attributes (secondary DA)
			if (csiescseq.arg[0] == 0) {
				ttywrite(da2_response,
				         sizeof(da2_response) - 1);
			}
			break;
		case 'm':  // Set/reset modify keys (IGNORED)
		case 'n':  // Disable modify keys (IGNORED)
			break;
		default:
			fprintf(stderr, "erresc: unknown csi ");
			csidump();
			break;
		}
		break;
	}
}

void
chardump(char c)
{
	if (isprint(c)) {
		putc(c, stderr);
	} else if (c == '\n') {
		fprintf(stderr, "\\n");
	} else if (c == '\r') {
		fprintf(stderr, "\\r");
	} else if (c == 0x1B) {
		fprintf(stderr, "\\e");
	} else {
		fprintf(stderr, "\\x%02x", c);
	}
}

void
csidump(void)
{
	size_t i;

	fprintf(stderr, "\\e[");
	for (i = 0; i < csiescseq.len; i++) {
		chardump(csiescseq.buf[i] & 0xFF);
	}
	putc('\n', stderr);
}

void
csireset(void)
{
	memset(&csiescseq, 0, sizeof(csiescseq));
}

void
strhandle(void)
{
	const char *p = NULL, *c = NULL;
	char *buf = NULL;
	int j, narg, par;
	size_t buflen;

	term.esc &= ~(ESC_STR_END | ESC_STR);
	strparse();
	par = (narg = strescseq.narg) ? atoi(strescseq.args[0]) : 0;

	switch (strescseq.type) {
	case ']':  // OSC -- Operating System Command
		switch (par) {
		case 0:
		case 1:
		case 2:
			if (IS_SET(MODE_WRITABLE_STATUS_LINE) && narg > 1) {
				xsettitle(strescseq.args[1]);
			}
			return;
		case 4:  // color set
			if (narg < 3) {
				break;
			}
			p = strescseq.args[2];
		// FALLTHROUGH
		case 104:  // color reset, here p = NULL
			j = (narg > 1) ? atoi(strescseq.args[1]) : -1;
			if (xsetcolorname(j, p)) {
				fprintf(stderr, "erresc: invalid color %s\n",
				        p);
			} else {
				/*
				 * TODO if defaultbg color is changed, borders
				 * are dirty
				 */
				redraw();
			}
			return;
		case 12:   // set cursor color (IGNORED)
		case 112:  // reset cursor color (IGNORED)
			return;
		case 52:
			if (narg < 3 || !defaultosc52[0]) {
				break;
			}
			c = strescseq.args[1];
			p = strescseq.args[2];
			if (strcmp(p, "?") == 0) {
				/* Pasting from the clipboard as a result of
				 * a control sequence is a security risk, so we
				 * always use an empty string.
				 */
				buflen = strlen(c) + 8;
				buf = (char *)xmalloc(buflen);
				buflen =
				    snprintf(buf, buflen, "\x1B]52;%s;\\", c);
				ttywrite(buf, buflen);
				free(buf);
				return;
			}
			if (base64decode(p, (uchar **)(&buf), &buflen) ==
			    strlen(p)) {
				for (*c || (c = defaultosc52); *c; c++) {
					switch (*c) {
					case 'c':
						xsetsel(xstrdup(buf), True,
						        CurrentTime);
						break;
					case 'p':
						xsetsel(xstrdup(buf), False,
						        CurrentTime);
						break;
					default:
						// Ignored.
						break;
					}
				}
			}
			free(buf);
			return;
		}
		break;
	case 'k':  // old title set compatibility
		xsettitle(strescseq.args[0]);
		return;
	case 'P':  // DCS -- Device Control String
		// TODO(townba): This might not be the best place to put this.
		if (strescseq.len > 0 && strcmp(strescseq.buf, "$q\"p") == 0) {
			static const char decscl_response[] =
			    "\x1BP65;1\"p\x1B\\";
			ttywrite(decscl_response, LEN(decscl_response) - 1);
		}
		term.mode |= ESC_DCS;
		return;
	case '_':  // APC -- Application Program Command
	case '^':  // PM -- Privacy Message
		return;
	}

	fprintf(stderr, "erresc: unknown str ");
	strdump();
}

void
strparse(void)
{
	int c;
	char *p = strescseq.buf;

	strescseq.narg = 0;
	strescseq.buf[strescseq.len] = 0;

	if (*p == 0) {
		return;
	}

	while (strescseq.narg < STR_ARG_SIZ) {
		strescseq.args[strescseq.narg++] = p;
		while ((c = *p) != ';' && c != 0) {
			++p;
		}
		if (c == 0) {
			return;
		}
		*p++ = 0;
	}
}

void
strdump(void)
{
	int i;
	uint c;

	fprintf(stderr, "\\e%c", strescseq.type);
	for (i = 0; i < strescseq.len; i++) {
		c = strescseq.buf[i] & 0xFF;
		if (c == 0) {
			putc('\n', stderr);
			return;
		}
		chardump(c);
	}
	fprintf(stderr, "\\e\\\n");
}

void
strreset(void)
{
	memset(&strescseq, 0, sizeof(strescseq));
}

void
sendbreak(UNUSED int unused)
{
	if (tcsendbreak(cmdfd, 0)) {
		perror("Error sending break");
	}
}

void
reset(UNUSED int unused)
{
	treset();
}

void
tprinter(const char *s, size_t len)
{
	if (iofd != -1 && xwrite(iofd, s, len) < 0) {
		fprintf(stderr, "Error writing in %s:%s\n", opt_io,
		        strerror(errno));
		close(iofd);
		iofd = -1;
	}
}

void
iso14755(UNUSED int unused)
{
	FILE *p = popen(opt_iso14755_cmd, "r");
	if (!p) {
		return;
	}

	char codepoint[9];
	char *us = fgets(codepoint, sizeof(codepoint), p);
	pclose(p);

	unsigned long utf32 = 0;
	char *e = 0;
	if (!us || (utf32 = strtoul(us, &e, 16)) == ULONG_MAX ||
	    (*e && *e != '\n')) {
		return;
	}

	char uc[max_utf8_bytes];
	ttysend(uc, utf8encode(utf32, uc));
}

void
toggleprinter(UNUSED int unused)
{
	term.mode ^= MODE_PRINT;
}

void
printscreen(UNUSED int unused)
{
	tdump();
}

void
printsel(UNUSED int unused)
{
	tdumpsel();
}

void
tdumpsel(void)
{
	char *ptr;

	if ((ptr = getsel())) {
		tprinter(ptr, strlen(ptr));
		free(ptr);
	}
}

void
tdumpline(int n)
{
	char buf[max_utf8_bytes];
	Glyph *bp, *end;

	bp = &term.line[n][0];
	end = &bp[MIN(tlinelen(n), term.col) - 1];
	if (bp != end || bp->u != ' ') {
		for (; bp <= end; ++bp) {
			tprinter(buf, utf8encode(bp->u, buf));
		}
	}
	tprinter("\n", 1);
}

void
tdump(void)
{
	int i;

	for (i = 0; i < term.row; ++i) {
		tdumpline(i);
	}
}

void
tputtab(int n)
{
	uint x = term.c.x;

	if (n > 0) {
		while (x < term.col && n--) {
			for (++x; x < term.col && !term.tabs[x]; ++x) {
			}
		}
	} else if (n < 0) {
		while (x > 0 && n++) {
			for (--x; x > 0 && !term.tabs[x]; --x) {
			}
		}
	}
	term.c.x = LIMIT(x, 0, (uint)(term.col - 1));
}

void
techo(Rune u)
{
	if (ISCONTROL(u)) {  // control code
		if (u & 0x80) {
			u &= 0x7F;
			tputc('^');
			tputc('[');
		} else if (u != '\n' && u != '\r' && u != '\t') {
			u ^= 0x40;
			tputc('^');
		}
	}
	tputc(u);
}

void
tdefutf8(char ascii)
{
	if (ascii == 'G') {
		term.mode |= MODE_UTF8;
	} else if (ascii == '@') {
		term.mode &= ~MODE_UTF8;
	}
}

void
tdeftran(char ascii)
{
	static char cs[] = "0>Bc";
	static int vcs[] = {CS_SPECIAL_GRAPHIC, CS_TECHNICAL, CS_US_ASCII,
	                    CS_CURSES};
	char *p;

	if ((p = strchr(cs, ascii)) == NULL) {
		fprintf(stderr, "esc unhandled charset: ESC ( %c\n", ascii);
	} else {
		term.trantbl[term.icharset] = vcs[p - cs];
	}
}

void
tdectest(char c)
{
	int x, y;

	if (c == '8') {  // DEC screen alignment test.
		for (x = 0; x < term.col; ++x) {
			for (y = 0; y < term.row; ++y) {
				tsetchar('E', &term.c.attr, x, y);
			}
		}
	}
}

void
tstrsequence(uchar c)
{
	strreset();

	switch (c) {
	case 0x90:  // DCS -- Device Control String
		c = 'P';
		term.esc |= ESC_DCS;
		break;
	case 0x9F:  // APC -- Application Program Command
		c = '_';
		break;
	case 0x9E:  // PM -- Privacy Message
		c = '^';
		break;
	case 0x9D:  // OSC -- Operating System Command
		c = ']';
		break;
	}
	strescseq.type = c;
	term.esc |= ESC_STR;
}

void
tcontrolcode(uchar ascii)
{
	switch (ascii) {
	case '\t':  // HT
		tputtab(1);
		return;
	case '\b':  // BS
		tmoveto(term.c.x - 1, term.c.y);
		return;
	case '\r':  // CR
		tmoveto(0, term.c.y);
		return;
	case '\f':  // LF
	case '\v':  // VT
	case '\n':  // LF
		// go to first col if the mode is set
		tnewline(IS_SET(MODE_CRLF));
		return;
	case '\a':  // BEL
		if (term.esc & ESC_STR_END) {
			// backwards compatibility to xterm
			strhandle();
		} else {
			if (!(xw.state & WIN_FOCUSED)) {
				xseturgency(1);
			}
			if (bellvolume) {
				XkbBell(xw.dpy, xw.win, bellvolume, (Atom)NULL);
			}
		}
		break;
	case 0x1B:  // ESC
		csireset();
		term.esc &= ~(ESC_CSI | ESC_ALTCHARSET | ESC_TEST);
		term.esc |= ESC_START;
		return;
	case 0x0E:  // SO (LS1 -- Locking shift 1)
	case 0x0F:  // SI (LS0 -- Locking shift 0)
		term.charset = 1 - (ascii - 0x0E);
		return;
	case 0x1A:  // SUB
		tsetchar('?', &term.c.attr, term.c.x, term.c.y);
	case 0x18:  // CAN
		csireset();
		break;
	case 0x05:  // ENQ (IGNORED)
	case 0x00:  // NUL (IGNORED)
	case 0x11:  // XON (IGNORED)
	case 0x13:  // XOFF (IGNORED)
	case 0x7F:  // DEL (IGNORED)
		return;
	case 0x80:  // TODO(townba): PAD
	case 0x81:  // TODO(townba): HOP
	case 0x82:  // TODO(townba): BPH
	case 0x83:  // TODO(townba): NBH
	case 0x84:  // TODO(townba): IND
		break;
	case 0x85:            // NEL -- Next line
		tnewline(1);  // always go to first col
		break;
	case 0x86:  // TODO(townba): SSA
	case 0x87:  // TODO(townba): ESA
		break;
	case 0x88:  // HTS -- Horizontal tab stop
		term.tabs[term.c.x] = 1;
		break;
	case 0x89:  // TODO(townba): HTJ
	case 0x8A:  // TODO(townba): VTS
	case 0x8B:  // TODO(townba): PLD
	case 0x8C:  // TODO(townba): PLU
	case 0x8D:  // TODO(townba): RI
	case 0x8E:  // TODO(townba): SS2
	case 0x8F:  // TODO(townba): SS3
	case 0x91:  // TODO(townba): PU1
	case 0x92:  // TODO(townba): PU2
	case 0x93:  // TODO(townba): STS
	case 0x94:  // TODO(townba): CCH
	case 0x95:  // TODO(townba): MW
	case 0x96:  // TODO(townba): SPA
	case 0x97:  // TODO(townba): EPA
	case 0x98:  // TODO(townba): SOS
	case 0x99:  // TODO(townba): SGCI
	case 0x9A:  // DECID -- Identify Terminal
	case 0x9B:  // TODO(townba): CSI
	case 0x9C:  // TODO(townba): ST
		break;
	case 0x90:  // DCS -- Device Control String
	case 0x9D:  // OSC -- Operating System Command
	case 0x9E:  // PM -- Privacy Message
	case 0x9F:  // APC -- Application Program Command
		tstrsequence(ascii);
		return;
	}
	// only CAN, SUB, \a and C1 chars interrupt a sequence
	term.esc &= ~(ESC_STR_END | ESC_STR);
}

/*
 * returns 1 when the sequence is finished and it hasn't to read
 * more characters for this sequence, otherwise 0
 */
int
eschandle(uchar ascii)
{
	switch (ascii) {
	case '[':
		term.esc |= ESC_CSI;
		return 0;
	case '#':
		term.esc |= ESC_TEST;
		return 0;
	case '%':
		term.esc |= ESC_UTF8;
		return 0;
	case 'P':  // DCS -- Device Control String
	case '_':  // APC -- Application Program Command
	case '^':  // PM -- Privacy Message
	case ']':  // OSC -- Operating System Command
	case 'k':  // old title set compatibility
		tstrsequence(ascii);
		return 0;
	case 'n':  // LS2 -- Locking shift 2
	case 'o':  // LS3 -- Locking shift 3
		term.charset = 2 + (ascii - 'n');
		break;
	case '(':  // GZD4 -- set primary charset G0
	case ')':  // G1D4 -- set secondary charset G1
	case '*':  // G2D4 -- set tertiary charset G2
	case '+':  // G3D4 -- set quaternary charset G3
		term.icharset = ascii - '(';
		term.esc |= ESC_ALTCHARSET;
		return 0;
	case 'D':  // IND -- Linefeed
		if (term.c.y == term.bot) {
			tscrollup(term.top, 1);
		} else {
			tmoveto(term.c.x, term.c.y + 1);
		}
		break;
	case 'E':             // NEL -- Next line
		tnewline(1);  // always go to first col
		break;
	case 'H':  // HTS -- Horizontal tab stop
		term.tabs[term.c.x] = 1;
		break;
	case 'M':  // RI -- Reverse index
		if (term.c.y == term.top) {
			tscrolldown(term.top, 1);
		} else {
			tmoveto(term.c.x, term.c.y - 1);
		}
		break;
	case 'c':  // RIS -- Reset to initial state
		treset();
		xresettitle();
		xloadcols();
		break;
	case 'g':  // Visual bell (IGNORED)
		break;
	case '=':  // DECPAM -- Application keypad
		term.mode |= MODE_APPKEYPAD;
		break;
	case '>':  // DECPNM -- Normal keypad
		term.mode &= ~MODE_APPKEYPAD;
		break;
	case '7':  // DECSC -- Save Cursor
		tcursor(CURSOR_SAVE);
		break;
	case '8':  // DECRC -- Restore Cursor
		tcursor(CURSOR_LOAD);
		break;
	case '\\':  // ST -- String Terminator
		if (term.esc & ESC_STR_END) {
			strhandle();
		}
		break;
	default:
		fprintf(stderr, "erresc: unknown sequence ESC ");
		chardump(ascii);
		putc('\n', stderr);
		break;
	}
	return 1;
}

void
tputc(Rune u)
{
	char c[max_utf8_bytes];
	int control;
	int width = 0;
	size_t len;
	Glyph *gp;

	control = ISCONTROL(u);
	if (IS_SET(MODE_UTF8)) {
		len = utf8encode(u, c);
		if (!control && (width = wcwidth(u)) == -1) {
			len = utf8encode(replacement_rune, c);
			width = wcwidth(u);
			if (width == -1) {
				width = 1;
			}
		}
	} else {
		c[0] = u;
		width = len = 1;
	}

	if (IS_SET(MODE_PRINT)) {
		tprinter(c, len);
	}

	/*
	 * STR sequence must be checked before anything else
	 * because it uses all following characters until it
	 * receives a ESC, a SUB, a ST or any other C1 control
	 * character.
	 */
	if (term.esc & ESC_STR) {
		if (u == '\a' || u == 0x18 || u == 0x1A || u == 0x1B ||
		    ISCONTROLC1(u)) {
			term.esc &= ~(ESC_START | ESC_STR | ESC_DCS);
			term.esc |= ESC_STR_END;
			goto check_control_code;
		}

		if (strescseq.len + len >= sizeof(strescseq.buf) - 1) {
			/*
			 * Here is a bug in terminals. If the user never sends
			 * some code to stop the str or esc command, then st
			 * will stop responding. But this is better than
			 * silently failing with unknown characters. At least
			 * then users will report back.
			 *
			 * In the case users ever get fixed, here is the code:
			 */
			/*
			 * term.esc = 0;
			 * strhandle();
			 */
			return;
		}

		memmove(&strescseq.buf[strescseq.len], c, len);
		strescseq.len += len;
		return;
	}

check_control_code:
	/*
	 * Actions of control codes must be performed as soon they arrive
	 * because they can be embedded inside a control sequence, and
	 * they must not cause conflicts with sequences.
	 */
	if (control) {
		tcontrolcode(u);
		// control codes are not shown ever
		return;
	} else if (term.esc & ESC_START) {
		if (term.esc & ESC_CSI) {
			csiescseq.buf[csiescseq.len++] = u;
			if (BETWEEN(u, 0x40, 0x7E) ||
			    csiescseq.len >= sizeof(csiescseq.buf) - 1) {
				term.esc = 0;
				csiparse();
				csihandle();
			}
			return;
		}
		if (term.esc & ESC_UTF8) {
			tdefutf8(u);
		} else if (term.esc & ESC_ALTCHARSET) {
			tdeftran(u);
		} else if (term.esc & ESC_TEST) {
			tdectest(u);
		} else {
			if (!eschandle(u)) {
				return;
			}
			// sequence already finished
		}
		term.esc = 0;
		/*
		 * All characters which form part of a sequence are not
		 * printed
		 */
		return;
	}
	if (sel.ob.x != -1 && BETWEEN(term.c.y, sel.ob.y, sel.oe.y)) {
		selclear(NULL);
	}

	gp = &term.line[term.c.y][term.c.x];
	if (IS_SET(MODE_WRAP) && (term.c.state & CURSOR_WRAPNEXT)) {
		gp->mode |= ATTR_WRAP;
		tnewline(1);
		gp = &term.line[term.c.y][term.c.x];
	}

	if (IS_SET(MODE_INSERT) && term.c.x + width < term.col) {
		memmove(gp + width, gp,
		        (term.col - term.c.x - width) * sizeof(Glyph));
	}

	if (term.c.x + width > term.col) {
		tnewline(1);
		gp = &term.line[term.c.y][term.c.x];
	}

	tsetchar(u, &term.c.attr, term.c.x, term.c.y);

	if (width == 2) {
		gp->mode |= ATTR_WIDE;
		if (term.c.x + 1 < term.col) {
			gp[1].u = 0;
			gp[1].mode = ATTR_WDUMMY;
		}
	}
	if (term.c.x + width < term.col) {
		tmoveto(term.c.x + width, term.c.y);
	} else {
		term.c.state |= CURSOR_WRAPNEXT;
	}
}

void
tresize(int col, int row)
{
	int i;
	int minrow = MIN(row, term.row);
	int mincol = MIN(col, term.col);
	int *bp;
	TCursor c;

	if (col < 1 || row < 1 || col > USHRT_MAX || row > USHRT_MAX) {
		fprintf(stderr, "tresize: error resizing to %dx%d\n", col, row);
		return;
	}

	/*
	 * slide screen to keep cursor where we expect it -
	 * tscrollup would work here, but we can optimize to
	 * memmove because we're freeing the earlier lines
	 */
	for (i = 0; i <= term.c.y - row; i++) {
		free(term.line[i]);
		free(term.alt[i]);
	}
	// ensure that both src and dst are not NULL
	if (i > 0) {
		memmove(term.line, term.line + i, row * sizeof(Line));
		memmove(term.alt, term.alt + i, row * sizeof(Line));
	}
	for (i += row; i < term.row; i++) {
		free(term.line[i]);
		free(term.alt[i]);
	}

	// resize to new width
	term.specbuf = (XftGlyphFontSpec *)xrealloc(
	    term.specbuf, col * sizeof(XftGlyphFontSpec));

	// resize to new height
	term.line = (Line *)xrealloc(term.line, row * sizeof(Line));
	term.alt = (Line *)xrealloc(term.alt, row * sizeof(Line));
	term.dirty = (int *)xrealloc(term.dirty, row * sizeof(*term.dirty));
	term.tabs = (int *)xrealloc(term.tabs, col * sizeof(*term.tabs));

	// resize each row to new width, zero-pad if needed
	for (i = 0; i < minrow; i++) {
		term.line[i] =
		    (Line)xrealloc(term.line[i], col * sizeof(Glyph));
		term.alt[i] = (Line)xrealloc(term.alt[i], col * sizeof(Glyph));
	}

	// allocate any new rows
	for (/* i == minrow */; i < row; i++) {
		term.line[i] = (Line)xmalloc(col * sizeof(Glyph));
		term.alt[i] = (Line)xmalloc(col * sizeof(Glyph));
	}
	if (col > term.col) {
		bp = term.tabs + term.col;

		memset(bp, 0, sizeof(*term.tabs) * (col - term.col));
		while (--bp > term.tabs && !*bp) {
		}
		for (bp += tabspaces; bp < term.tabs + col; bp += tabspaces) {
			*bp = 1;
		}
	}
	// update terminal size
	term.col = col;
	term.row = row;
	// reset scrolling region
	tsetscroll(0, row - 1);
	// make use of the LIMIT in tmoveto
	tmoveto(term.c.x, term.c.y);
	// Clearing both screens (it makes dirty all lines)
	c = term.c;
	for (i = 0; i < 2; i++) {
		if (mincol < col && 0 < minrow) {
			tclearregion(mincol, 0, col - 1, minrow - 1);
		}
		if (0 < col && minrow < row) {
			tclearregion(0, minrow, col - 1, row - 1);
		}
		tswapscreen();
		tcursor(CURSOR_LOAD);
	}
	term.c = c;
}

void
xresize(int col, int row)
{
	xw.tw = MAX(1, col * xw.cw);
	xw.th = MAX(1, row * xw.ch);

	XFreePixmap(xw.dpy, xw.buf);
	xw.buf = XCreatePixmap(xw.dpy, xw.win, xw.w, xw.h,
	                       DefaultDepth(xw.dpy, xw.scr));
	XftDrawChange(xw.draw, xw.buf);
	xclear(0, 0, xw.w, xw.h);
}

ushort
sixd_to_16bit(int x)
{
	return (x == 0) ? 0 : (0x3737 + 0x2828 * x);
}

int
xloadcolor(int i, const char *name, Color *ncolor)
{
	XRenderColor color = {.alpha = 0xFFFF};

	if (!name) {
		if (BETWEEN(i, 16, 255)) {         // 256 color
			if (i < 6 * 6 * 6 + 16) {  // same colors as xterm
				color.red = sixd_to_16bit(((i - 16) / 36) % 6);
				color.green = sixd_to_16bit(((i - 16) / 6) % 6);
				color.blue = sixd_to_16bit(((i - 16) / 1) % 6);
			} else {  // greyscale
				color.red =
				    0x0808 + 0x0A0A * (i - (6 * 6 * 6 + 16));
				color.green = color.blue = color.red;
			}
			return XftColorAllocValue(xw.dpy, xw.vis, xw.cmap,
			                          &color, ncolor);
		}
		name = colorname[i];
	}

	return XftColorAllocName(xw.dpy, xw.vis, xw.cmap, name, ncolor);
}

void
xloadcols(void)
{
	static int loaded;
	Color *cp;

	if (loaded) {
		for (cp = dc.col; cp < &dc.col[LEN(dc.col)]; ++cp) {
			XftColorFree(xw.dpy, xw.vis, xw.cmap, cp);
		}
	}

	for (size_t i = 0; i < LEN(dc.col); i++) {
		if (!xloadcolor(i, NULL, &dc.col[i])) {
			if (colorname[i]) {
				die("Could not allocate color '%s'\n",
				    colorname[i]);
			} else {
				die("Could not allocate color %d\n", i);
			}
		}
	}
	loaded = 1;
}

int
xsetcolorname(int x, const char *name)
{
	Color ncolor;

	if (x < 0 || !BETWEEN((size_t)(x), 0, LEN(dc.col))) {
		return 1;
	}

	if (!xloadcolor(x, name, &ncolor)) {
		return 1;
	}

	XftColorFree(xw.dpy, xw.vis, xw.cmap, &dc.col[x]);
	dc.col[x] = ncolor;

	return 0;
}

// Absolute coordinates.
void
xclear(int x1, int y1, int x2, int y2)
{
	XftDrawRect(xw.draw,
	            &dc.col[IS_SET(MODE_REVERSE) ? defaultfg : defaultbg], x1,
	            y1, x2 - x1, y2 - y1);
}

void
xhints(void)
{
	XClassHint xclass = {(char *)(opt_name ? opt_name : termname),
	                     (char *)(opt_class ? opt_class : termname)};
	XWMHints wm = {.flags = InputHint, .input = 1};
	XSizeHints *sizeh = NULL;

	sizeh = XAllocSizeHints();

	sizeh->flags = PSize | PResizeInc | PBaseSize;
	sizeh->height = xw.h;
	sizeh->width = xw.w;
	sizeh->height_inc = xw.ch;
	sizeh->width_inc = xw.cw;
	sizeh->base_height = 2 * borderpx;
	sizeh->base_width = 2 * borderpx;
	if (xw.isfixed) {
		sizeh->flags |= PMaxSize | PMinSize;
		sizeh->min_width = sizeh->max_width = xw.w;
		sizeh->min_height = sizeh->max_height = xw.h;
	}
	if (xw.gm & (XValue | YValue)) {
		sizeh->flags |= USPosition | PWinGravity;
		sizeh->x = xw.l;
		sizeh->y = xw.t;
		sizeh->win_gravity = xgeommasktogravity(xw.gm);
	}

	XSetWMProperties(xw.dpy, xw.win, NULL, NULL, NULL, 0, sizeh, &wm,
	                 &xclass);
	XFree(sizeh);
}

int
xgeommasktogravity(int mask)
{
	switch (mask & (XNegative | YNegative)) {
	case 0:
		return NorthWestGravity;
	case XNegative:
		return NorthEastGravity;
	case YNegative:
		return SouthWestGravity;
	}

	return SouthEastGravity;
}

int
xloadfont(Font *f, const FcPattern *pattern)
{
	FcPattern *configured;
	FcPattern *match;
	FcResult result;
	XGlyphInfo extents;
	int wantattr, haveattr;

	/*
	 * Manually configure instead of calling XftMatchFont
	 * so that we can use the configured pattern for
	 * "missing glyph" lookups.
	 */
	configured = FcPatternDuplicate(pattern);
	if (!configured) {
		return 1;
	}

	FcConfigSubstitute(NULL, configured, FcMatchPattern);
	XftDefaultSubstitute(xw.dpy, xw.scr, configured);

	match = FcFontMatch(NULL, configured, &result);
	if (!match) {
		FcPatternDestroy(configured);
		return 1;
	}

	if (!(f->match = XftFontOpenPattern(xw.dpy, match))) {
		FcPatternDestroy(configured);
		FcPatternDestroy(match);
		return 1;
	}

	if ((XftPatternGetInteger(pattern, "slant", 0, &wantattr) ==
	     XftResultMatch)) {
		/*
		 * Check if xft was unable to find a font with the appropriate
		 * slant but gave us one anyway. Try to mitigate.
		 */
		if ((XftPatternGetInteger(f->match->pattern, "slant", 0,
		                          &haveattr) != XftResultMatch) ||
		    haveattr < wantattr) {
			f->badslant = 1;
			fputs("st: font slant does not match\n", stderr);
		}
	}

	if ((XftPatternGetInteger(pattern, "weight", 0, &wantattr) ==
	     XftResultMatch)) {
		if ((XftPatternGetInteger(f->match->pattern, "weight", 0,
		                          &haveattr) != XftResultMatch) ||
		    haveattr != wantattr) {
			f->badweight = 1;
			fputs("st: font weight does not match\n", stderr);
		}
	}

	XftTextExtentsUtf8(xw.dpy, f->match, (const FcChar8 *)ascii_printable,
	                   strlen(ascii_printable), &extents);

	f->set = NULL;
	f->pattern = configured;

	f->ascent = f->match->ascent;
	f->descent = f->match->descent;
	f->lbearing = 0;
	f->rbearing = f->match->max_advance_width;

	f->height = f->ascent + f->descent;
	f->width = DIVCEIL(extents.xOff, strlen(ascii_printable));

	return 0;
}

void
xloadfonts(const char *fontstr, double fontsize)
{
	FcPattern *pattern;
	double fontval;

	if (fontstr[0] == '-') {
		pattern = XftXlfdParse(fontstr, False, False);
	} else {
		pattern = FcNameParse((FcChar8 *)fontstr);
	}

	if (!pattern) {
		die("st: can't open font %s\n", fontstr);
	}

	if (fontsize > 1) {
		FcPatternDel(pattern, FC_PIXEL_SIZE);
		FcPatternDel(pattern, FC_SIZE);
		FcPatternAddDouble(pattern, FC_PIXEL_SIZE, fontsize);
		usedfontsize = fontsize;
	} else {
		if (FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &fontval) ==
		    FcResultMatch) {
			usedfontsize = fontval;
		} else if (FcPatternGetDouble(pattern, FC_SIZE, 0, &fontval) ==
		           FcResultMatch) {
			usedfontsize = -1;
		} else {
			/*
			 * Default font size is 12, if none given. This is to
			 * have a known usedfontsize value.
			 */
			FcPatternAddDouble(pattern, FC_PIXEL_SIZE, 12);
			usedfontsize = 12;
		}
		defaultfontsize = usedfontsize;
	}

	if (xloadfont(&dc.font, pattern)) {
		die("st: can't open font %s\n", fontstr);
	}

	if (usedfontsize < 0) {
		FcPatternGetDouble(dc.font.match->pattern, FC_PIXEL_SIZE, 0,
		                   &fontval);
		usedfontsize = fontval;
		if (fontsize == 0) {
			defaultfontsize = fontval;
		}
	}

	// Setting character width and height.
	xw.cw = ceilf(dc.font.width * cwscale);
	xw.ch = ceilf(dc.font.height * chscale);

	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
	if (xloadfont(&dc.ifont, pattern)) {
		die("st: can't open font %s\n", fontstr);
	}

	FcPatternDel(pattern, FC_WEIGHT);
	FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
	if (xloadfont(&dc.ibfont, pattern)) {
		die("st: can't open font %s\n", fontstr);
	}

	FcPatternDel(pattern, FC_SLANT);
	FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ROMAN);
	if (xloadfont(&dc.bfont, pattern)) {
		die("st: can't open font %s\n", fontstr);
	}

	FcPatternDestroy(pattern);
}

void
xunloadfont(Font *f)
{
	XftFontClose(xw.dpy, f->match);
	FcPatternDestroy(f->pattern);
	if (f->set) {
		FcFontSetDestroy(f->set);
	}
}

void
xunloadfonts(void)
{
	// Free the loaded fonts in the font cache.
	while (frclen > 0) {
		XftFontClose(xw.dpy, frc[--frclen].font);
	}

	xunloadfont(&dc.font);
	xunloadfont(&dc.bfont);
	xunloadfont(&dc.ifont);
	xunloadfont(&dc.ibfont);
}

void
xzoom(int increase)
{
	xzoomabs(usedfontsize + increase);
}

void
xzoomabs(int fontsize)
{
	xunloadfonts();
	xloadfonts(usedfont, fontsize);
	cresize(0, 0);
	ttyresize();
	redraw();
	xhints();
}

void
xzoomreset(UNUSED int unused)
{
	if (defaultfontsize > 0) {
		xzoomabs(defaultfontsize);
	}
}

const char *
xgetresstr(XrmDatabase xrmdb, const char *name, const char *xclass,
           const char *def)
{
	char *type = NULL;
	XrmValue value = {0};
	if (!XrmGetResource(xrmdb, name, xclass, &type, &value) ||
	    strcmp(type, "String") != 0) {
		return def;
	}
	return value.addr;
}

Bool
xgetresbool(XrmDatabase xrmdb, const char *name, const char *xclass, Bool def)
{
	const char *val = xgetresstr(xrmdb, name, xclass, NULL);
	if (!val) {
		return def;
	}
	if (strcmp(val, "false") == 0) {
		return False;
	}
	if (strcmp(val, "true") == 0) {
		return True;
	}
	fprintf(stderr, "unexpected value for %s\n", name);
	return def;
}

void
xinit(int argc, char *argv[])
{
	XGCValues gcvalues;
	Cursor cursor;
	Window parent;
	pid_t thispid = getpid();
	XColor xmousefg, xmousebg;
	XrmDatabase cmdlinedb = NULL, maindb = NULL;
	char *resman = NULL;
	// clang-format off
	XrmOptionDescRec opTable[] = {
	    {"-?",		"._h",		XrmoptionNoArg,		"true"},
	    {"--",		"._e",		XrmoptionSkipLine,	NULL},
	    {"-a",		".allowAltScreen", XrmoptionNoArg,	"false"},
	    {"-c",		".class",	XrmoptionSepArg,	NULL},
	    {"-e",		"._e",		XrmoptionSkipLine,	NULL},
	    {"-f",		".font",	XrmoptionSepArg,	NULL},
	    {"-g",		".geometry",	XrmoptionSepArg,	NULL},
	    {"-h",		"._h",		XrmoptionNoArg,		"true"},
	    {"-i",		".fixedGeometry", XrmoptionNoArg,	"true"},
	    {"-l",		".line", 	XrmoptionSepArg,	NULL},
	    {"-n",		".name", 	XrmoptionSepArg,	NULL},
	    {"-o",		".outputFile",	XrmoptionSepArg,	NULL},
	    {"-T",		".title",	XrmoptionSepArg,	NULL},
	    {"-t",		".title",	XrmoptionSepArg,	NULL},
	    {"-u",		".iso14755Command", XrmoptionSepArg,	NULL},
	    {"-v",		"._v",		XrmoptionNoArg,		"true"},
	    {"-w",		".embed",	XrmoptionSepArg,	NULL},
	    {"-display",	".display",	XrmoptionSepArg,	NULL},
	    {"-xrm",		NULL,		XrmoptionResArg,	NULL},
	};
	// clang-format on

	XrmInitialize();

	// Get some initialization options from just the command line.
	XrmParseCommand(&cmdlinedb, opTable, LEN(opTable), "st", &argc, argv);
	--argc, ++argv;
	if (xgetresbool(cmdlinedb, "st._h", "St._H", False)) {
		usage();
	}
	if (xgetresbool(cmdlinedb, "st._v", "St._V", False)) {
		die("%s " VERSION " (c) 2010-2016 st engineers\n", argv0);
	}
	if (!(xw.dpy = XOpenDisplay(
	          xgetresstr(cmdlinedb, "st.display", "St.Display", NULL)))) {
		die("Can't open display\n");
	}

	// Other options come from resources or the command line.
	if ((resman = XResourceManagerString(xw.dpy)) != NULL) {
		XrmMergeDatabases(XrmGetStringDatabase(resman), &maindb);
	}
	// Command line is higher priority than resources.
	XrmMergeDatabases(cmdlinedb, &maindb);

	// Set options based on resources and command line.
	opt_allowaltscreen = xgetresbool(maindb, "st.allowAltScreen",
	                                 "St.AllowAltScreen", allowaltscreen);
	opt_class = xgetresstr(maindb, "st.class", "St.Class", NULL);
	opt_font = xgetresstr(maindb, "st.font", "St.Font", NULL);
	xw.l = xw.t = 0;
	xw.gm = XParseGeometry(
	    xgetresstr(maindb, "st.geometry", "St.Geometry", NULL), &xw.l,
	    &xw.t, &opt_cols, &opt_rows);
	xw.isfixed =
	    xgetresbool(maindb, "st.fixedGeometry", "St.FixedGeometry", False);
	opt_io = xgetresstr(maindb, "st.outputFile", "St.OutputFile", NULL);
	opt_iso14755_cmd = xgetresstr(maindb, "st.iso14755Command",
	                              "St.Iso14755Command", iso14755_cmd);
	opt_line = xgetresstr(maindb, "st.line", "St.Line", NULL);
	opt_name = xgetresstr(maindb, "st.name", "St.Name", NULL);
	opt_title = xgetresstr(maindb, "st.title", "St.Title", argv0);
	opt_embed = xgetresstr(maindb, "st.embed", "St.Embed", NULL);
	if (argc) {
		// Does the first argument look like a flag?
		if (argv[0][0] == '-') {
			// Drop '-e' and '--'.
			if (argv[0][1] == 'e' || argv[0][1] == '-') {
				--argc, ++argv;
			} else {
				usage();
			}
		}
		if (argc) {
			opt_cmd = (const char **)(argv);
		}
	}

	tnew(MAX(opt_cols, 1), MAX(opt_rows, 1));

	xw.scr = XDefaultScreen(xw.dpy);
	xw.vis = XDefaultVisual(xw.dpy, xw.scr);

	// font
	if (!FcInit()) {
		die("Could not init fontconfig.\n");
	}

	usedfont = (opt_font == NULL) ? font : opt_font;
	xloadfonts(usedfont, 0);

	// colors
	xw.cmap = XDefaultColormap(xw.dpy, xw.scr);
	xloadcols();

	// adjust fixed window geometry
	xw.w = 2 * borderpx + term.col * xw.cw;
	xw.h = 2 * borderpx + term.row * xw.ch;
	if (xw.gm & XNegative) {
		xw.l += DisplayWidth(xw.dpy, xw.scr) - xw.w - 2;
	}
	if (xw.gm & YNegative) {
		xw.t += DisplayHeight(xw.dpy, xw.scr) - xw.h - 2;
	}

	// Events
	xw.attrs.background_pixel = dc.col[defaultbg].pixel;
	xw.attrs.border_pixel = dc.col[defaultbg].pixel;
	xw.attrs.bit_gravity = NorthWestGravity;
	xw.attrs.event_mask = FocusChangeMask | KeyPressMask | ExposureMask |
	                      VisibilityChangeMask | StructureNotifyMask |
	                      ButtonMotionMask | ButtonPressMask |
	                      ButtonReleaseMask;
	xw.attrs.colormap = xw.cmap;

	if (!(opt_embed && (parent = strtol(opt_embed, NULL, 0)))) {
		parent = XRootWindow(xw.dpy, xw.scr);
	}
	xw.win =
	    XCreateWindow(xw.dpy, parent, xw.l, xw.t, xw.w, xw.h, 0,
	                  XDefaultDepth(xw.dpy, xw.scr), InputOutput, xw.vis,
	                  CWBackPixel | CWBorderPixel | CWBitGravity |
	                      CWEventMask | CWColormap,
	                  &xw.attrs);

	memset(&gcvalues, 0, sizeof(gcvalues));
	gcvalues.graphics_exposures = False;
	dc.gc = XCreateGC(xw.dpy, parent, GCGraphicsExposures, &gcvalues);
	xw.buf = XCreatePixmap(xw.dpy, xw.win, xw.w, xw.h,
	                       DefaultDepth(xw.dpy, xw.scr));
	XSetForeground(xw.dpy, dc.gc, dc.col[defaultbg].pixel);
	XFillRectangle(xw.dpy, xw.buf, dc.gc, 0, 0, xw.w, xw.h);

	// Xft rendering context
	xw.draw = XftDrawCreate(xw.dpy, xw.buf, xw.vis, xw.cmap);

	// input methods
	if ((xw.xim = XOpenIM(xw.dpy, NULL, NULL, NULL)) == NULL) {
		XSetLocaleModifiers("@im=local");
		if ((xw.xim = XOpenIM(xw.dpy, NULL, NULL, NULL)) == NULL) {
			XSetLocaleModifiers("@im=");
			if ((xw.xim = XOpenIM(xw.dpy, NULL, NULL, NULL)) ==
			    NULL) {
				die("XOpenIM failed. Could not open input"
				    " device.\n");
			}
		}
	}
	xw.xic = XCreateIC(xw.xim, XNInputStyle,
	                   XIMPreeditNothing | XIMStatusNothing, XNClientWindow,
	                   xw.win, XNFocusWindow, xw.win, NULL);
	if (xw.xic == NULL) {
		die("XCreateIC failed. Could not obtain input method.\n");
	}

	// white cursor, black outline
	cursor = XCreateFontCursor(xw.dpy, mouseshape);
	XDefineCursor(xw.dpy, xw.win, cursor);

	if (XParseColor(xw.dpy, xw.cmap, colorname[mousefg], &xmousefg) == 0) {
		xmousefg.red = 0xFFFF;
		xmousefg.green = 0xFFFF;
		xmousefg.blue = 0xFFFF;
	}

	if (XParseColor(xw.dpy, xw.cmap, colorname[mousebg], &xmousebg) == 0) {
		xmousebg.red = 0x0000;
		xmousebg.green = 0x0000;
		xmousebg.blue = 0x0000;
	}

	XRecolorCursor(xw.dpy, cursor, &xmousefg, &xmousebg);

	xw.xembed = XInternAtom(xw.dpy, "_XEMBED", False);
	xw.wmdeletewin = XInternAtom(xw.dpy, "WM_DELETE_WINDOW", False);
	xw.netwmname = XInternAtom(xw.dpy, "_NET_WM_NAME", False);
	XSetWMProtocols(xw.dpy, xw.win, &xw.wmdeletewin, 1);

	xw.netwmpid = XInternAtom(xw.dpy, "_NET_WM_PID", False);
	XChangeProperty(xw.dpy, xw.win, xw.netwmpid, XA_CARDINAL, 32,
	                PropModeReplace, (uchar *)&thispid, 1);

	xresettitle();
	XMapWindow(xw.dpy, xw.win);
	xhints();
	XSync(xw.dpy, False);
}

int
xmakeglyphfontspecs(XftGlyphFontSpec *specs, const Glyph *glyphs, int len,
                    int x, int y)
{
	float winx = borderpx + x * xw.cw, winy = borderpx + y * xw.ch, xp, yp;
	ushort mode, prevmode = USHRT_MAX;
	Font *font = &dc.font;
	int frcflags = FRC_NORMAL;
	float runewidth = xw.cw;
	Rune rune;
	FT_UInt glyphidx;
	FcResult fcres;
	FcPattern *fcpattern, *fontpattern;
	FcFontSet *fcsets[] = {NULL};
	FcCharSet *fccharset;
	int i, numspecs = 0;
	size_t f;

	for (i = 0, xp = winx, yp = winy + font->ascent; i < len; ++i) {
		// Fetch rune and mode for current glyph.
		rune = glyphs[i].u;
		mode = glyphs[i].mode;

		// Skip dummy wide-character spacing.
		if (mode == ATTR_WDUMMY) {
			continue;
		}

		// Determine font for glyph if different from previous glyph.
		if (prevmode != mode) {
			prevmode = mode;
			font = &dc.font;
			frcflags = FRC_NORMAL;
			runewidth = xw.cw * ((mode & ATTR_WIDE) ? 2.0f : 1.0f);
			if ((mode & ATTR_ITALIC) && (mode & ATTR_BOLD)) {
				font = &dc.ibfont;
				frcflags = FRC_ITALICBOLD;
			} else if (mode & ATTR_ITALIC) {
				font = &dc.ifont;
				frcflags = FRC_ITALIC;
			} else if (mode & ATTR_BOLD) {
				font = &dc.bfont;
				frcflags = FRC_BOLD;
			}
			yp = winy + font->ascent;
		}

		// Lookup character index with default font.
		glyphidx = XftCharIndex(xw.dpy, font->match, rune);
		if (glyphidx) {
			specs[numspecs].font = font->match;
			specs[numspecs].glyph = glyphidx;
			specs[numspecs].x = (short)xp;
			specs[numspecs].y = (short)yp;
			xp += runewidth;
			numspecs++;
			continue;
		}

		// Fallback on font cache, search the font cache for match.
		for (f = 0; f < frclen; f++) {
			glyphidx = XftCharIndex(xw.dpy, frc[f].font, rune);
			// Everything correct.
			if (glyphidx && frc[f].flags == frcflags) {
				break;
			}
			// We got a default font for a not found glyph.
			if (!glyphidx && frc[f].flags == frcflags &&
			    frc[f].unicodep == rune) {
				break;
			}
		}

		// Nothing was found. Use fontconfig to find matching font.
		if (f >= frclen) {
			if (!font->set) {
				font->set =
				    FcFontSort(0, font->pattern, 1, 0, &fcres);
			}
			fcsets[0] = font->set;

			/*
			 * Nothing was found in the cache. Now use
			 * some dozen of Fontconfig calls to get the
			 * font for one single character.
			 *
			 * Xft and fontconfig are design failures.
			 */
			fcpattern = FcPatternDuplicate(font->pattern);
			fccharset = FcCharSetCreate();

			FcCharSetAddChar(fccharset, rune);
			FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
			FcPatternAddBool(fcpattern, FC_SCALABLE, 1);

			FcConfigSubstitute(0, fcpattern, FcMatchPattern);
			FcDefaultSubstitute(fcpattern);

			fontpattern =
			    FcFontSetMatch(0, fcsets, 1, fcpattern, &fcres);

			// Overwrite or create the new cache entry.
			if (frclen >= LEN(frc)) {
				frclen = LEN(frc) - 1;
				XftFontClose(xw.dpy, frc[frclen].font);
				frc[frclen].unicodep = 0;
			}

			frc[frclen].font =
			    XftFontOpenPattern(xw.dpy, fontpattern);
			frc[frclen].flags = frcflags;
			frc[frclen].unicodep = rune;

			glyphidx = XftCharIndex(xw.dpy, frc[frclen].font, rune);

			f = frclen;
			frclen++;

			FcPatternDestroy(fcpattern);
			FcCharSetDestroy(fccharset);
		}

		specs[numspecs].font = frc[f].font;
		specs[numspecs].glyph = glyphidx;
		specs[numspecs].x = (short)xp;
		specs[numspecs].y = (short)yp;
		xp += runewidth;
		numspecs++;
	}

	return numspecs;
}

Color *
xgetcolor(uint32_t basecol, XRenderColor *rendercol, Color *truecol)
{
	if (!IS_TRUECOL(basecol)) {
		return &dc.col[basecol];
	}
	rendercol->alpha = 0xFFFF;
	rendercol->red = TRUERED(basecol);
	rendercol->green = TRUEGREEN(basecol);
	rendercol->blue = TRUEBLUE(basecol);
	XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, rendercol, truecol);
	return truecol;
}

void
xdrawglyphfontspecs(const XftGlyphFontSpec *specs, Glyph base, int len, int x,
                    int y)
{
	int charlen = len * ((base.mode & ATTR_WIDE) ? 2 : 1);
	int winx = borderpx + x * xw.cw, winy = borderpx + y * xw.ch,
	    width = charlen * xw.cw;
	Color *fg, *bg, revfg, revbg, truefg, truebg;
	Color **destfg, **destbg;
	XRenderColor colfg, colbg;
	XRectangle r;
	uint32_t srcfg;

	// Fallback on color display for attributes not supported by the font
	if (base.mode & ATTR_ITALIC && base.mode & ATTR_BOLD) {
		if (dc.ibfont.badslant || dc.ibfont.badweight) {
			base.fg = defaultattr;
		}
	} else if ((base.mode & ATTR_ITALIC && dc.ifont.badslant) ||
	           (base.mode & ATTR_BOLD && dc.bfont.badweight)) {
		base.fg = defaultattr;
	}

	fg = xgetcolor(base.fg, &colfg, &truefg);
	bg = xgetcolor(base.bg, &colbg, &truebg);

	/*
	 * When reversing, we change the background instead of the foreground
	 * because we swap them later.
	 */
	destfg = !(base.mode & ATTR_REVERSE) ? &fg : &bg;
	destbg = !(base.mode & ATTR_REVERSE) ? &bg : &fg;
	srcfg = !(base.mode & ATTR_REVERSE) ? base.fg : base.bg;

	// Change basic system colors [0-7] to bright system colors [8-15]
	if ((base.mode & ATTR_BOLD_FAINT) == ATTR_BOLD &&
	    BETWEEN(srcfg, 0, 7)) {
		*destfg = &dc.col[srcfg + 8];
	}

	if (IS_SET(MODE_REVERSE)) {
		if (fg == &dc.col[defaultfg]) {
			fg = &dc.col[defaultbg];
		} else {
			colfg.red = ~fg->color.red;
			colfg.green = ~fg->color.green;
			colfg.blue = ~fg->color.blue;
			colfg.alpha = fg->color.alpha;
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg,
			                   &revfg);
			fg = &revfg;
		}

		if (bg == &dc.col[defaultbg]) {
			bg = &dc.col[defaultfg];
		} else {
			colbg.red = ~bg->color.red;
			colbg.green = ~bg->color.green;
			colbg.blue = ~bg->color.blue;
			colbg.alpha = bg->color.alpha;
			XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colbg,
			                   &revbg);
			bg = &revbg;
		}
	}

	if ((base.mode & ATTR_BOLD_FAINT) == ATTR_FAINT) {
		colfg.red = (*destfg)->color.red / 2;
		colfg.green = (*destfg)->color.green / 2;
		colfg.blue = (*destfg)->color.blue / 2;
		colfg.alpha = (*destfg)->color.alpha;
		XftColorAllocValue(xw.dpy, xw.vis, xw.cmap, &colfg, &revfg);
		*destfg = &revfg;
	}

	if ((base.mode & ATTR_BLINK) && (term.mode & MODE_BLINK)) {
		destfg = destbg;
	}

	if (base.mode & ATTR_INVISIBLE) {
		destfg = destbg;
	}

	// Intelligent cleaning up of the borders.
	if (x == 0) {
		xclear(0, (y == 0) ? 0 : winy, borderpx,
		       winy + xw.ch + ((y >= term.row - 1) ? xw.h : 0));
	}
	if (x + charlen >= term.col) {
		xclear(winx + width, (y == 0) ? 0 : winy, xw.w,
		       ((y >= term.row - 1) ? xw.h : (winy + xw.ch)));
	}
	if (y == 0) {
		xclear(winx, 0, winx + width, borderpx);
	}
	if (y == term.row - 1) {
		xclear(winx, winy + xw.ch, winx + width, xw.h);
	}

	// Clean up the region we want to draw to.
	XftDrawRect(xw.draw, *destbg, winx, winy, width, xw.ch);

	// Set the clip region because Xft is sometimes dirty.
	r.x = 0;
	r.y = 0;
	r.height = xw.ch;
	r.width = width;
	XftDrawSetClipRectangles(xw.draw, winx, winy, &r, 1);

	// Render the glyphs.
	XftDrawGlyphFontSpec(xw.draw, *destfg, specs, len);

	// Render underline and strikethrough.
	if (base.mode & ATTR_UNDERLINE) {
		XftDrawRect(xw.draw, *destfg, winx, winy + dc.font.ascent + 1,
		            width, 1);
	}

	if (base.mode & ATTR_STRUCK) {
		XftDrawRect(xw.draw, *destfg, winx,
		            winy + 2 * dc.font.ascent / 3, width, 1);
	}

	// Reset clip to none.
	XftDrawSetClip(xw.draw, 0);
}

void
xdrawglyph(Glyph g, int x, int y)
{
	int numspecs;
	XftGlyphFontSpec spec;

	numspecs = xmakeglyphfontspecs(&spec, &g, 1, x, y);
	xdrawglyphfontspecs(&spec, g, numspecs, x, y);
}

void
xdrawcursor(void)
{
	static int oldx = 0, oldy = 0;
	int curx;
	Glyph g = {' ', ATTR_NULL, defaultbg, defaultcs}, og;
	int ena_sel = sel.ob.x != -1 && sel.alt == IS_SET(MODE_ALTSCREEN);
	Color drawcol;

	LIMIT(oldx, 0, term.col - 1);
	LIMIT(oldy, 0, term.row - 1);

	curx = term.c.x;

	// adjust position if in dummy
	if (term.line[oldy][oldx].mode & ATTR_WDUMMY) {
		oldx--;
	}
	if (term.line[term.c.y][curx].mode & ATTR_WDUMMY) {
		curx--;
	}

	// remove the old cursor
	og = term.line[oldy][oldx];
	if (ena_sel && selected(oldx, oldy)) {
		og.mode ^= ATTR_REVERSE;
	}
	xdrawglyph(og, oldx, oldy);

	g.u = term.line[term.c.y][term.c.x].u;

	// Select the right color for the right mode.
	if (IS_SET(MODE_REVERSE)) {
		g.mode |= ATTR_REVERSE;
		g.bg = defaultfg;
		if (ena_sel && selected(term.c.x, term.c.y)) {
			drawcol = dc.col[defaultcs];
			g.fg = defaultrcs;
		} else {
			drawcol = dc.col[defaultrcs];
			g.fg = defaultcs;
		}
	} else {
		if (ena_sel && selected(term.c.x, term.c.y)) {
			drawcol = dc.col[defaultrcs];
			g.fg = defaultfg;
			g.bg = defaultrcs;
		} else {
			drawcol = dc.col[defaultcs];
		}
	}

	if (IS_SET(MODE_HIDE)) {
		return;
	}

	// draw the new one
	if (xw.state & WIN_FOCUSED) {
		switch (xw.cursor) {
		case 7:  // st extension: snowman
			utf8decode("\xE2\x98\x83", 3, &g.u);
		case 0:  // Blinking Block
		case 1:  // Blinking Block (Default)
		case 2:  // Steady Block
			g.mode |= term.line[term.c.y][curx].mode & ATTR_WIDE;
			xdrawglyph(g, term.c.x, term.c.y);
			break;
		case 3:  // Blinking Underline
		case 4:  // Steady Underline
			XftDrawRect(
			    xw.draw, &drawcol, borderpx + curx * xw.cw,
			    borderpx + (term.c.y + 1) * xw.ch - cursorthickness,
			    xw.cw, cursorthickness);
			break;
		case 5:  // Blinking bar
		case 6:  // Steady bar
			XftDrawRect(xw.draw, &drawcol, borderpx + curx * xw.cw,
			            borderpx + term.c.y * xw.ch,
			            cursorthickness, xw.ch);
			break;
		}
	} else {
		XftDrawRect(xw.draw, &drawcol, borderpx + curx * xw.cw,
		            borderpx + term.c.y * xw.ch, xw.cw - 1, 1);
		XftDrawRect(xw.draw, &drawcol, borderpx + curx * xw.cw,
		            borderpx + term.c.y * xw.ch, 1, xw.ch - 1);
		XftDrawRect(xw.draw, &drawcol,
		            borderpx + (curx + 1) * xw.cw - 1,
		            borderpx + term.c.y * xw.ch, 1, xw.ch - 1);
		XftDrawRect(xw.draw, &drawcol, borderpx + curx * xw.cw,
		            borderpx + (term.c.y + 1) * xw.ch - 1, xw.cw, 1);
	}
	oldx = curx, oldy = term.c.y;
}

void
xsettitle(const char *p)
{
	XTextProperty prop;

	Xutf8TextListToTextProperty(xw.dpy, (char **)(&p), 1, XUTF8StringStyle,
	                            &prop);
	XSetWMName(xw.dpy, xw.win, &prop);
	XSetTextProperty(xw.dpy, xw.win, &prop, xw.netwmname);
	XFree(prop.value);
}

void
xresettitle(void)
{
	xsettitle(opt_title ? opt_title : "st");
}

void
redraw(void)
{
	tfulldirt();
	draw();
}

void
draw(void)
{
	drawregion(0, 0, term.col, term.row);
	XCopyArea(xw.dpy, xw.buf, xw.win, dc.gc, 0, 0, xw.w, xw.h, 0, 0);
	XSetForeground(
	    xw.dpy, dc.gc,
	    dc.col[IS_SET(MODE_REVERSE) ? defaultfg : defaultbg].pixel);
}

void
drawregion(int x1, int y1, int x2, int y2)
{
	int i, x, y, ox, numspecs;
	Glyph base, gnew;
	XftGlyphFontSpec *specs;
	int ena_sel = sel.ob.x != -1 && sel.alt == IS_SET(MODE_ALTSCREEN);

	if (!(xw.state & WIN_VISIBLE)) {
		return;
	}

	for (y = y1; y < y2; y++) {
		if (!term.dirty[y]) {
			continue;
		}

		term.dirty[y] = 0;

		specs = term.specbuf;
		numspecs = xmakeglyphfontspecs(specs, &term.line[y][x1],
		                               x2 - x1, x1, y);

		i = ox = 0;
		for (x = x1; x < x2 && i < numspecs; x++) {
			gnew = term.line[y][x];
			if (gnew.mode == ATTR_WDUMMY) {
				continue;
			}
			if (ena_sel && selected(x, y)) {
				gnew.mode ^= ATTR_REVERSE;
			}
			if (i > 0 && ATTRCMP(base, gnew)) {
				xdrawglyphfontspecs(specs, base, i, ox, y);
				specs += i;
				numspecs -= i;
				i = 0;
			}
			if (i == 0) {
				ox = x;
				base = gnew;
			}
			i++;
		}
		if (i > 0) {
			xdrawglyphfontspecs(specs, base, i, ox, y);
		}
	}
	xdrawcursor();
}

void
expose(UNUSED XEvent *unused)
{
	redraw();
}

void
visibility(XEvent *ev)
{
	XVisibilityEvent *e = &ev->xvisibility;

	MODBIT(xw.state, e->state != VisibilityFullyObscured, WIN_VISIBLE);
}

void
unmap(UNUSED XEvent *unused)
{
	xw.state &= ~WIN_VISIBLE;
}

void
xsetpointermotion(int set)
{
	MODBIT(xw.attrs.event_mask, set, PointerMotionMask);
	XChangeWindowAttributes(xw.dpy, xw.win, CWEventMask, &xw.attrs);
}

void
xseturgency(int add)
{
	XWMHints *h = XGetWMHints(xw.dpy, xw.win);

	MODBIT(h->flags, add, XUrgencyHint);
	XSetWMHints(xw.dpy, xw.win, h);
	XFree(h);
}

void
focus(XEvent *ev)
{
	XFocusChangeEvent *e = &ev->xfocus;

	if (e->mode == NotifyGrab) {
		return;
	}

	if (ev->type == FocusIn) {
		XSetICFocus(xw.xic);
		xw.state |= WIN_FOCUSED;
		xseturgency(0);
		if (IS_SET(MODE_FOCUS)) {
			ttywrite("\x1B[I", 3);
		}
	} else {
		XUnsetICFocus(xw.xic);
		xw.state &= ~WIN_FOCUSED;
		if (IS_SET(MODE_FOCUS)) {
			ttywrite("\x1B[O", 3);
		}
	}
}

int
modifiers_match(uint mask, uint state)
{
	return mask == XK_ANY_MOD || mask == (state & ~ignoremod);
}

const char *
kmap(KeySym k, uint state)
{
	const Key *kp;
	size_t i;

	// Check for mapped keys out of X11 function keys.
	for (i = 0; i < LEN(mappedkeys); i++) {
		if (mappedkeys[i] == k) {
			break;
		}
	}
	if (i == LEN(mappedkeys)) {
		if ((k & 0xFFFF) < 0xFD00) {
			return NULL;
		}
	}

	for (kp = key; kp < key + LEN(key); kp++) {
		if (kp->k != k) {
			continue;
		}

		if (!modifiers_match(kp->mask, state)) {
			continue;
		}

		if (IS_SET(MODE_APPKEYPAD) ? kp->appkey < 0 : kp->appkey > 0) {
			continue;
		}

		if (IS_SET(MODE_APPCURSOR) ? kp->appcursor < 0
		                           : kp->appcursor > 0) {
			continue;
		}

		if (IS_SET(MODE_CRLF) ? kp->crlf < 0 : kp->crlf > 0) {
			continue;
		}

		return kp->s;
	}

	return NULL;
}

void
kpress(XEvent *ev)
{
	XKeyEvent *e = &ev->xkey;
	KeySym ksym;
	char buf[32];
	const char *customkey;
	int len;
	Rune c;
	Status status;
	const Shortcut *bp;

	if (IS_SET(MODE_KBDLOCK)) {
		return;
	}

	len = XmbLookupString(xw.xic, e, buf, sizeof buf, &ksym, &status);
	// 1. shortcuts
	for (bp = shortcuts; bp < shortcuts + LEN(shortcuts); bp++) {
		if (ksym == bp->keysym && modifiers_match(bp->mod, e->state)) {
			bp->func(bp->arg);
			return;
		}
	}

	// 2. custom keys from config.h
	if ((customkey = kmap(ksym, e->state))) {
		ttysend(customkey, strlen(customkey));
		return;
	}

	// 3. composed string from input method
	if (len == 0) {
		return;
	}
	if (len == 1 && e->state & Mod1Mask) {
		if (IS_SET(MODE_8BIT)) {
			if (*buf < 0x7F) {
				c = *buf | 0x80;
				len = utf8encode(c, buf);
			}
		} else {
			buf[1] = buf[0];
			buf[0] = 0x1B;
			len = 2;
		}
	}
	ttysend(buf, len);
}

void
cmessage(XEvent *e)
{
	/*
	 * See xembed specs
	 *  http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
	 */
	if (e->xclient.message_type == xw.xembed && e->xclient.format == 32) {
		if (e->xclient.data.l[1] == XEMBED_FOCUS_IN) {
			xw.state |= WIN_FOCUSED;
			xseturgency(0);
		} else if (e->xclient.data.l[1] == XEMBED_FOCUS_OUT) {
			xw.state &= ~WIN_FOCUSED;
		}
	} else if ((Atom)(e->xclient.data.l[0]) == xw.wmdeletewin) {
		// Send SIGHUP to shell
		kill(pid, SIGHUP);
		exit_with_code = 0;
	}
}

void
cresize(int width, int height)
{
	int col, row;

	if (width != 0) {
		xw.w = width;
	}
	if (height != 0) {
		xw.h = height;
	}

	col = (xw.w - 2 * borderpx) / xw.cw;
	row = (xw.h - 2 * borderpx) / xw.ch;

	tresize(col, row);
	xresize(col, row);
}

void
resize(XEvent *e)
{
	if (e->xconfigure.width == xw.w && e->xconfigure.height == xw.h) {
		return;
	}

	cresize(e->xconfigure.width, e->xconfigure.height);
	ttyresize();
}

int
run(void)
{
	XEvent ev;
	int w = xw.w, h = xw.h;
	fd_set rfd;
	int xfd = XConnectionNumber(xw.dpy), xev, blinkset = 0, dodraw = 0;
	struct timespec drawtimeout, *tv = NULL, now, last, lastblink;
	long deltatime;

	// Waiting for window mapping
	do {
		XNextEvent(xw.dpy, &ev);
		/*
		 * This XFilterEvent call is required because of XOpenIM. It
		 * does filter out the key event and some client message for
		 * the input method too.
		 */
		if (XFilterEvent(&ev, None)) {
			continue;
		}
		if (ev.type == ConfigureNotify) {
			w = ev.xconfigure.width;
			h = ev.xconfigure.height;
		}
	} while (ev.type != MapNotify);

	cresize(w, h);
	ttynew();
	ttyresize();

	clock_gettime(CLOCK_MONOTONIC, &last);
	lastblink = last;

	for (xev = actionfps;;) {
		if (exit_with_code >= 0) {
			return exit_with_code;
		}
		FD_ZERO(&rfd);
		FD_SET(cmdfd, &rfd);
		FD_SET(xfd, &rfd);

		if (pselect(MAX(xfd, cmdfd) + 1, &rfd, NULL, NULL, tv, NULL) <
		    0) {
			if (errno == EINTR) {
				continue;
			}
			die("select failed: %s\n", strerror(errno));
		}
		if (FD_ISSET(cmdfd, &rfd)) {
			ttyread();
			if (blinktimeout) {
				blinkset = tattrset(ATTR_BLINK);
				if (!blinkset) {
					MODBIT(term.mode, 0, MODE_BLINK);
				}
			}
		}

		if (FD_ISSET(xfd, &rfd)) {
			xev = actionfps;
		}

		clock_gettime(CLOCK_MONOTONIC, &now);
		drawtimeout.tv_sec = 0;
		drawtimeout.tv_nsec = (1000 * 1E6) / xfps;
		tv = &drawtimeout;

		dodraw = 0;
		if (blinktimeout && TIMEDIFF(now, lastblink) > blinktimeout) {
			tsetdirtattr(ATTR_BLINK);
			term.mode ^= MODE_BLINK;
			lastblink = now;
			dodraw = 1;
		}
		deltatime = TIMEDIFF(now, last);
		if (deltatime > 1000 / (xev ? xfps : actionfps)) {
			dodraw = 1;
			last = now;
		}

		if (dodraw) {
			while (XPending(xw.dpy)) {
				XNextEvent(xw.dpy, &ev);
				if (XFilterEvent(&ev, None)) {
					continue;
				}
				if (handler[ev.type]) {
					(handler[ev.type])(&ev);
				}
			}

			draw();
			XFlush(xw.dpy);

			if (xev && !FD_ISSET(xfd, &rfd)) {
				xev--;
			}
			if (!FD_ISSET(cmdfd, &rfd) && !FD_ISSET(xfd, &rfd)) {
				if (blinkset) {
					if (TIMEDIFF(now, lastblink) >
					    blinktimeout) {
						drawtimeout.tv_nsec = 1000;
					} else {
						drawtimeout.tv_nsec =
						    (1E6 *
						     (blinktimeout -
						      TIMEDIFF(now,
						               lastblink)));
					}
					drawtimeout.tv_sec =
					    drawtimeout.tv_nsec / 1E9;
					drawtimeout.tv_nsec %= (long)1E9;
				} else {
					tv = NULL;
				}
			}
		}
	}
}

void
usage(void)
{
	die("usage: %s [-?ahiv] [-c <class>] [-f <font>] [-g <geometry>]\n"
	    "          [-n <name>] [-o <file>] [-T <title>] [-t <title>]\n"
	    "          [-u <ISO/IEC 14755 command>] [-w <windowid>]\n"
	    "          [-display <display>] [-xrm <xrm>]\n"
	    "          [[-e | --] <command> [<arg>...] | -l <line> "
	    "[stty_args]]\n",
	    argv0, argv0);
}

int
main(int argc, char *argv[])
{
	signal(SIGSEGV, sigsegv_handler);
	argv0 = xstrdup(basename(argv[0]));

	xw.l = xw.t = 0;
	xw.isfixed = False;
	xw.cursor = cursorshape;

	setlocale(LC_CTYPE, "");
	XSetLocaleModifiers("");
	xinit(argc, argv);
	selinit();
	return run();
}
