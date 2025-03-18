#include <X11/Xlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#define ABS(N) (((N)<0)?-(N):(N))
#define MAX(x,y) ((x)<(y)?(y):(x))
#define MIN(x,y) ((x)>(y)?(y):(x))

/* Border settings */
#define BORDER_WIDTH 2
#define BORDER_COLOR 0xFFFFFF  /* White */
#define UNFOCUSED_BORDER_COLOR 0x808080  /* Gray */

typedef struct client {
    Window window;
    int x, y;
    unsigned int width, height;
    struct client *next;
} client;

Display *d;
int s;
Window root;
client *clients = NULL;
client *current = NULL;

void tile_clients(void) {
    if (!clients) return;

    int n = 0;
    client *c;
    for (c = clients; c; c = c->next) n++;  /* Count windows */

    int screen_width = DisplayWidth(d, s);
    int screen_height = DisplayHeight(d, s);
    int border_adjust = 2 * BORDER_WIDTH;

    if (n == 1) {
        /* Single window: fullscreen */
        c = clients;
        XMoveResizeWindow(d, c->window, 0, 0, screen_width - border_adjust, screen_height - border_adjust);
    } else {
        /* Two or more windows: split horizontally */
        int width = (screen_width - border_adjust) / n;
        int x = 0;
        for (c = clients; c; c = c->next) {
            XMoveResizeWindow(d, c->window, x, 0, width - border_adjust, screen_height - border_adjust);
            x += width;
        }
    }
}

void update_borders(void) {
    for (client *c = clients; c; c = c->next) {
        XSetWindowBorderWidth(d, c->window, BORDER_WIDTH);
        XSetWindowBorder(d, c->window, c == current ? BORDER_COLOR : UNFOCUSED_BORDER_COLOR);
    }
}

void add_client(Window win) {
    client *c = malloc(sizeof(client));
    XGetGeometry(d, win, &(Window){0}, &c->x, &c->y, &c->width, &c->height, &(unsigned int){0}, &(unsigned int){0});
    c->window = win;
    c->next = clients;
    clients = c;
    if (!current) current = c;
    tile_clients();
    update_borders();
}

void remove_client(Window win) {
    client *c = clients, *prev = NULL;
    while (c && c->window != win) {
        prev = c;
        c = c->next;
    }
    if (!c) return;
    if (prev) prev->next = c->next;
    else clients = c->next;
    if (current == c) current = c->next ? c->next : clients;
    free(c);
    tile_clients();  /* Re-tile after removal */
    update_borders();
}

void init(void) {
    Window *child;
    unsigned int nchild;
    XQueryTree(d, root, &(Window){0}, &(Window){0}, &child, &nchild);
    for (unsigned int i = 0; i < nchild; i++) {
        XSelectInput(d, child[i], EnterWindowMask|LeaveWindowMask|SubstructureNotifyMask);
        XMapWindow(d, child[i]);
        add_client(child[i]);
    }
}

client *get_client(Window win) {
    for (client *c = clients; c; c = c->next)
        if (c->window == win) return c;
    return NULL;
}

void config_request(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XConfigureWindow(d, ev->window, ev->value_mask, &(XWindowChanges){
        .x = ev->x, .y = ev->y, .width = ev->width, .height = ev->height, .sibling = ev->above, .stack_mode = ev->detail
    });
    tile_clients();  /* Enforce tiling after config requests */
}

void map_request(XEvent *e) {
    Window win = e->xmaprequest.window;
    XSelectInput(d, win, EnterWindowMask|LeaveWindowMask|SubstructureNotifyMask);
    add_client(win);
    XMapWindow(d, win);
    current = get_client(win);
    XSetInputFocus(d, win, RevertToParent, CurrentTime);
}

void unmap_notify(XEvent *e) {
    Window win = e->xunmap.window;
    remove_client(win);
}

void enter_notify(XEvent *e) {
    if (e->xcrossing.window) {
        current = get_client(e->xcrossing.window);
        XSetInputFocus(d, e->xcrossing.window, RevertToParent, CurrentTime);
        update_borders();
    }
}

int xerror() { return 0; }

static void (*event_handler[LASTEvent])(XEvent *e) = {
    [ConfigureRequest] = config_request,
    [MapRequest]       = map_request,
    [UnmapNotify]      = unmap_notify,
    [EnterNotify]      = enter_notify
};

int main() {
    if (!(d = XOpenDisplay(0))) exit(1);
    s = DefaultScreen(d);
    root = RootWindow(d, s);

    signal(SIGCHLD, SIG_IGN);
    XSetErrorHandler(xerror);

    XSelectInput(d, root, SubstructureRedirectMask|SubstructureNotifyMask);
    XDefineCursor(d, root, XCreateFontCursor(d, 68));

    init();

    XEvent ev;
    while (!XNextEvent(d, &ev))
        if (event_handler[ev.type]) event_handler[ev.type](&ev);
    return 0;
}
