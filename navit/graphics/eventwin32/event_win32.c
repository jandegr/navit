#include <windows.h>
#include <windowsx.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "debug.h"
#include "callback.h"
#include "plugin.h"
#include "graphics_win32.h"
#include "profile.h"
#include "keys.h"


static HWND g_hwnd = NULL;

extern void WINAPI SystemIdleTimerReset(void);
static struct event_timeout *
            event_win32_add_timeout(int timeout, int multi, struct callback *cb);

static void
event_win32_main_loop_run(void)
{
    MSG msg;

    dbg(lvl_debug,"enter\n");
    while (GetMessage(&msg, 0, 0, 0))
    {
        TranslateMessage(&msg);       /*  Keyboard input.      */
        DispatchMessage(&msg);
    }

}

static void event_win32_main_loop_quit(void)
{
#ifdef HAVE_API_WIN32_CE
    HWND hwndTaskbar;
    HWND hwndSip;
#endif

    dbg(lvl_debug,"enter\n");

#ifdef HAVE_API_WIN32_CE
    hwndTaskbar = FindWindow(L"HHTaskBar", NULL);
    hwndSip = FindWindow(L"MS_SIPBUTTON", NULL);
    // activate the SIP button
    ShowWindow(hwndSip, SW_SHOW);
    ShowWindow(hwndTaskbar, SW_SHOW);
#endif

    DestroyWindow(g_hwnd);
}

static struct event_watch *
            event_win32_add_watch(int h, enum event_watch_cond cond, struct callback *cb)
{
    dbg(lvl_debug,"enter\n");
    return NULL;
}

static void
event_win32_remove_watch(struct event_watch *ev)
{
    dbg(lvl_debug,"enter\n");
}

static GList *timers;
struct event_timeout
{
    UINT_PTR timer_id;
    int multi;
    struct callback *cb;
    struct event_timeout *next;
};

static void run_timer(UINT_PTR idEvent)
{
    GList *l;
    struct event_timeout *t;
    l = timers;
    while (l)
    {
        t = l->data;
        if (t->timer_id == idEvent)
        {
            struct callback *cb = t->cb;
            dbg(lvl_info, "Timer %d (multi: %d)\n", t->timer_id, t->multi);
            if (!t->multi)
            {
                KillTimer(NULL, t->timer_id);
                timers = g_list_remove(timers, t);
                g_free(t);
            }
            callback_call_0(cb);
            return;
        }
        l = g_list_next(l);
    }
    dbg(lvl_error, "timer %d not found\n", idEvent);
}

static VOID CALLBACK win32_timer_cb(HWND hwnd, UINT uMsg,
                                    UINT_PTR idEvent,
                                    DWORD dwTime)
{
    run_timer(idEvent);
}

static struct event_timeout *
            event_win32_add_timeout(int timeout, int multi, struct callback *cb)
{
    struct event_timeout *t;
    t = g_new0(struct event_timeout, 1);
    if (!t)
        return t;
    t->multi = multi;
    timers = g_list_prepend(timers, t);
    t->cb = cb;
    t->timer_id = SetTimer(NULL, 0, timeout, win32_timer_cb);
    dbg(lvl_debug, "Started timer %d for %d (multi: %d)\n", t->timer_id, timeout, multi);
    return t;
}

static void
event_win32_remove_timeout(struct event_timeout *to)
{
    if (to)
    {
        GList *l;
        struct event_timeout *t=NULL;
        dbg(lvl_debug, "Try stopping timer %d\n", to->timer_id);
        l = timers;
        while (l)
        {
            t = l->data;
            /* Use the pointer not the ID, IDs are reused */
            if (t == to)
            {
                KillTimer(NULL, t->timer_id);
                timers = g_list_remove(timers, t);
                g_free(t);
                return;
            }
            l = g_list_next(l);
        }
        dbg(lvl_error, "Timer %d not found\n", to->timer_id);
        g_free(to);
    }
}

static struct event_idle *
            event_win32_add_idle(int priority, struct callback *cb)
{
	return (struct event_idle *)event_win32_add_timeout(1, 1, cb);
}

static void
event_win32_remove_idle(struct event_idle *ev)
{
    event_win32_remove_timeout((struct event_timeout *)ev);
}

static void
event_win32_call_callback(struct callback_list *cb)
{
    PostMessage(g_hwnd, WM_USER+2, (WPARAM)cb , (LPARAM)0);
}

static struct event_methods event_win32_methods =
{
    event_win32_main_loop_run,
    event_win32_main_loop_quit,
    event_win32_add_watch,
    event_win32_remove_watch,
    event_win32_add_timeout,
    event_win32_remove_timeout,
    event_win32_add_idle,
    event_win32_remove_idle,
    event_win32_call_callback,
};

static struct event_priv *
            event_win32_new(struct event_methods *meth)
{
	dbg(lvl_error,"NEW win32 event");
    *meth=event_win32_methods;
    return NULL;
}

void
plugin_init(void)
{
    plugin_register_event_type("eventwin32", event_win32_new);
}
