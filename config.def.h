/* See LICENSE file for copyright and license details. */

/*
 * appearance
 *
 * font: see http://freedesktop.org/software/fontconfig/fontconfig-user.html
 */
static const char font[] = "Liberation Mono:pixelsize=12:antialias=true:autohint=true";
static const int borderpx = 2;

/*
 * What program is execed by st depends of these precedence rules:
 * 1: program passed with -e
 * 2: utmp option
 * 3: SHELL environment variable
 * 4: value of shell in /etc/passwd
 * 5: value of shell in config.h
 */
static const char shell[] = "/bin/sh";
static const char *utmp = NULL;
static const char stty_args[] = "stty raw pass8 nl -echo -iexten -cstopb 38400";

/* The command executed for entering Unicode code points. */
static const char iso14755_cmd[] = "dmenu -p 'Unicode code point in hexadecimal:' < /dev/null";

/* identification sequence returned in DA and DECID */
static const char vtiden[] = "\033[?6c";

// The response to DA1.
static const char da1_response[] = "\x1B[?65;1;2;7;9;12;18;19;21;22;23;24;42;44;45;46c";

/* identification sequence returned in secondary DA */
static const char vtiden2[] = "\033[>41;1;0c";

/* Kerning / character bounding-box multipliers */
static const float cwscale = 1.0;
static const float chscale = 1.0;

/*
 * word delimiter string
 *
 * More advanced example: " `'\"()[]{}"
 */
static const char worddelimiters[] = " ";

/* selection timeouts (in milliseconds) */
static const unsigned int doubleclicktimeout = 300;
static const unsigned int tripleclicktimeout = 600;

/* alt screens */
static const int allowaltscreen = 1;

/* frames per second st should at maximum draw to the screen */
static const unsigned int xfps = 120;
static const unsigned int actionfps = 30;

/*
 * blinking timeout (set to 0 to disable blinking) for the terminal blinking
 * attribute.
 */
static const unsigned int blinktimeout = 800;

/*
 * thickness of underline and bar cursors
 */
static const unsigned int cursorthickness = 2;

/*
 * bell volume. It must be a value between -100 and 100. Use 0 for disabling
 * it
 */
static const int bellvolume = 100;

/* default TERM value */
static const char termname[] = "st-256color";

/*
 * spaces per tab
 *
 * When you are changing this value, don't forget to adapt the »it« value in
 * the st.info and appropriately install the st.info in the environment where
 * you use this st version.
 *
 *	it#$tabspaces,
 *
 * Secondly make sure your kernel is not expanding tabs. When running `stty
 * -a` »tab0« should appear. You can tell the terminal to not expand tabs by
 *  running following command:
 *
 *	stty tabs
 */
static const unsigned int tabspaces = 8;

/* Terminal colors (16 first used in escape sequence) */
static const char *const colorname[] = {
	/* 8 normal colors */
	"black",
	"red3",
	"green3",
	"yellow3",
	"blue2",
	"magenta3",
	"cyan3",
	"gray90",

	/* 8 bright colors */
	"gray50",
	"red",
	"green",
	"yellow",
	"#5c5cff",
	"magenta",
	"cyan",
	"white",

	[255] = 0,

	/* more colors can be added after 255 to use with DefaultXX */
	"#cccccc",
	"#555555",
};


/*
 * Default colors (colorname index)
 * foreground, background, cursor, reverse cursor
 */
static const unsigned int defaultfg = 7;
static const unsigned int defaultbg = 0;
static const unsigned int defaultcs = 256;
static const unsigned int defaultrcs = 257;

/*
 * Default shape of cursor
 * 2: Block ("█")
 * 4: Underline ("_")
 * 6: Bar ("|")
 * 7: Snowman ("☃")
 */
static const unsigned int cursorshape = 2;

/*
 * Default columns and rows numbers
 */

static const unsigned int cols = 80;
static const unsigned int rows = 24;

/*
 * Default colour and shape of the mouse cursor
 */
static const unsigned int mouseshape = XC_xterm;
static const unsigned int mousefg = 7;
static const unsigned int mousebg = 0;

/*
 * Color used to display font attributes when fontconfig selected a font which
 * doesn't match the ones requested.
 */
static const unsigned int defaultattr = 7;

/*
 * Default destination(s) for OSC 52 (copy/paste) operations: 'c' is clipboard,
 * 'p' is primary. An empty string disables OSC 52.
 */
static const char defaultosc52[] = "p";

/*
 * Internal mouse shortcuts.
 * Beware that overloading Button1 will disable the selection.
 */
static const MouseShortcut mshortcuts[] = {
	/* button               mask            string */
	{ Button4,              XK_ANY_MOD,     "\031" },
	{ Button5,              XK_ANY_MOD,     "\005" },
};

/* Internal keyboard shortcuts. */
#define MODKEY (ControlMask|ShiftMask)
#define ControlShiftMask (ControlMask|ShiftMask)
#define ControlMod1ShiftMask (ControlMask|Mod1Mask|ShiftMask)

static const Shortcut shortcuts[] = {
	/* mask                 keysym          function        argument */
	{ XK_ANY_MOD,           XK_Break,       sendbreak,       0 },
	{ ControlMask,          XK_Print,       toggleprinter,   0 },
	{ ShiftMask,            XK_Print,       printscreen,     0 },
	{ Mod1Mask,             XK_Print,       printsel,        0 },
	{ ControlMask,          XK_equal,       xzoom,          +1 },
	{ ControlMask,          XK_minus,       xzoom,          -1 },
	{ ControlMask,          XK_0,           xzoomreset,      0 },
	{ ControlShiftMask,     XK_C,           clipcopy,        0 },
	{ ControlShiftMask,     XK_K,           reset,           0 },
	{ ControlShiftMask,     XK_L,           iso14755,        0 },
	{ ControlShiftMask,     XK_V,           clippaste,       0 },
	{ ControlMod1ShiftMask, XK_V,           selpaste,        0 },
};

/*
 * Special keys (change & recompile st.info accordingly)
 *
 * Mask value:
 * * Use XK_ANY_MOD to match the key no matter modifiers state
 * * Use XK_NO_MOD to match the key alone (no modifiers)
 * appkey value:
 * * 0: no value
 * * > 0: keypad application mode enabled
 * * < 0: keypad application mode disabled
 * appcursor value:
 * * 0: no value
 * * > 0: cursor application mode enabled
 * * < 0: cursor application mode disabled
 * crlf value
 * * 0: no value
 * * > 0: crlf mode is enabled
 * * < 0: crlf mode is disabled
 *
 * Be careful with the order of the definitions because st searches in
 * this table sequentially, so any XK_ANY_MOD must be in the last
 * position for a key.
 */

/*
 * If you want keys other than the X11 function keys (0xFD00 - 0xFFFF)
 * to be mapped below, add them to this array.
 */
static const KeySym mappedkeys[] = { (KeySym)(-1) };

/*
 * State bits to ignore when matching key or button events.  By default,
 * numlock (Mod2Mask) and keyboard layout (XK_SWITCH_MOD) are ignored.
 */
static const uint ignoremod = Mod2Mask|XK_SWITCH_MOD;

/*
 * Override mouse-select while mask is active (when MODE_MOUSE is set).
 * Note that if you want to use ShiftMask with selmasks, set this to an other
 * modifier, set to 0 to not use it.
 */
static const uint forceselmod = ShiftMask;

/*
 * This is the huge key array which defines all compatibility to the Linux
 * world. Please decide about changes wisely.
 */
static const Key key[] = {
	/* keysym           mask            string      appkey appcursor crlf */
	{ XK_KP_Home,       ShiftMask,      "\033[1;2H",     0,    0,    0},
	{ XK_KP_Home,       Mod1Mask,       "\033[1;3H",     0,    0,    0},
	{ XK_KP_Home,       Mod1Mask|ShiftMask,             "\033[1;4H",     0,    0,    0},
	{ XK_KP_Home,       ControlMask,    "\033[1;5H",     0,    0,    0},
	{ XK_KP_Home,       ControlMask|ShiftMask,          "\033[1;6H",     0,    0,    0},
	{ XK_KP_Home,       ControlMask|Mod1Mask,           "\033[1;7H",     0,    0,    0},
	{ XK_KP_Home,       ControlMask|Mod1Mask|ShiftMask, "\033[1;8H",     0,    0,    0},
	{ XK_KP_Home,       XK_ANY_MOD,     "\033[H",        0,   -1,    0},
	{ XK_KP_Home,       XK_ANY_MOD,     "\033OH",        0,   +1,    0},
	{ XK_KP_Up,         ShiftMask,      "\033[1;2A",     0,    0,    0},
	{ XK_KP_Up,         Mod1Mask,       "\033[1;3A",     0,    0,    0},
	{ XK_KP_Up,         Mod1Mask|ShiftMask,             "\033[1;4A",     0,    0,    0},
	{ XK_KP_Up,         ControlMask,    "\033[1;5A",     0,    0,    0},
	{ XK_KP_Up,         ControlMask|ShiftMask,          "\033[1;6A",     0,    0,    0},
	{ XK_KP_Up,         ControlMask|Mod1Mask,           "\033[1;7A",     0,    0,    0},
	{ XK_KP_Up,         ControlMask|Mod1Mask|ShiftMask, "\033[1;8A",     0,    0,    0},
	{ XK_KP_Up,         XK_ANY_MOD,     "\033[A",        0,   -1,    0},
	{ XK_KP_Up,         XK_ANY_MOD,     "\033OA",        0,   +1,    0},
	{ XK_KP_Down,       ShiftMask,      "\033[1;2B",     0,    0,    0},
	{ XK_KP_Down,       Mod1Mask,       "\033[1;3B",     0,    0,    0},
	{ XK_KP_Down,       Mod1Mask|ShiftMask,             "\033[1;4B",     0,    0,    0},
	{ XK_KP_Down,       ControlMask,    "\033[1;5B",     0,    0,    0},
	{ XK_KP_Down,       ControlMask|ShiftMask,          "\033[1;6B",     0,    0,    0},
	{ XK_KP_Down,       ControlMask|Mod1Mask,           "\033[1;7B",     0,    0,    0},
	{ XK_KP_Down,       ControlMask|Mod1Mask|ShiftMask, "\033[1;8B",     0,    0,    0},
	{ XK_KP_Down,       XK_ANY_MOD,     "\033[B",        0,   -1,    0},
	{ XK_KP_Down,       XK_ANY_MOD,     "\033OB",        0,   +1,    0},
	{ XK_KP_Left,       ShiftMask,      "\033[1;2D",     0,    0,    0},
	{ XK_KP_Left,       Mod1Mask,       "\033[1;3D",     0,    0,    0},
	{ XK_KP_Left,       Mod1Mask|ShiftMask,             "\033[1;4D",     0,    0,    0},
	{ XK_KP_Left,       ControlMask,    "\033[1;5D",     0,    0,    0},
	{ XK_KP_Left,       ControlMask|ShiftMask,          "\033[1;6D",     0,    0,    0},
	{ XK_KP_Left,       ControlMask|Mod1Mask,           "\033[1;7D",     0,    0,    0},
	{ XK_KP_Left,       ControlMask|Mod1Mask|ShiftMask, "\033[1;8D",     0,    0,    0},
	{ XK_KP_Left,       XK_ANY_MOD,     "\033[D",        0,   -1,    0},
	{ XK_KP_Left,       XK_ANY_MOD,     "\033OD",        0,   +1,    0},
	{ XK_KP_Right,      ShiftMask,      "\033[1;2C",     0,    0,    0},
	{ XK_KP_Right,      Mod1Mask,       "\033[1;3C",     0,    0,    0},
	{ XK_KP_Right,      Mod1Mask|ShiftMask,             "\033[1;4C",     0,    0,    0},
	{ XK_KP_Right,      ControlMask,    "\033[1;5C",     0,    0,    0},
	{ XK_KP_Right,      ControlMask|ShiftMask,          "\033[1;6C",     0,    0,    0},
	{ XK_KP_Right,      ControlMask|Mod1Mask,           "\033[1;7C",     0,    0,    0},
	{ XK_KP_Right,      ControlMask|Mod1Mask|ShiftMask, "\033[1;8C",     0,    0,    0},
	{ XK_KP_Right,      XK_ANY_MOD,     "\033[C",        0,   -1,    0},
	{ XK_KP_Right,      XK_ANY_MOD,     "\033OC",        0,   +1,    0},
	{ XK_KP_Prior,      ShiftMask,      "\033[5;2~",     0,    0,    0},
	{ XK_KP_Prior,      Mod1Mask,       "\033[5;3~",     0,    0,    0},
	{ XK_KP_Prior,      Mod1Mask|ShiftMask,             "\033[5;4~",     0,    0,    0},
	{ XK_KP_Prior,      ControlMask,    "\033[5;5~",     0,    0,    0},
	{ XK_KP_Prior,      ControlMask|ShiftMask,          "\033[5;6~",     0,    0,    0},
	{ XK_KP_Prior,      ControlMask|Mod1Mask,           "\033[5;7~",     0,    0,    0},
	{ XK_KP_Prior,      ControlMask|Mod1Mask|ShiftMask, "\033[5;8~",     0,    0,    0},
	{ XK_KP_Prior,      XK_ANY_MOD,     "\033[5~",       0,    0,    0},
	{ XK_KP_Begin,      ShiftMask,      "\033[1;2E",     0,    0,    0},
	{ XK_KP_Begin,      Mod1Mask,       "\033[1;3E",     0,    0,    0},
	{ XK_KP_Begin,      Mod1Mask|ShiftMask,             "\033[1;4E",     0,    0,    0},
	{ XK_KP_Begin,      ControlMask,    "\033[1;5E",     0,    0,    0},
	{ XK_KP_Begin,      ControlMask|ShiftMask,          "\033[1;6E",     0,    0,    0},
	{ XK_KP_Begin,      ControlMask|Mod1Mask,           "\033[1;7E",     0,    0,    0},
	{ XK_KP_Begin,      ControlMask|Mod1Mask|ShiftMask, "\033[1;8E",     0,    0,    0},
	{ XK_KP_Begin,      XK_ANY_MOD,     "\033[E",        0,   -1,    0},
	{ XK_KP_Begin,      XK_ANY_MOD,     "\033OE",        0,   +1,    0},
	{ XK_KP_End,        ShiftMask,      "\033[1;2F",     0,    0,    0},
	{ XK_KP_End,        Mod1Mask,       "\033[1;3F",     0,    0,    0},
	{ XK_KP_End,        Mod1Mask|ShiftMask,             "\033[1;4F",     0,    0,    0},
	{ XK_KP_End,        ControlMask,    "\033[1;5F",     0,    0,    0},
	{ XK_KP_End,        ControlMask|ShiftMask,          "\033[1;6F",     0,    0,    0},
	{ XK_KP_End,        ControlMask|Mod1Mask,           "\033[1;7F",     0,    0,    0},
	{ XK_KP_End,        ControlMask|Mod1Mask|ShiftMask, "\033[1;8F",     0,    0,    0},
	{ XK_KP_End,        XK_ANY_MOD,     "\033[F",        0,   -1,    0},
	{ XK_KP_End,        XK_ANY_MOD,     "\033OF",        0,   +1,    0},
	{ XK_KP_Next,       ShiftMask,      "\033[6;2~",     0,    0,    0},
	{ XK_KP_Next,       Mod1Mask,       "\033[6;3~",     0,    0,    0},
	{ XK_KP_Next,       Mod1Mask|ShiftMask,             "\033[6;4~",     0,    0,    0},
	{ XK_KP_Next,       ControlMask,    "\033[6;5~",     0,    0,    0},
	{ XK_KP_Next,       ControlMask|ShiftMask,          "\033[6;6~",     0,    0,    0},
	{ XK_KP_Next,       ControlMask|Mod1Mask,           "\033[6;7~",     0,    0,    0},
	{ XK_KP_Next,       ControlMask|Mod1Mask|ShiftMask, "\033[6;8~",     0,    0,    0},
	{ XK_KP_Next,       XK_ANY_MOD,     "\033[6~",       0,    0,    0},
	{ XK_KP_Enter,      XK_ANY_MOD,     "\r",            0,   -1,   -1},
	{ XK_KP_Enter,      XK_ANY_MOD,     "\r\n",          0,   -1,   +1},
	{ XK_KP_Enter,      XK_ANY_MOD,     "\033OM",        0,   +1,    0},
	{ XK_KP_Insert,     ShiftMask,      "\033[2;2~",     0,    0,    0},
	{ XK_KP_Insert,     Mod1Mask,       "\033[2;3~",     0,    0,    0},
	{ XK_KP_Insert,     Mod1Mask|ShiftMask,             "\033[2;4~",     0,    0,    0},
	{ XK_KP_Insert,     ControlMask,    "\033[2;5~",     0,    0,    0},
	{ XK_KP_Insert,     ControlMask|ShiftMask,          "\033[2;6~",     0,    0,    0},
	{ XK_KP_Insert,     ControlMask|Mod1Mask,           "\033[2;7~",     0,    0,    0},
	{ XK_KP_Insert,     ControlMask|Mod1Mask|ShiftMask, "\033[2;8~",     0,    0,    0},
	{ XK_KP_Insert,     XK_ANY_MOD,     "\033[2~",       0,    0,    0},
	{ XK_KP_Delete,     ShiftMask,      "\033[3;2~",     0,    0,    0},
	{ XK_KP_Delete,     Mod1Mask,       "\033[3;3~",     0,    0,    0},
	{ XK_KP_Delete,     Mod1Mask|ShiftMask,             "\033[3;4~",     0,    0,    0},
	{ XK_KP_Delete,     ControlMask,    "\033[3;5~",     0,    0,    0},
	{ XK_KP_Delete,     ControlMask|ShiftMask,          "\033[3;6~",     0,    0,    0},
	{ XK_KP_Delete,     ControlMask|Mod1Mask,           "\033[3;7~",     0,    0,    0},
	{ XK_KP_Delete,     ControlMask|Mod1Mask|ShiftMask, "\033[3;8~",     0,    0,    0},
	{ XK_KP_Delete,     XK_ANY_MOD,     "\033[3~",       0,    0,    0},
	{ XK_Up,            ShiftMask,      "\033[1;2A",     0,    0,    0},
	{ XK_Up,            Mod1Mask,       "\033[1;3A",     0,    0,    0},
	{ XK_Up,            Mod1Mask|ShiftMask,             "\033[1;4A",     0,    0,    0},
	{ XK_Up,            ControlMask,    "\033[1;5A",     0,    0,    0},
	{ XK_Up,            ControlMask|ShiftMask,          "\033[1;6A",     0,    0,    0},
	{ XK_Up,            ControlMask|Mod1Mask,           "\033[1;7A",     0,    0,    0},
	{ XK_Up,            ControlMask|Mod1Mask|ShiftMask, "\033[1;8A",     0,    0,    0},
	{ XK_Up,            XK_ANY_MOD,     "\033[A",        0,   -1,    0},
	{ XK_Up,            XK_ANY_MOD,     "\033OA",        0,   +1,    0},
	{ XK_Down,          ShiftMask,      "\033[1;2B",     0,    0,    0},
	{ XK_Down,          Mod1Mask,       "\033[1;3B",     0,    0,    0},
	{ XK_Down,          Mod1Mask|ShiftMask,             "\033[1;4B",     0,    0,    0},
	{ XK_Down,          ControlMask,    "\033[1;5B",     0,    0,    0},
	{ XK_Down,          ControlMask|ShiftMask,          "\033[1;6B",     0,    0,    0},
	{ XK_Down,          ControlMask|Mod1Mask,           "\033[1;7B",     0,    0,    0},
	{ XK_Down,          ControlMask|Mod1Mask|ShiftMask, "\033[1;8B",     0,    0,    0},
	{ XK_Down,          XK_ANY_MOD,     "\033[B",        0,   -1,    0},
	{ XK_Down,          XK_ANY_MOD,     "\033OB",        0,   +1,    0},
	{ XK_Left,          ShiftMask,      "\033[1;2D",     0,    0,    0},
	{ XK_Left,          Mod1Mask,       "\033[1;3D",     0,    0,    0},
	{ XK_Left,          Mod1Mask|ShiftMask,             "\033[1;4D",     0,    0,    0},
	{ XK_Left,          ControlMask,    "\033[1;5D",     0,    0,    0},
	{ XK_Left,          ControlMask|ShiftMask,          "\033[1;6D",     0,    0,    0},
	{ XK_Left,          ControlMask|Mod1Mask,           "\033[1;7D",     0,    0,    0},
	{ XK_Left,          ControlMask|Mod1Mask|ShiftMask, "\033[1;8D",     0,    0,    0},
	{ XK_Left,          XK_ANY_MOD,     "\033[D",        0,   -1,    0},
	{ XK_Left,          XK_ANY_MOD,     "\033OD",        0,   +1,    0},
	{ XK_Right,         ShiftMask,      "\033[1;2C",     0,    0,    0},
	{ XK_Right,         Mod1Mask,       "\033[1;3C",     0,    0,    0},
	{ XK_Right,         Mod1Mask|ShiftMask,             "\033[1;4C",     0,    0,    0},
	{ XK_Right,         ControlMask,    "\033[1;5C",     0,    0,    0},
	{ XK_Right,         ControlMask|ShiftMask,          "\033[1;6C",     0,    0,    0},
	{ XK_Right,         ControlMask|Mod1Mask,           "\033[1;7C",     0,    0,    0},
	{ XK_Right,         ControlMask|Mod1Mask|ShiftMask, "\033[1;8C",     0,    0,    0},
	{ XK_Right,         XK_ANY_MOD,     "\033[C",        0,   -1,    0},
	{ XK_Right,         XK_ANY_MOD,     "\033OC",        0,   +1,    0},
	{ XK_ISO_Left_Tab,  ShiftMask,      "\033[Z",        0,    0,    0},
	{ XK_Tab,           ShiftMask,      "\033[Z",        0,    0,    0},
	{ XK_Return,        Mod1Mask,       "\033\r",        0,    0,   -1},
	{ XK_Return,        Mod1Mask,       "\033\r\n",      0,    0,   +1},
	{ XK_Return,        XK_ANY_MOD,     "\r",            0,    0,   -1},
	{ XK_Return,        XK_ANY_MOD,     "\r\n",          0,    0,   +1},
	{ XK_Insert,        ShiftMask,      "\033[2;2~",     0,    0,    0},
	{ XK_Insert,        Mod1Mask,       "\033[2;3~",     0,    0,    0},
	{ XK_Insert,        Mod1Mask|ShiftMask,             "\033[2;4~",     0,    0,    0},
	{ XK_Insert,        ControlMask,    "\033[2;5~",     0,    0,    0},
	{ XK_Insert,        ControlMask|ShiftMask,          "\033[2;6~",     0,    0,    0},
	{ XK_Insert,        ControlMask|Mod1Mask,           "\033[2;7~",     0,    0,    0},
	{ XK_Insert,        ControlMask|Mod1Mask|ShiftMask, "\033[2;8~",     0,    0,    0},
	{ XK_Insert,        XK_ANY_MOD,     "\033[2~",       0,    0,    0},
	{ XK_Delete,        ShiftMask,      "\033[3;2~",     0,    0,    0},
	{ XK_Delete,        Mod1Mask,       "\033[3;3~",     0,    0,    0},
	{ XK_Delete,        Mod1Mask|ShiftMask,             "\033[3;4~",     0,    0,    0},
	{ XK_Delete,        ControlMask,    "\033[3;5~",     0,    0,    0},
	{ XK_Delete,        ControlMask|ShiftMask,          "\033[3;6~",     0,    0,    0},
	{ XK_Delete,        ControlMask|Mod1Mask,           "\033[3;7~",     0,    0,    0},
	{ XK_Delete,        ControlMask|Mod1Mask|ShiftMask, "\033[3;8~",     0,    0,    0},
	{ XK_Delete,        XK_ANY_MOD,     "\033[3~",       0,    0,    0},
	{ XK_BackSpace,     Mod1Mask,       "\033\177",      0,    0,    0},
	{ XK_BackSpace,     ControlMask,    "\010",          0,    0,    0},
	{ XK_BackSpace,     ControlMask|Mod1Mask,           "\033\010",      0,    0,    0},
	{ XK_BackSpace,     XK_ANY_MOD,     "\177",          0,    0,    0},
	{ XK_Home,          ShiftMask,      "\033[1;2H",     0,    0,    0},
	{ XK_Home,          Mod1Mask,       "\033[1;3H",     0,    0,    0},
	{ XK_Home,          Mod1Mask|ShiftMask,             "\033[1;4H",     0,    0,    0},
	{ XK_Home,          ControlMask,    "\033[1;5H",     0,    0,    0},
	{ XK_Home,          ControlMask|ShiftMask,          "\033[1;6H",     0,    0,    0},
	{ XK_Home,          ControlMask|Mod1Mask,           "\033[1;7H",     0,    0,    0},
	{ XK_Home,          ControlMask|Mod1Mask|ShiftMask, "\033[1;8H",     0,    0,    0},
	{ XK_Home,          XK_ANY_MOD,     "\033[H",        0,   -1,    0},
	{ XK_Home,          XK_ANY_MOD,     "\033OH",        0,   +1,    0},
	{ XK_End,           ShiftMask,      "\033[1;2F",     0,    0,    0},
	{ XK_End,           Mod1Mask,       "\033[1;3F",     0,    0,    0},
	{ XK_End,           Mod1Mask|ShiftMask,             "\033[1;4F",     0,    0,    0},
	{ XK_End,           ControlMask,    "\033[1;5F",     0,    0,    0},
	{ XK_End,           ControlMask|ShiftMask,          "\033[1;6F",     0,    0,    0},
	{ XK_End,           ControlMask|Mod1Mask,           "\033[1;7F",     0,    0,    0},
	{ XK_End,           ControlMask|Mod1Mask|ShiftMask, "\033[1;8F",     0,    0,    0},
	{ XK_End,           XK_ANY_MOD,     "\033[F",        0,   -1,    0},
	{ XK_End,           XK_ANY_MOD,     "\033OF",        0,   +1,    0},
	{ XK_Prior,         ShiftMask,      "\033[5;2~",     0,    0,    0},
	{ XK_Prior,         Mod1Mask,       "\033[5;3~",     0,    0,    0},
	{ XK_Prior,         Mod1Mask|ShiftMask,             "\033[5;4~",     0,    0,    0},
	{ XK_Prior,         ControlMask,    "\033[5;5~",     0,    0,    0},
	{ XK_Prior,         ControlMask|ShiftMask,          "\033[5;6~",     0,    0,    0},
	{ XK_Prior,         ControlMask|Mod1Mask,           "\033[5;7~",     0,    0,    0},
	{ XK_Prior,         ControlMask|Mod1Mask|ShiftMask, "\033[5;8~",     0,    0,    0},
	{ XK_Prior,         XK_ANY_MOD,     "\033[5~",       0,    0,    0},
	{ XK_Next,          ShiftMask,      "\033[6;2~",     0,    0,    0},
	{ XK_Next,          Mod1Mask,       "\033[6;3~",     0,    0,    0},
	{ XK_Next,          Mod1Mask|ShiftMask ,            "\033[6;4~",     0,    0,    0},
	{ XK_Next,          ControlMask,    "\033[6;5~",     0,    0,    0},
	{ XK_Next,          ControlMask|ShiftMask,          "\033[6;6~",     0,    0,    0},
	{ XK_Next,          ControlMask|Mod1Mask,           "\033[6;7~",     0,    0,    0},
	{ XK_Next,          ControlMask|Mod1Mask|ShiftMask, "\033[6;8~",     0,    0,    0},
	{ XK_Next,          XK_ANY_MOD,     "\033[6~",       0,    0,    0},
	{ XK_F1, /* F13 */  ShiftMask,      "\033[1;2P",     0,    0,    0},
	{ XK_F1, /* F25 */  ControlMask,    "\033[1;5P",     0,    0,    0},
	{ XK_F1, /* F37 */  Mod4Mask,       "\033[1;6P",     0,    0,    0},
	{ XK_F1, /* F37 */  ControlMask|ShiftMask,          "\033[1;6P",     0,    0,    0},
	{ XK_F1, /* F49 */  Mod1Mask,       "\033[1;3P",     0,    0,    0},
	{ XK_F1, /* F61 */  Mod3Mask,       "\033[1;4P",     0,    0,    0},
	{ XK_F1, /* F61 */  Mod1Mask|ShiftMask,             "\033[1;4P",     0,    0,    0},
	{ XK_F1,            XK_ANY_MOD,     "\033OP" ,       0,    0,    0},
	{ XK_F2, /* F14 */  ShiftMask,      "\033[1;2Q",     0,    0,    0},
	{ XK_F2, /* F26 */  ControlMask,    "\033[1;5Q",     0,    0,    0},
	{ XK_F2, /* F38 */  Mod4Mask,       "\033[1;6Q",     0,    0,    0},
	{ XK_F2, /* F38 */  ControlMask|ShiftMask,          "\033[1;6Q",     0,    0,    0},
	{ XK_F2, /* F50 */  Mod1Mask,       "\033[1;3Q",     0,    0,    0},
	{ XK_F2, /* F62 */  Mod3Mask,       "\033[1;4Q",     0,    0,    0},
	{ XK_F2, /* F62 */  Mod1Mask|ShiftMask,             "\033[1;4Q",     0,    0,    0},
	{ XK_F2,            XK_ANY_MOD,     "\033OQ" ,       0,    0,    0},
	{ XK_F3, /* F15 */  ShiftMask,      "\033[1;2R",     0,    0,    0},
	{ XK_F3, /* F27 */  ControlMask,    "\033[1;5R",     0,    0,    0},
	{ XK_F3, /* F39 */  Mod4Mask,       "\033[1;6R",     0,    0,    0},
	{ XK_F3, /* F39 */  ControlMask|ShiftMask,          "\033[1;6R",     0,    0,    0},
	{ XK_F3, /* F51 */  Mod1Mask,       "\033[1;3R",     0,    0,    0},
	{ XK_F3, /* F63 */  Mod3Mask,       "\033[1;4R",     0,    0,    0},
	{ XK_F3, /* F63 */  Mod1Mask|ShiftMask,             "\033[1;4R",     0,    0,    0},
	{ XK_F3,            XK_ANY_MOD,     "\033OR" ,       0,    0,    0},
	{ XK_F4, /* F16 */  ShiftMask,      "\033[1;2S",     0,    0,    0},
	{ XK_F4, /* F28 */  ControlMask,    "\033[1;5S",     0,    0,    0},
	{ XK_F4, /* F40 */  Mod4Mask,       "\033[1;6S",     0,    0,    0},
	{ XK_F4, /* F40 */  ControlMask|ShiftMask,          "\033[1;6S",     0,    0,    0},
	{ XK_F4, /* F52 */  Mod1Mask,       "\033[1;3S",     0,    0,    0},
	{ XK_F4,            XK_ANY_MOD,     "\033OS" ,       0,    0,    0},
	{ XK_F5, /* F17 */  ShiftMask,      "\033[15;2~",    0,    0,    0},
	{ XK_F5, /* F29 */  ControlMask,    "\033[15;5~",    0,    0,    0},
	{ XK_F5, /* F41 */  Mod4Mask,       "\033[15;6~",    0,    0,    0},
	{ XK_F5, /* F41 */  ControlMask|ShiftMask,          "\033[15;6~",    0,    0,    0},
	{ XK_F5, /* F53 */  Mod1Mask,       "\033[15;3~",    0,    0,    0},
	{ XK_F5,            XK_ANY_MOD,     "\033[15~",      0,    0,    0},
	{ XK_F6, /* F18 */  ShiftMask,      "\033[17;2~",    0,    0,    0},
	{ XK_F6, /* F30 */  ControlMask,    "\033[17;5~",    0,    0,    0},
	{ XK_F6, /* F42 */  Mod4Mask,       "\033[17;6~",    0,    0,    0},
	{ XK_F6, /* F42 */  ControlMask|ShiftMask,          "\033[17;6~",    0,    0,    0},
	{ XK_F6, /* F54 */  Mod1Mask,       "\033[17;3~",    0,    0,    0},
	{ XK_F6,            XK_ANY_MOD,     "\033[17~",      0,    0,    0},
	{ XK_F7, /* F19 */  ShiftMask,      "\033[18;2~",    0,    0,    0},
	{ XK_F7, /* F31 */  ControlMask,    "\033[18;5~",    0,    0,    0},
	{ XK_F7, /* F43 */  Mod4Mask,       "\033[18;6~",    0,    0,    0},
	{ XK_F7, /* F43 */  ControlMask|ShiftMask,          "\033[18;6~",    0,    0,    0},
	{ XK_F7, /* F55 */  Mod1Mask,       "\033[18;3~",    0,    0,    0},
	{ XK_F7,            XK_ANY_MOD,     "\033[18~",      0,    0,    0},
	{ XK_F8, /* F20 */  ShiftMask,      "\033[19;2~",    0,    0,    0},
	{ XK_F8, /* F32 */  ControlMask,    "\033[19;5~",    0,    0,    0},
	{ XK_F8, /* F44 */  Mod4Mask,       "\033[19;6~",    0,    0,    0},
	{ XK_F8, /* F44 */  ControlMask|ShiftMask,          "\033[19;6~",    0,    0,    0},
	{ XK_F8, /* F56 */  Mod1Mask,       "\033[19;3~",    0,    0,    0},
	{ XK_F8,            XK_ANY_MOD,     "\033[19~",      0,    0,    0},
	{ XK_F9, /* F21 */  ShiftMask,      "\033[20;2~",    0,    0,    0},
	{ XK_F9, /* F33 */  ControlMask,    "\033[20;5~",    0,    0,    0},
	{ XK_F9, /* F45 */  Mod4Mask,       "\033[20;6~",    0,    0,    0},
	{ XK_F9, /* F45 */  ControlMask|ShiftMask,          "\033[20;6~",    0,    0,    0},
	{ XK_F9, /* F57 */  Mod1Mask,       "\033[20;3~",    0,    0,    0},
	{ XK_F9,            XK_ANY_MOD,     "\033[20~",      0,    0,    0},
	{ XK_F10, /* F22 */ ShiftMask,      "\033[21;2~",    0,    0,    0},
	{ XK_F10, /* F34 */ ControlMask,    "\033[21;5~",    0,    0,    0},
	{ XK_F10, /* F46 */ Mod4Mask,       "\033[21;6~",    0,    0,    0},
	{ XK_F10, /* F46 */ ControlMask|ShiftMask,          "\033[21;6~",    0,    0,    0},
	{ XK_F10, /* F58 */ Mod1Mask,       "\033[21;3~",    0,    0,    0},
	{ XK_F10,           XK_ANY_MOD,     "\033[21~",      0,    0,    0},
	{ XK_F11, /* F23 */ ShiftMask,      "\033[23;2~",    0,    0,    0},
	{ XK_F11, /* F35 */ ControlMask,    "\033[23;5~",    0,    0,    0},
	{ XK_F11, /* F47 */ Mod4Mask,       "\033[23;6~",    0,    0,    0},
	{ XK_F11, /* F47 */ ControlMask|ShiftMask,          "\033[23;6~",    0,    0,    0},
	{ XK_F11, /* F59 */ Mod1Mask,       "\033[23;3~",    0,    0,    0},
	{ XK_F11,           XK_ANY_MOD,     "\033[23~",      0,    0,    0},
	{ XK_F12, /* F24 */ ShiftMask,      "\033[24;2~",    0,    0,    0},
	{ XK_F12, /* F36 */ ControlMask,    "\033[24;5~",    0,    0,    0},
	{ XK_F12, /* F48 */ Mod4Mask,       "\033[24;6~",    0,    0,    0},
	{ XK_F12, /* F48 */ ControlMask|ShiftMask,          "\033[24;6~",    0,    0,    0},
	{ XK_F12, /* F60 */ Mod1Mask,       "\033[24;3~",    0,    0,    0},
	{ XK_F12,           XK_ANY_MOD,     "\033[24~",      0,    0,    0},
	{ XK_F13,           XK_ANY_MOD,     "\033[1;2P",     0,    0,    0},
	{ XK_F14,           XK_ANY_MOD,     "\033[1;2Q",     0,    0,    0},
	{ XK_F15,           XK_ANY_MOD,     "\033[1;2R",     0,    0,    0},
	{ XK_F16,           XK_ANY_MOD,     "\033[1;2S",     0,    0,    0},
	{ XK_F17,           XK_ANY_MOD,     "\033[15;2~",    0,    0,    0},
	{ XK_F18,           XK_ANY_MOD,     "\033[17;2~",    0,    0,    0},
	{ XK_F19,           XK_ANY_MOD,     "\033[18;2~",    0,    0,    0},
	{ XK_F20,           XK_ANY_MOD,     "\033[19;2~",    0,    0,    0},
	{ XK_F21,           XK_ANY_MOD,     "\033[20;2~",    0,    0,    0},
	{ XK_F22,           XK_ANY_MOD,     "\033[21;2~",    0,    0,    0},
	{ XK_F23,           XK_ANY_MOD,     "\033[23;2~",    0,    0,    0},
	{ XK_F24,           XK_ANY_MOD,     "\033[24;2~",    0,    0,    0},
	{ XK_F25,           XK_ANY_MOD,     "\033[1;5P",     0,    0,    0},
	{ XK_F26,           XK_ANY_MOD,     "\033[1;5Q",     0,    0,    0},
	{ XK_F27,           XK_ANY_MOD,     "\033[1;5R",     0,    0,    0},
	{ XK_F28,           XK_ANY_MOD,     "\033[1;5S",     0,    0,    0},
	{ XK_F29,           XK_ANY_MOD,     "\033[15;5~",    0,    0,    0},
	{ XK_F30,           XK_ANY_MOD,     "\033[17;5~",    0,    0,    0},
	{ XK_F31,           XK_ANY_MOD,     "\033[18;5~",    0,    0,    0},
	{ XK_F32,           XK_ANY_MOD,     "\033[19;5~",    0,    0,    0},
	{ XK_F33,           XK_ANY_MOD,     "\033[20;5~",    0,    0,    0},
	{ XK_F34,           XK_ANY_MOD,     "\033[21;5~",    0,    0,    0},
	{ XK_F35,           XK_ANY_MOD,     "\033[23;5~",    0,    0,    0},
	{ XK_Help,          ShiftMask,      "\033[28;2~",    0,    0,    0},
	{ XK_Help,          Mod1Mask,       "\033[28;3~",    0,    0,    0},
	{ XK_Help,          Mod1Mask|ShiftMask,             "\033[28;4~",    0,    0,    0},
	{ XK_Help,          ControlMask,    "\033[28;5~",    0,    0,    0},
	{ XK_Help,          ControlMask|ShiftMask,          "\033[28;6~",    0,    0,    0},
	{ XK_Help,          ControlMask|Mod1Mask,           "\033[28;7~",    0,    0,    0},
	{ XK_Help,          ControlMask|Mod1Mask|ShiftMask, "\033[28;8~",    0,    0,    0},
	{ XK_Help,          XK_ANY_MOD,     "\033[28~",      0,    0,    0},
	{ XK_Print,         XK_ANY_MOD,     "\033[35~",      0,    0,    0},
};

/*
 * Selection types' masks.
 * Use the same masks as usual.
 * Button1Mask is always unset, to make masks match between ButtonPress.
 * ButtonRelease and MotionNotify.
 * If no match is found, regular selection is used.
 */
static const uint selmasks[] = {
	[SEL_RECTANGULAR] = Mod1Mask,
};

/*
 * Printable characters in ASCII, used to estimate the advance width
 * of single wide characters.
 */
static const char ascii_printable[] =
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~";

