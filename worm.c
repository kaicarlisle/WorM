/* worm.c
*
*  I started this from dminiwm 6/3/22
*  Permission is hereby granted, free of charge, to any person obtaining a
*  copy of this software and associated documentation files (the "Software"),
*  to deal in the Software without restriction, including without limitation
*  the rights to use, copy, modify, merge, publish, distribute, sublicense,
*  and/or sell copies of the Software, and to permit persons to whom the
*  Software is furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
*  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*  DEALINGS IN THE SOFTWARE.
*
*/

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlocale.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#include "worm.h"
#include "config.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(a, lo, hi) (MAX(MIN(hi, a), lo))
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))
// Function that returns the number of items in array X
#define ARRAYLENGTH(X)    (sizeof(X)/sizeof(*X))

// Variable
static Display *display;
static unsigned int bool_quit, current_desktop, previous_desktop, doresize;
static int screen_height, screen_width;
static unsigned int panel_size, screen, border_width, win_focus_colour, win_unfocus_colour;
static unsigned int numwins, showpanel, splitx, splity, fullscreen;
static int xerror(Display *dis, XErrorEvent *ee), (*xerrorxlib)(Display *, XErrorEvent *);
unsigned int numlockmask;		/* dynamic key lock mask */
static Window root;
static client *head, *current, *transient_head, *swap_from, *swap_to;
static XWindowAttributes attr;
static XButtonEvent starter;
static Atom *protocols, wm_delete_window, protos;

// Events array
static void (*events[LASTEvent])(XEvent *e) = {
    [KeyPress] = keypress,
    [MapRequest] = maprequest,
    [UnmapNotify] = unmapnotify,
    [ButtonPress] = buttonpress,
    [MotionNotify] = motionnotify,
    [ButtonRelease] = buttonrelease,
    [DestroyNotify] = destroynotify,
    [ConfigureRequest] = configurerequest
};

// Desktop array
static desktop desktops[DESKTOPS];

/* ***************************** Window Management ******************************* */
// Adds the window w to the linked list of clients on this desktop
// if this desktop is full, go to the next and add it there
void add_window(Window w) {
    // Very much a possibility that you have 40 windows open, in which case this will cycle through 
    // all your desktops until the end of time
    Arg arg = {.i = 1};
    if(numwins == 4) {
        rotate_desktop(arg);
        add_window(w);
        return;
    }

    client *c, *t;

    if(!(c = (client *)calloc(1,sizeof(client)))) {
        logger("\033[0;31mError calloc!");
        exit(1);
    }

    c->win = w;
    
    if(head == NULL) {
        c->next = NULL; c->prev = NULL;
        head = c;
    } else {
        for(t=head;t->next;t=t->next); // Start at the last in the stack
        t->next = c; c->next = NULL;
        c->prev = t;
    }
    
    current = c;
    numwins += 1;
    save_desktop(current_desktop);
}

void add_transient_window(Window w) {
    client *c, *t;

    if(!(c = (client *)calloc(1,sizeof(client)))) {
        logger("\033[0;31mError calloc!");
        exit(1);
    }

    // place the transient window in the center of the screen
    XClassHint chh = {0};
    if(XGetClassHint(display, w, &chh)) {
        if(chh.res_class) XFree(chh.res_class);
        if(chh.res_name) XFree(chh.res_name);
    }
    XGetWindowAttributes(display, w, &attr);
    XMoveWindow(display, w, screen_width/2-(attr.width/2), screen_height/2-(attr.height/2)+panel_size);
    XGetWindowAttributes(display, w, &attr);
    c->x = attr.x;
    if(attr.y < panel_size) c->y = panel_size;
    else c->y = attr.y;
    c->width = attr.width;
    c->height = attr.height;

    c->win = w;
    
    if(transient_head == NULL) {
        c->next = NULL; c->prev = NULL;
        transient_head = c;
    } else {
        for(t=transient_head;t->next;t=t->next); // Start at the last in the stack
        t->next = c; c->next = NULL;
        c->prev = t;
    }

    save_desktop(current_desktop);
}

void remove_window(Window w, unsigned int dr, unsigned int transient_window) {
    client *c, *dummy;

    // Go through the relevant list of clients (head/transient_head)
    dummy = (transient_window == 1) ? transient_head : head;
    for(c=dummy;c;c=c->next) {
        if(c->win == w) {
            if(c->prev == NULL && c->next == NULL) {
                dummy = NULL;
            } else if(c->prev == NULL) {
                dummy = c->next;
                c->next->prev = NULL;
            } else if(c->next == NULL) {
                c->prev->next = NULL;
            } else {
                c->prev->next = c->next;
                c->next->prev = c->prev;
            } break;
        }
    }
    if(transient_window == 1) {
        transient_head = dummy;
        free(c);
        save_desktop(current_desktop);
        update_current();
    } else {
        head = dummy;
        XUngrabButton(display, AnyButton, AnyModifier, c->win);
        XUnmapWindow(display, c->win);
        numwins -= 1;
        if(head != NULL) {
            current = head;
        } else current = NULL;
        if(dr == 0) free(c);
        save_desktop(current_desktop);
        tile();
        update_current();
    }

    if(numwins == 0) {
        last_desktop();
    }
}

void rotate_win_hor(const Arg arg) {
    Arg next_desktop = {.i = (current_desktop + DESKTOPS + 1) % DESKTOPS};
    Arg prev_desktop = {.i = (current_desktop + DESKTOPS - 1) % DESKTOPS};

    switch(numwins) {
        case 1:
            // if right pressed and next desktop has >=1 window, rotate desktop
            // if left pressed and prev desktop has >= window, rotate desktop -1
            if(arg.i == 1 && desktops[next_desktop.i].numwins >= 1) {
                change_desktop(next_desktop);
            } else if(arg.i == -1 && desktops[prev_desktop.i].numwins >= 1) {
                change_desktop(prev_desktop);
            }
            break;
        case 2:
            // if current=window 1 and right pressed, current = window 2 (= current->next)
            // else if current = window 2 and left pressed, current = window 1 (= head)
            if(arg.i == 1 && current == head) {
                current = current->next;
            } else if(arg.i == -1 && current == head->next) {
                current = head;
            }

            // if current=window 1 and left pressed and prev desktop has >= window, rotate desktop -1
            // if current=window 2 and right pressed and next desktop has >=1 window, rotate desktop
            else if(arg.i == 1 && current == head->next && desktops[next_desktop.i].numwins >= 1) {
                change_desktop(next_desktop);
            } else if(arg.i == -1 && current == head && desktops[prev_desktop.i].numwins >= 1) {
                change_desktop(prev_desktop);
            }
            break;
        case 3:
            // if current=window 1 and right pressed, current = window 2 (= current->next)
            // else if current = window 2 or 3 and left pressed, current = window 1 (= head)
            if(arg.i == 1 && current == head) {
                current = current->next;
            } else if(arg.i == -1 && (current == head->next || current == head->next->next)) {
                current = head;
            }

            // if current=window 1 and left pressed and prev desktop has >= window, rotate desktop -1
            // if current=window 2 or 3 and right pressed and next desktop has >=1 window, rotate desktop
            else if(arg.i == 1 && (current == head->next || current == head->next->next) && desktops[next_desktop.i].numwins >= 1) {
                change_desktop(next_desktop);
            } else if(arg.i == -1 && current == head && desktops[prev_desktop.i].numwins >= 1) {
                change_desktop(prev_desktop);
            }
            break;
        case 4:
            // if current=window 1 and right pressed, current = window 2 (= current->next)
            // else if current = window 2 and left pressed, current = window 1 (= head)
            // else if current = window 3 and left pressed, current = window 4 (current->next)
            // else if current = window 4 and right pressed, current = window 3 (current->prev)
            if(arg.i == 1 && current == head) {
                current = current->next;
            } else if(arg.i == -1 && current == head->next) {
                current = head;
            } else if(arg.i == 1 && current == head->next->next->next) {
                current = current->prev;
            } else if(arg.i == -1 && current == head->next->next) {
                current = current->next;
            }

            // if current=window 1 or 4 and left pressed and prev desktop has >= window, rotate desktop -1
            // if current=window 2 or 3 and right pressed and next desktop has >=1 window, rotate desktop
            else if(arg.i == 1 && (current == head->next || current == head->next->next) && desktops[next_desktop.i].numwins >= 1) {
                change_desktop(next_desktop);
            } else if(arg.i == -1 && (current == head || current == head->next->next->next) && desktops[prev_desktop.i].numwins >= 1) {
                change_desktop(prev_desktop);
            }
            break;
    }
    tile();
    update_current();
}

void rotate_win_ver(const Arg arg) {
    switch(numwins) {
        case 1:
            return;
        case 2:
            return;
        case 3:
            // if current=window 3 and up pressed, current = window 2 (= current->prev)
            // else if current = window 2 and down pressed, current = window 3 (= current->next)
            if(arg.i == -1 && current == head->next->next) {
                current = current->prev;
            } else if(arg.i == 1 && current == head->next) {
                current = current->next;
            }
            break;
        case 4:
            // if current=window 3 and up pressed, current = window 2 (= current->prev)
            // else if current = window 2 and down pressed, current = window 3 (= current->next)
            // else if current=window 1 and down pressed, current = window 4
            // else if current=window 4 and up pressed, current = window 1
            if(arg.i == -1 && current == head->next->next) {
                current = current->prev;
            } else if(arg.i == 1 && current == head->next) {
                current = current->next;
            } else if(arg.i == -1 && current == head->next->next->next) {
                current = head;
            } else if(arg.i == 1 && current == head) {
                current = head->next->next->next;
            }
            break;
    }
    tile();
    update_current();

}

void toggle_fullscreen() {
    fullscreen = !fullscreen;
    if(fullscreen) {
        border_width = 0;
    } else {
        border_width = BORDER_WIDTH;
    }
    update_current();
    save_desktop(current_desktop);
    tile();
}

void move_split_hor(const Arg arg) {
    splitx = CLAMP(splitx + arg.i, SPLIT_LIMIT, screen_width - SPLIT_LIMIT);
    tile();
    save_desktop(current_desktop);
}

void move_split_ver(const Arg arg) {
    splity = CLAMP(splity + arg.i, SPLIT_LIMIT, screen_height - SPLIT_LIMIT);
    tile();
    save_desktop(current_desktop);
}

void center_split() {
    splitx = screen_width / 2;
    splity = screen_height / 2;
    tile();
    save_desktop(current_desktop);
}

// void perform_swap() {
//     // TODO: Allow swapping between desktops
//     // TODO: Allow swapping onto a blank desktop
//     if(swap_from == swap_to) return;

//     Window temp;
//     temp = swap_from->win;
//     swap_from->win = swap_to->win;
//     swap_to->win = temp;

//     swap_from = NULL;
//     swap_to = NULL;

//     save_desktop(current_desktop);
//     tile();
//     update_current();
// }

// These should update the pointers swap_from/to and desktop_swap_from/to
// void move_swap_hor(const Arg arg){
//     if(swap_from == NULL) {
//         swap_from = current;
//         swap_to = current;
//     }
// }

// void move_swap_ver(const Arg arg) {
//     if(swap_from == NULL) {
//         swap_from = current;
//         swap_to = current;
//     }
// }

void move_to_desktop(const Arg arg) {
    Window temp = current->win;

    // remove the window
    remove_window(current->win, 0, 0);
    tile();
    save_desktop(current_desktop);

    // add the new window to the new desktop
    change_desktop(arg);
    add_window(temp);
    tile();
    update_current();
}

/* **************************** Desktop Management ************************************* */
void update_info() {
    if(!OUTPUT_INFO) return;
    unsigned int i, j;

    fflush(stdout);
    for(i=0;i<DESKTOPS;++i) {
        j = (i == current_desktop) ? 1 : 0;
        fprintf(stdout, "%d:%d:%d ", i, desktops[i].numwins, j);
    }
    printf("\n");
    fflush(stdout);
}

void change_desktop(const Arg arg) {
    if(arg.i == current_desktop) return;
    client *c; unsigned int tmp = current_desktop;

    // Save current "properties"
    save_desktop(current_desktop); previous_desktop = current_desktop;

    // Take "properties" from the new desktop
    load_desktop(arg.i);

    // Map all windows
    if(head != NULL) {
        for(c=head;c;c=c->next)
            XMapWindow(display,c->win);
        tile();
    }
    if(transient_head != NULL)
        for(c=transient_head;c;c=c->next)
            XMapWindow(display,c->win);

    load_desktop(tmp);
    // Unmap all window
    if(transient_head != NULL)
        for(c=transient_head;c;c=c->next)
            XUnmapWindow(display,c->win);
    if(head != NULL)
        for(c=head;c;c=c->next)
            XUnmapWindow(display,c->win);

    load_desktop(arg.i);
    update_current();
    update_info();
}

void last_desktop() {
    Arg a = {.i = previous_desktop};
    change_desktop(a);
}

void rotate_desktop(const Arg arg) {
    Arg a = {.i = (current_desktop + DESKTOPS + arg.i) % DESKTOPS};
    change_desktop(a);
}

void save_desktop(unsigned int i) {
    desktops[i].numwins = numwins;
    desktops[i].showbar = showpanel;
    desktops[i].splitx = splitx;
    desktops[i].splity = splity;
    desktops[i].fullscreen = fullscreen;
    desktops[i].head = head;
    desktops[i].current = current;
    desktops[i].transient = transient_head;
}

void load_desktop(unsigned int i) {
    numwins = desktops[i].numwins;
    showpanel = desktops[i].showbar;
    splitx = desktops[i].splitx;
    splity = desktops[i].splity;
    fullscreen = desktops[i].fullscreen;
    head = desktops[i].head;
    current = desktops[i].current;
    transient_head = desktops[i].transient;
    current_desktop = i;
}

void tile() {
    if(head == NULL) return;
    client *c = NULL;
    unsigned int ypos = showpanel * panel_size;

    if(!fullscreen) {
        switch(numwins) {
            case 1:
                XMoveResizeWindow(display, head->win, 
                    0, ypos, 
                    screen_width-border_width, screen_height-ypos-border_width);
                break;
            case 2:
                XMoveResizeWindow(display, head->win, 
                    0, ypos, 
                    splitx-border_width, screen_height-ypos-border_width);
                c = head->next;
                XMoveResizeWindow(display, c->win, 
                    splitx, ypos, 
                    screen_width-splitx-border_width, screen_height-ypos-border_width);
                break;
            case 3:
                XMoveResizeWindow(display, head->win, 
                    0, ypos, 
                    splitx-border_width, screen_height-ypos-border_width);
                c = head->next;
                XMoveResizeWindow(display, c->win, 
                    splitx, ypos, 
                    screen_width-splitx-border_width, splity-border_width);
                c = c->next;
                XMoveResizeWindow(display, c->win, 
                    splitx, splity+ypos, 
                    screen_width-splitx-border_width, screen_height-splity-ypos-border_width);
                break;
            case 4:
                XMoveResizeWindow(display, head->win, 
                    0, ypos, 
                    splitx-border_width, splity-border_width);
                c = head->next;
                XMoveResizeWindow(display, c->win, 
                    splitx, ypos, 
                    screen_width-splitx-border_width, splity-border_width);
                c = c->next;
                XMoveResizeWindow(display, c->win, 
                    splitx, splity+ypos, 
                    screen_width-splitx-border_width, screen_height-splity-ypos-border_width);
                c = c->next;
                XMoveResizeWindow(display, c->win, 
                    0, splity+ypos, 
                    splitx-border_width, screen_height-splity-ypos-border_width);
                break;
        }
    
        // Make sure all the windows are shown
        c = head;
        while(c != NULL) {
            XMapWindow(display, c->win);
            c = c->next;
        }
    } else {
        XSetWindowBorderWidth(display,current->win,border_width);
        XMoveResizeWindow(display, current->win, 
            0, ypos, 
            screen_width+BORDER_WIDTH, screen_height-ypos+BORDER_WIDTH);
        XMapWindow(display, current->win);
    }
}

void update_current() {
    if(head == NULL) return;

    client *c = head;
    while(c != NULL) {
        XSetWindowBorderWidth(display,c->win,border_width);

        if(c != current) {
            XSetWindowBorder(display,c->win,win_unfocus_colour);
            XGrabButton(display, AnyButton, AnyModifier, c->win, True, ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
        }
        else {
            // "Enable" current window
            XSetWindowBorder(display,c->win,win_focus_colour);
            XSetInputFocus(display,c->win,RevertToParent,CurrentTime);
            XRaiseWindow(display,c->win);
            XUngrabButton(display, AnyButton, AnyModifier, c->win);
        }
        c = c->next;
    }
    if(transient_head != NULL) {
        for(c=transient_head;c->next;c=c->next)
            XRaiseWindow(display,c->win);
        XSetInputFocus(display,transient_head->win,RevertToParent,CurrentTime);
    }
    XSync(display, False);
}

void toggle_panel() {
    showpanel = !showpanel;
    tile();
    save_desktop(current_desktop);
}

/* ********************** Keyboard Management ********************** */
void grabkeys() {
    unsigned int i,j;
    KeyCode code;

    // numlock workaround
    XModifierKeymap *modmap;
    numlockmask = 0;
    modmap = XGetModifierMapping(display);
    for (i=0;i<8;++i) {
        for (j=0;j<modmap->max_keypermod;++j) {
            if(modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(display, XK_Num_Lock))
                numlockmask = (1 << i);
        }
    }
    XFreeModifiermap(modmap);

    XUngrabKey(display, AnyKey, AnyModifier, root);
    // For each shortcuts
    for(i=0;i<ARRAYLENGTH(keys);++i) {
        code = XKeysymToKeycode(display,keys[i].keysym);
        XGrabKey(display, code, keys[i].mod, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(display, code, keys[i].mod | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(display, code, keys[i].mod | numlockmask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(display, code, keys[i].mod | numlockmask | LockMask, root, True, GrabModeAsync, GrabModeAsync);
    }
    for(i=1;i<4;i+=2) {
        XGrabButton(display, i, MOD1, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(display, i, MOD1 | LockMask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(display, i, MOD1 | numlockmask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(display, i, MOD1 | numlockmask | LockMask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    }
}

void keypress(XEvent *e) {
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev = &e->xkey;

    keysym = XkbKeycodeToKeysym(display, (KeyCode)ev->keycode, 0, 0);
    //fprintf(stderr, "pressed key %s\n", XKeysymToString(keysym));
    for(i=0;i<ARRAYLENGTH(keys); ++i) {
        if(keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)) {
            if(keys[i].function)
                keys[i].function(keys[i].arg);
        }
    }
}

/* ********************** Signal Management ************************** */
void configurerequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    wc.x = ev->x;
    wc.y = ev->y + panel_size;
    wc.width = (ev->width < screen_width-border_width) ? ev->width:screen_width+border_width;
    wc.height = (ev->height < screen_height-border_width-panel_size) ? ev->height:screen_height+border_width;
    wc.border_width = 0;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(display, ev->window, ev->value_mask, &wc);
    XSync(display, False);
}

void maprequest(XEvent *e) {
    XMapRequestEvent *ev = &e->xmaprequest;

    XGetWindowAttributes(display, ev->window, &attr);
    if(attr.override_redirect) return;

    // For fullscreen mplayer (and maybe some other program)
    client *c;

    for(c=head;c;c=c->next) {
        if(ev->window == c->win) {
            XMapWindow(display,ev->window);
            return;
        }
    }

   	Window trans = None;
    if (XGetTransientForHint(display, ev->window, &trans) && trans != None) {
        add_transient_window(ev->window); 
        if((attr.y + attr.height) > screen_height)
            XMoveResizeWindow(display,ev->window,attr.x,panel_size,attr.width,attr.height-10);
        XSetWindowBorderWidth(display,ev->window,border_width);
        XSetWindowBorder(display,ev->window,win_focus_colour);
        XMapWindow(display, ev->window);
        update_current();
        return;
    }

    if(current != NULL) XUnmapWindow(display, current->win);
    XClassHint ch = {0};

    if(ch.res_class) XFree(ch.res_class);
    if(ch.res_name) XFree(ch.res_name);

    add_window(ev->window);
    tile();
    XMapWindow(display,ev->window);
    update_current();
    update_info();
}

void destroynotify(XEvent *e) {
    unsigned int i = 0, tmp = current_desktop;
    client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    save_desktop(tmp);
    for(i=current_desktop;i<current_desktop+DESKTOPS;++i) {
        load_desktop(i%DESKTOPS);
        for(c=head;c;c=c->next)
            if(ev->window == c->win) {
                remove_window(ev->window, 0, 0);
                load_desktop(tmp);
                update_info();
                return;
            }
        if(transient_head != NULL) {
            for(c=transient_head;c;c=c->next)
                if(ev->window == c->win) {
                    remove_window(ev->window, 0, 1);
                    load_desktop(tmp);
                    return;
                }
        }
    }
    load_desktop(tmp);
}

void unmapnotify(XEvent *e) { // for thunderbird's write window and maybe others
    XUnmapEvent *ev = &e->xunmap;
    client *c;

    if(ev->send_event == 1) {
        for(c=head;c;c=c->next)
            if(ev->window == c->win) {
                remove_window(ev->window, 1, 0);
                update_info();
                return;
            }
    }
}

void buttonpress(XEvent *e) {
    XButtonEvent *ev = &e->xbutton;
    client *c;

    for(c=transient_head;c;c=c->next)
        if(ev->window == c->win) {
            XSetInputFocus(display,ev->window,RevertToParent,CurrentTime);
            return;
        }
    // change focus with LMB
    if(ev->window != current->win && ev->button == Button1)
        for(c=head;c;c=c->next)
            if(ev->window == c->win) {
                current = c;
                update_current();
                XSendEvent(display, PointerWindow, False, 0xfff, e);
                XFlush(display);
                return;
            }

    if(ev->subwindow == None) return;
    for(c=head;c;c=c->next)
        if(ev->subwindow == c->win) {
            XGrabPointer(display, ev->subwindow, True, PointerMotionMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
            XGetWindowAttributes(display, ev->subwindow, &attr);
            starter = e->xbutton; doresize = 1;
        }
}

void motionnotify(XEvent *e) {
    int xdiff, ydiff;
    client *c;
    XMotionEvent *ev = &e->xmotion;

    if(ev->window != current->win) {
        for(c=head;c;c=c->next)
           if(ev->window == c->win) {
                current = c;
                update_current();
                return;
           }
    }
    if(doresize < 1) return;
    while(XCheckTypedEvent(display, MotionNotify, e));
    xdiff = ev->x_root - starter.x_root;
    ydiff = ev->y_root - starter.y_root;
    XMoveResizeWindow(display, ev->window, attr.x + (starter.button==1 ? xdiff : 0), attr.y + (starter.button==1 ? ydiff : 0),
        MAX(1, attr.width + (starter.button==3 ? xdiff : 0)),
        MAX(1, attr.height + (starter.button==3 ? ydiff : 0)));
}

void buttonrelease(XEvent *e) {
    if(doresize < 1) {
        XSendEvent(display, PointerWindow, False, 0xfff, e);
        XFlush(display);
        return;
    }
    XUngrabPointer(display, CurrentTime);
    return;
}

void kill_client() {
    if(head == NULL) return;
    kill_client_now(current->win);
    remove_window(current->win, 0, 0);
    update_info();
}

void kill_client_now(Window w) {
    int n, i;
    XEvent ke;

    if (XGetWMProtocols(display, w, &protocols, &n) != 0) {
        for(i=n;i>=0;--i) {
            if (protocols[i] == wm_delete_window) {
                ke.type = ClientMessage;
                ke.xclient.window = w;
                ke.xclient.message_type = protos;
                ke.xclient.format = 32;
                ke.xclient.data.l[0] = wm_delete_window;
                ke.xclient.data.l[1] = CurrentTime;
                XSendEvent(display, w, False, NoEventMask, &ke);
            }
        }
    } else XKillClient(display, w);
    XFree(protocols);
}

void quit() {
    unsigned int i;
    client *c;

    logger("\033[0;34mYou Quit : Bye!");
    for(i=0;i<DESKTOPS;++i) {
        if(desktops[i].head != NULL) load_desktop(i);
        else continue;
        for(c=head;c;c=c->next)
            kill_client_now(c->win);
    }
    XClearWindow(display, root);
    XUngrabKey(display, AnyKey, AnyModifier, root);
    XSync(display, False);
    XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
    bool_quit = 1;
}

unsigned long getcolor(const char* color) {
    XColor c;
    Colormap map = DefaultColormap(display,screen);

    if(!XAllocNamedColor(display,map,color,&c,&c))
        logger("\033[0;31mError parsing color!");

    return c.pixel;
}

void logger(const char* e) {
    fprintf(stderr,"\n\033[0;34m:: WorM : %s \033[0;m\n", e);
}

void setup() {
    unsigned int i;

    // Install a signal
    sigchld(0);

    // Screen and root window
    screen = DefaultScreen(display);
    root = RootWindow(display,screen);

    border_width = BORDER_WIDTH;
    panel_size = PANEL_HEIGHT;
    // Screen width and height
    screen_width = XDisplayWidth(display,screen) - border_width;
    screen_height = XDisplayHeight(display,screen) - border_width;

    char *loc;
    loc = setlocale(LC_ALL, "");
    if (!loc || !strcmp(loc, "C") || !strcmp(loc, "POSIX") || !XSupportsLocale())
        logger("LOCALE FAILED");

    // For having the panel shown at startup or not
    showpanel = SHOW_PANEL;
    if(showpanel) toggle_panel();

    // Colors
    win_focus_colour = getcolor(FOCUS);
    win_unfocus_colour = getcolor(UNFOCUS);

    // Shortcuts
    grabkeys();

    // Set up all desktop
    for(i=0;i<DESKTOPS;++i) {
        desktops[i].numwins = 0;
        desktops[i].showbar = showpanel;
        desktops[i].splitx = screen_width / 2;
        desktops[i].splity = screen_height / 2;
        desktops[i].head = NULL;
        desktops[i].current = NULL;
        desktops[i].transient = NULL;
    }

    // Select first desktop by default
    previous_desktop = 0;
    swap_from = NULL;
    swap_to = NULL;
    load_desktop(0);
    wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    protos = XInternAtom(display, "WM_PROTOCOLS", False);
    // To catch maprequest and destroynotify (if other wm running)
    XSelectInput(display,root,SubstructureNotifyMask|SubstructureRedirectMask);
    // For exiting
    bool_quit = 0;
    logger("\033[0;32mWe're up and running!");
    update_info();
}

void sigchld(int unused) {
	if(signal(SIGCHLD, sigchld) == SIG_ERR) {
		logger("\033[0;31mCan't install SIGCHLD handler");
		exit(1);
        }
	while(0 < waitpid(-1, NULL, WNOHANG));
}

void spawn(const Arg arg) {
    if(fork() == 0) {
        if(fork() == 0) {
            if(display) close(ConnectionNumber(display));
            setsid();
            execvp((char*)arg.com[0],(char**)arg.com);
        }
        exit(0);
    }
}

/* There's no way to check accesses to destroyed windows, thus those cases are ignored (especially on UnmapNotify's).  Other types of errors call Xlibs default error handler, which may call exit.  */
int xerror(Display *dis, XErrorEvent *ee) {
    if(ee->error_code == BadWindow || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
    if(ee->error_code == BadAccess) {
        logger("\033[0;31mIs Another Window Manager Running? Exiting!");
        exit(1);
    } else logger("\033[0;31mBad Window Error!");
    return xerrorxlib(dis, ee); /* may call exit */
}

void start() {
    XEvent ev;

    while(!bool_quit && !XNextEvent(display,&ev)) {
        if(events[ev.type])
            events[ev.type](&ev);
    }
}


int main() {
    // Open display
    if(!(display = XOpenDisplay(NULL))) {
        logger("\033[0;31mCannot open display!");
        exit(1);
    }
    XSetErrorHandler(xerror);

    // Setup env
    setup();

    // Start wm
    start();

    // Close display
    XCloseDisplay(display);

    exit(0);
}
