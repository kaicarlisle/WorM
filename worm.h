#include <X11/Xlib.h>
#include <string.h>

typedef union {
    const char** com;
    const int i;
} Arg;

// Structs
typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg arg);
    const Arg arg;
} key;

typedef struct client client;
struct client{
    // Prev and next client
    client *next, *prev;
    // The window
    Window win;
    unsigned int x, y, width, height;
};

typedef struct desktop desktop;
struct desktop{
    unsigned int numwins, showbar, splitx, splity, fullscreen;
    client *head, *current, *transient;
};

// Functions
static void add_window(Window w);
static void add_transient_window(Window w);
static void buttonpress(XEvent *e);
static void buttonrelease(XEvent *e);
static void center_split();
static void change_desktop(const Arg arg);
static void configurerequest(XEvent *e);
static void destroynotify(XEvent *e);
static void draw_swap_borders();
static void enter_swap();
static unsigned long getcolor(const char* color);
static void grabkeys();
static void keypress(XEvent *e);
static void kill_client();
static void kill_client_now(Window w);
static void last_desktop();
static void logger(const char* e);
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static void move_split_hor(const Arg arg);
static void move_split_ver(const Arg arg);
static void move_swap_hor(const Arg arg);
static void move_swap_ver(const Arg arg);
static void move_to_desktop(const Arg arg);
static void perform_swap();
static void quit();
static void remove_window(Window w, unsigned int dr, unsigned int tw);
static void rotate_desktop(const Arg arg);
static void rotate_win_hor(const Arg arg);
static void rotate_win_ver(const Arg arg);
static void save_desktop(unsigned int i);
static void load_desktop(unsigned int i);
static void setup();
static void sigchld(int unused);
static void spawn(const Arg arg);
static void start();
static void tile();
static void toggle_fullscreen();
static void update_fullscreen();
static void toggle_panel();
static void unmapnotify(XEvent *e);    // Thunderbird's write window just unmaps...
static void update_current();
static void update_info();
