#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>  /* Add this for XA_WINDOW and XA_ATOM */
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#define ABS(N) (((N)<0)?-(N):(N))
#define MAX(x,y) ((x)<(y)?(y):(x))
#define MIN(x,y) ((x)>(y)?(y):(x))

/* Border settings */
#define BORDER_WIDTH 2
#define BORDER_COLOR 0xFFFFFF  /* White */
#define UNFOCUSED_BORDER_COLOR 0x808080  /* Gray */

/* Additional window attributes to track */
#define RESPECT_SIZE_HINTS 1  /* Set to 0 if you want to ignore window size hints */

/* Debug logging */
#define DEBUG 0
#define DLOG(fmt, ...) do { if (DEBUG) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); } while (0)

typedef struct client {
    Window window;
    int x, y;
    unsigned int width, height;
    bool is_fullscreen;
    bool is_urgent;
    bool is_fixed;  /* Prevents window from being resized/moved */
    char *name;     /* Window title */
    struct client *next;
} client;

/* Window manager state */
Display *d;
int s;
Window root;
client *clients = NULL;
client *current = NULL;
bool running = true;

/* Function prototypes */
void tile_clients(void);
void update_borders(void);
void handle_error(const char *msg);
void cleanup(void);

/* Error handling function */
void handle_error(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    if (errno) {
        perror("System error");
    }
}

/* Clean up allocated resources */
void cleanup(void) {
    client *c, *next;
    for (c = clients; c; c = next) {
        next = c->next;
        free(c->name);
        free(c);
    }
    if (d) XCloseDisplay(d);
}

/* Get window name using _NET_WM_NAME or WM_NAME */
char *get_window_name(Window w) {
    XTextProperty prop;
    char *name = NULL;
    
    /* Try _NET_WM_NAME first (UTF-8) */
    Atom net_wm_name = XInternAtom(d, "_NET_WM_NAME", False);
    
    if (XGetTextProperty(d, w, &prop, net_wm_name) && prop.nitems > 0) {
        name = strdup((char *)prop.value);
        XFree(prop.value);
        return name;
    }
    
    /* Fall back to WM_NAME */
    if (XGetWMName(d, w, &prop) && prop.nitems > 0) {
        name = strdup((char *)prop.value);
        XFree(prop.value);
        return name;
    }
    
    return strdup("unknown");
}

/* Window tiling algorithm */
void tile_clients(void) {
    DLOG("Tiling windows...");
    if (!clients) return;

    int n = 0;
    client *c;
    for (c = clients; c; c = c->next) n++;  /* Count windows */
    DLOG("Found %d windows to tile", n);

    int screen_width = DisplayWidth(d, s);
    int screen_height = DisplayHeight(d, s);
    int border_adjust = 2 * BORDER_WIDTH;
    
    /* Check for fullscreen windows first */
    for (c = clients; c; c = c->next) {
        if (c->is_fullscreen) {
            XMoveResizeWindow(d, c->window, 0, 0, 
                             screen_width - border_adjust, 
                             screen_height - border_adjust);
            /* No need to process further - fullscreen takes precedence */
            return;
        }
    }

    if (n == 1) {
        /* Single window: maximize */
        c = clients;
        XMoveResizeWindow(d, c->window, 0, 0, 
                         screen_width - border_adjust, 
                         screen_height - border_adjust);
        c->x = 0;
        c->y = 0;
        c->width = screen_width - border_adjust;
        c->height = screen_height - border_adjust;
    } else {
        /* Two or more windows: tiling algorithm */
        int master_width = screen_width / 2;
        
        /* First window goes in the master area */
        c = clients;
        XMoveResizeWindow(d, c->window, 0, 0, 
                         master_width - border_adjust, 
                         screen_height - border_adjust);
        c->x = 0;
        c->y = 0;
        c->width = master_width - border_adjust;
        c->height = screen_height - border_adjust;
        
        /* Stack the other clients in the remaining space */
        if (n > 1) {
            int stack_height = screen_height / (n - 1);
            int y = 0;
            int i = 0;
            
            for (c = c->next; c; c = c->next, i++) {
                int height = stack_height;
                
                /* Last window gets any remaining space */
                if (i == n - 2) {
                    height = screen_height - y;
                }
                
                XMoveResizeWindow(d, c->window, master_width, y, 
                                 screen_width - master_width - border_adjust, 
                                 height - border_adjust);
                c->x = master_width;
                c->y = y;
                c->width = screen_width - master_width - border_adjust;
                c->height = height - border_adjust;
                y += height;
            }
        }
    }
    
    /* Force windows to update their size */
    XSync(d, False);
}

/* Update window borders */
void update_borders(void) {
    for (client *c = clients; c; c = c->next) {
        XSetWindowBorderWidth(d, c->window, BORDER_WIDTH);
        XSetWindowBorder(d, c->window, c == current ? BORDER_COLOR : UNFOCUSED_BORDER_COLOR);
    }
    XSync(d, False);
}

/* Add a window to the client list */
void add_client(Window win) {
    DLOG("Adding window %lu", win);
    
    /* Skip windows that we shouldn't manage */
    XWindowAttributes attr;
    if (!XGetWindowAttributes(d, win, &attr)) {
        DLOG("Failed to get attributes for window %lu", win);
        return;
    }
    
    /* Don't manage windows that are override_redirect */
    if (attr.override_redirect) {
        DLOG("Skipping override_redirect window %lu", win);
        return;
    }
    
    /* Create a new client */
    client *c = malloc(sizeof(client));
    if (!c) {
        handle_error("Failed to allocate memory for client");
        return;
    }
    
    memset(c, 0, sizeof(client)); /* Zero all fields */
    c->window = win;
    c->x = attr.x;
    c->y = attr.y;
    c->width = attr.width;
    c->height = attr.height;
    c->is_fullscreen = false;
    c->is_urgent = false;
    c->is_fixed = false;
    c->name = get_window_name(win);
    
    /* Add to the beginning of the list */
    c->next = clients;
    clients = c;
    
    /* Make it the current client */
    if (!current) current = c;
    
    /* Apply window manager hints */
    XWMHints *wmhints = XGetWMHints(d, win);
    if (wmhints) {
        if (wmhints->flags & XUrgencyHint) {
            c->is_urgent = true;
        }
        XFree(wmhints);
    }
    
    /* Set up event mask for this window */
    XSelectInput(d, win, EnterWindowMask | LeaveWindowMask |
                       StructureNotifyMask | PropertyChangeMask);
    
    /* Rearrange all windows */
    tile_clients();
    update_borders();
}

/* Remove a window from the client list */
void remove_client(Window win) {
    DLOG("Removing window %lu", win);
    
    client *c = clients, *prev = NULL;
    while (c && c->window != win) {
        prev = c;
        c = c->next;
    }
    
    if (!c) {
        DLOG("Window %lu not found in client list", win);
        return;
    }
    
    /* Remove from the list */
    if (prev) prev->next = c->next;
    else clients = c->next;
    
    /* Update current if necessary */
    if (current == c) {
        current = c->next ? c->next : clients;
        if (current) {
            XSetInputFocus(d, current->window, RevertToPointerRoot, CurrentTime);
        }
    }
    
    /* Free resources */
    free(c->name);
    free(c);
    
    /* Rearrange remaining windows */
    tile_clients();
    update_borders();
}

/* Initialize the window manager */
void init(void) {
    Window returned_root, returned_parent;
    Window *child_windows;
    unsigned int nchildren;

    /* Query existing windows */
    if (!XQueryTree(d, root, &returned_root, &returned_parent, &child_windows, &nchildren)) {
        handle_error("Failed to query the window tree");
        return;
    }
    
    /* Set up event masks for existing windows */
    for (unsigned int i = 0; i < nchildren; i++) {
        Window win = child_windows[i];
        XWindowAttributes attr;
        
        /* Skip windows we shouldn't manage */
        if (!XGetWindowAttributes(d, win, &attr)) continue;
        if (attr.override_redirect) continue;
        if (attr.map_state != IsViewable) continue;
        
        /* Set up event mask */
        XSelectInput(d, win, EnterWindowMask | LeaveWindowMask |
                           StructureNotifyMask | PropertyChangeMask);
        
        /* Add to our client list */
        add_client(win);
    }
    
    /* Free the list of children from XQueryTree */
    if (child_windows) XFree(child_windows);
    
    /* Initial focus */
    if (current) {
        XSetInputFocus(d, current->window, RevertToPointerRoot, CurrentTime);
    }
}

/* Find a client by window */
client *get_client(Window win) {
    for (client *c = clients; c; c = c->next)
        if (c->window == win) return c;
    return NULL;
}

/* Handle ConfigureRequest events */
void config_request(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    client *c = get_client(ev->window);
    
    /* Allow configuration for windows we don't manage */
    if (!c) {
        XWindowChanges wc;
        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling = ev->above;
        wc.stack_mode = ev->detail;
        XConfigureWindow(d, ev->window, ev->value_mask, &wc);
        return;
    }
    
    /* For managed windows, we allow size changes but enforce our tiling */
    /* Just acknowledge the request but maintain our layout */
    XConfigureEvent ce;
    ce.type = ConfigureNotify;
    ce.display = d;
    ce.event = ev->window;
    ce.window = ev->window;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->width;
    ce.height = c->height;
    ce.border_width = BORDER_WIDTH;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(d, ev->window, False, StructureNotifyMask, (XEvent *)&ce);
}

/* Handle MapRequest events */
void map_request(XEvent *e) {
    Window win = e->xmaprequest.window;
    XWindowAttributes attr;
    
    /* Get window attributes */
    if (!XGetWindowAttributes(d, win, &attr)) {
        DLOG("Failed to get attributes for window %lu", win);
        return;
    }
    
    /* Skip windows we shouldn't manage */
    if (attr.override_redirect) {
        DLOG("Skipping override_redirect window %lu", win);
        XMapWindow(d, win);
        return;
    }
    
    /* Get window type from EWMH */
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    
    Atom wm_window_type = XInternAtom(d, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_window_type_dialog = XInternAtom(d, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    
    /* Check if it's a dialog or special window type */
    if (XGetWindowProperty(d, win, wm_window_type, 0, 32, False, AnyPropertyType,
                         &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
        
        Atom *atoms = (Atom *)prop;
        bool is_special = false;
        
        for (unsigned long i = 0; i < nitems; i++) {
            if (atoms[i] == wm_window_type_dialog) {
                is_special = true;
                break;
            }
        }
        
        XFree(prop);
        
        if (is_special) {
            /* For special windows like dialogs, we still map them but might handle differently */
            DLOG("Detected dialog/special window: %lu", win);
        }
    }
    
    /* Add the window to our client list */
    add_client(win);
    
    /* Map the window */
    XMapWindow(d, win);
    
    /* Focus the new window */
    current = get_client(win);
    if (current) {
        XSetInputFocus(d, win, RevertToPointerRoot, CurrentTime);
        XRaiseWindow(d, win);
    }
    
    /* Update borders */
    update_borders();
}

/* Handle UnmapNotify events */
void unmap_notify(XEvent *e) {
    Window win = e->xunmap.window;
    
    /* Skip synthetic unmap events */
    if (e->xunmap.event != root) return;
    
    /* Remove the client */
    remove_client(win);
}

/* Handle EnterNotify events (window focus) */
void enter_notify(XEvent *e) {
    XCrossingEvent *ev = &e->xcrossing;
    
    /* Skip events caused by pointer grab/ungrab */
    if (ev->mode != NotifyNormal || ev->detail == NotifyInferior) return;
    
    client *c = get_client(ev->window);
    if (c) {
        current = c;
        XSetInputFocus(d, c->window, RevertToPointerRoot, CurrentTime);
        update_borders();
    }
}

/* Handle DestroyNotify events */
void destroy_notify(XEvent *e) {
    Window win = e->xdestroywindow.window;
    remove_client(win);
}

/* Handle client messages */
void client_message(XEvent *e) {
    XClientMessageEvent *ev = &e->xclient;
    client *c = get_client(ev->window);
    if (!c) return;
    
    /* Handle EWMH fullscreen requests */
    Atom wm_state = XInternAtom(d, "_NET_WM_STATE", False);
    Atom wm_fullscreen = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
    
    if (ev->message_type == wm_state && 
        ((Atom)ev->data.l[1] == wm_fullscreen || (Atom)ev->data.l[2] == wm_fullscreen)) {
        
        /* Check the action - 0: remove, 1: add, 2: toggle */
        int action = ev->data.l[0];
        if (action == 0) { /* remove */
            c->is_fullscreen = false;
        } else if (action == 1) { /* add */
            c->is_fullscreen = true;
        } else if (action == 2) { /* toggle */
            c->is_fullscreen = !c->is_fullscreen;
        }
        
        /* Update the layout */
        tile_clients();
        update_borders();
    }
}

/* Handle property notify events */
void property_notify(XEvent *e) {
    XPropertyEvent *ev = &e->xproperty;
    client *c = get_client(ev->window);
    if (!c) return;
    
    /* Update window title */
    if (ev->atom == XInternAtom(d, "_NET_WM_NAME", False) ||
        ev->atom == XInternAtom(d, "WM_NAME", False)) {
        
        free(c->name);
        c->name = get_window_name(c->window);
        DLOG("Window title changed to: %s", c->name);
    }
    
    /* Handle urgent hint */
    if (ev->atom == XInternAtom(d, "WM_HINTS", False)) {
        XWMHints *wmhints = XGetWMHints(d, c->window);
        if (wmhints) {
            if (wmhints->flags & XUrgencyHint) {
                c->is_urgent = true;
            } else {
                c->is_urgent = false;
            }
            XFree(wmhints);
        }
    }
}

/* Custom error handler */
int xerror(Display *dpy, XErrorEvent *ee) {
    char error_text[256];
    XGetErrorText(dpy, ee->error_code, error_text, sizeof(error_text));
    fprintf(stderr, "X Error: %s (resource id: %lu, request code: %d, error code: %d)\n",
            error_text, ee->resourceid, ee->request_code, ee->error_code);
    return 0; /* Continue execution */
}

/* Handle signals */
void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        running = false;
    }
}

/* Event handler mapping */
static void (*event_handler[LASTEvent])(XEvent *e) = {
    [ConfigureRequest] = config_request,
    [MapRequest]       = map_request,
    [UnmapNotify]      = unmap_notify,
    [DestroyNotify]    = destroy_notify,
    [EnterNotify]      = enter_notify,
    [ClientMessage]    = client_message,
    [PropertyNotify]   = property_notify
};

int main(void) {
    /* Open display */
    if (!(d = XOpenDisplay(NULL))) {
        fprintf(stderr, "Error: cannot open display\n");
        return 1;
    }
    
    /* Get default screen and root window */
    s = DefaultScreen(d);
    root = RootWindow(d, s);
    
    /* Set up signal handlers */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    
    /* Set error handler */
    XSetErrorHandler(xerror);
    
    /* Set up root window event mask */
    XSelectInput(d, root, SubstructureRedirectMask | SubstructureNotifyMask);
    
    /* Set cursor */
    XDefineCursor(d, root, XCreateFontCursor(d, 68)); /* XC_left_ptr */
    
    /* Initialize EWMH atoms */
    Atom wm_check = XInternAtom(d, "_NET_SUPPORTING_WM_CHECK", False);
    Atom wm_name = XInternAtom(d, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(d, "UTF8_STRING", False);
    Atom supported = XInternAtom(d, "_NET_SUPPORTED", False);
    Atom wm_state = XInternAtom(d, "_NET_WM_STATE", False);
    Atom wm_fullscreen = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
    
    /* Create a window to identify ourselves as the window manager */
    Window wmcheck = XCreateSimpleWindow(d, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(d, wmcheck, wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheck, 1);
    XChangeProperty(d, wmcheck, wm_name, utf8_string, 8, PropModeReplace, (unsigned char *)"wind", 4);
    XChangeProperty(d, root, wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&wmcheck, 1);
    
    /* Advertise supported EWMH features */
    Atom supported_atoms[] = { wm_check, wm_state, wm_fullscreen };
    XChangeProperty(d, root, supported, XA_ATOM, 32, PropModeReplace, (unsigned char *)supported_atoms, 3);
    
    /* Initialize window manager */
    init();
    
    /* Main event loop */
    XEvent ev;
    while (running && !XNextEvent(d, &ev)) {
        if (event_handler[ev.type]) {
            event_handler[ev.type](&ev);
        }
    }
    
    /* Clean up */
    XDestroyWindow(d, wmcheck);
    cleanup();
    
    return 0;
}
