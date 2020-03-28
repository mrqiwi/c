// sudo apt-get install libcairo2-dev
// gcc take_screenshot.c -I/usr/include/cairo -lcairo -lX11
// ./prog :0 output.png
#include <stdio.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <X11/Xlib.h>

int main(int argc, char** argv)
{
    if (argc != 3) {
        printf("Usage: ./prog <display> <output file> \n");
        return -1;
    }

    Display *disp = XOpenDisplay(argv[1]);
    if (!disp) {
        printf("Given display cannot be found, exiting: %s\n" , argv[1]);
        return -1;
    }

    int scr = DefaultScreen(disp);
    Window root = DefaultRootWindow(disp);

    cairo_surface_t *surface = cairo_xlib_surface_create(disp, root, DefaultVisual(disp, scr), DisplayWidth(disp, scr), DisplayHeight(disp, scr));
    cairo_surface_write_to_png(surface, argv[2]);

    cairo_surface_destroy(surface);

    return 0;
}