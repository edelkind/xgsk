#include <X11/Xlib.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

#include <get_opts.h>

static int usage(void);

add_opt(opt_r,  "r",  "on-release",    OPT_TOGGLE, (opt_toggle_t)0, 0);
add_opt(opt_s,  "s",  "sync-mouse",    OPT_TOGGLE, (opt_toggle_t)0, 0);
add_opt(opt_t,  "t",  "timeout",       OPT_UINT,   (opt_uint_t)  0, 0);
add_opt(opt_h,  "h",  "help",          OPT_TOGGLE, (opt_toggle_t)0, usage);

put_opts(options, &opt_r, &opt_s, &opt_t, &opt_h);

static char *progname;
static int exitval;
static Display *disp;


/****************************************************************************
 *** helper/error funtions
 ****************************************************************************/

static int
usage(void)
{
    fprintf(stderr, "usage: %s [options]\n", progname);
    fprintf(stderr, " -> X Grab Single Key\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -r, --on-release      quit on key release vs. press (bool: %d)\n",
            opt_r.v_opt.opt_toggle);
    fprintf(stderr, "  -s, --sync-mouse      make mouse events synchronous (bool: %d)\n",
            opt_s.v_opt.opt_toggle);
    fprintf(stderr, "  -t, --timeout         timeout in ms, or 0 (uint: %d)\n",
            opt_t.v_opt.opt_uint);

    exit(1);
}

static inline void
die_error(char *str)
{
    perror(str);
    exit(1);
}

static inline void
defer_error(char *str)
{
    perror(str);
    exitval = 1;
}


/****************************************************************************
 *** signal handler
 ****************************************************************************/

static void
alarm_handler(int arg)
{
    (void) XUngrabKeyboard(disp, CurrentTime);
    fprintf(stderr, "timeout\n");
    exit(0);
}



/****************************************************************************
  Loop events
  If event is a keypress, display the key.
  Exit on either key press or key release, depending on opt_r setting.
 ****************************************************************************/
static void
key_loop()
{
    XEvent ev;
    unsigned int code;
    int pressed = 0;
    char *s;

    for (;;) {
        XNextEvent(disp, &ev);

        switch (ev.type) {
              case (KeyRelease):
                  if (!pressed) continue;
                  return;
            case KeyPress:
                  pressed = 1;
                  code = ((XKeyPressedEvent*)&ev)->keycode;
                  s = XKeysymToString(XKeycodeToKeysym(disp, code, 0));

                  if(s) printf("%s\n", s);

                  if (!opt_r.v_opt.opt_toggle)
                      return;
        }
    }
}


/****************************************************************************
  Grab the keyboard and call key_loop() to loop events.
  - Ungrab keyboard before returning.
  - Ungrab keyboard and exit on timeout.
  - If opt_s is set, use synchronous mouse mode (queues all mouse movements
    for later)
 ****************************************************************************/
static inline void
grab_keyboard_loop()
{
#   define TO opt_t.v_opt.opt_uint
    struct itimerval nval = {
                         { 0, 0 },
                         { TO / 1000, (TO % 1000)*1000 },
                     };

    signal(SIGALRM, alarm_handler);

    if (setitimer(ITIMER_REAL, &nval, 0))
        die_error("setitimer");

    if (XGrabKeyboard(disp,
            DefaultRootWindow(disp),
            False,
            opt_s.v_opt.opt_toggle ? GrabModeSync : GrabModeAsync,
            GrabModeAsync,
            CurrentTime)<0) {
        defer_error("XGrabKeyboard");
        return;
    }

    key_loop();

    if (XUngrabKeyboard(disp, CurrentTime)<0)
        defer_error("XUngrabKeyboard");
}




int
main(int argc, char **argv)
{
    char **args;
    int argn;

    progname = argv[0];

    argn = get_opts_errormatic (&args, argv+1, argc-1, options);

    if(!(disp = XOpenDisplay(NULL)))
        die_error("XOpenDisplay");

    grab_keyboard_loop();

    if (XCloseDisplay(disp))
        defer_error("XCloseDisplay");

    return exitval;
} 
