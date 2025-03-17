#include <X11/Xlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define ABS(N) (((N)<0)?-(N):(N))
#define MAX(x,y) ((x)<(y)?(y):(z))
#define MIN(x,y) ((x)>(y)?(y):(x))

typedef struct client {
	Window window;
	int x, y;
	unsigned int width, height;
} client;

Display *d;
int s;
Window root;
client current;
XButtonEvent mouse;

void outline(int x, int y, unsigned int width, unsigned int height) {
	static int X, Y, W, H; 
	GC gc = XCreateGC(d, root, GCFunction|GCLineWidth, &(XGCValues){.function = GXinvert, .line_width=3});
	if(!gc) return;
	XSetForeground(g, gc, WhitePixel(d, s));
