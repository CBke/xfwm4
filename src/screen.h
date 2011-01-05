/*      $Id$

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
        MA 02110-1301, USA.


        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <xfconf/xfconf.h>
#include <libxfce4kbd-private/xfce-shortcuts-provider.h>


#ifdef HAVE_LIBSTARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif

#ifndef INC_SCREEN_H
#define INC_SCREEN_H

#include "display.h"
#include "settings.h"
#include "mywindow.h"
#include "mypixmap.h"
#include "client.h"
#include "hints.h"

#define MODIFIER_MASK           (ShiftMask | \
                                 ControlMask | \
                                 AltMask | \
                                 MetaMask | \
                                 SuperMask | \
                                 HyperMask)

#ifdef HAVE_COMPOSITOR
struct _gaussian_conv {
    int     size;
    double  *data;
};
typedef struct _gaussian_conv gaussian_conv;
#endif /* HAVE_COMPOSITOR */

struct _ScreenInfo
{
    /* The display this screen belongs to */
    DisplayInfo *display_info;

    /* Window stacking, per screen */
    GList *windows_stack;
    Client *last_raise;
    GList *windows;
    Client *clients;
    guint client_count;
    unsigned long client_serial;
    gint key_grabs;
    gint pointer_grabs;

    /* Theme pixmaps and other params, per screen */
    XfwmColor title_colors[2];
    XfwmColor title_shadow_colors[2];
    xfwmPixmap buttons[BUTTON_COUNT][STATE_COUNT];
    xfwmPixmap corners[CORNER_COUNT][2];
    xfwmPixmap sides[SIDE_COUNT][2];
    xfwmPixmap title[TITLE_COUNT][2];
    xfwmPixmap top[TITLE_COUNT][2];

    /* Per screen graphic contexts */
    GC box_gc;
    GdkGC *black_gc;
    GdkGC *white_gc;

    /* Title font height */
    gint font_height;

    /* Screen data */
    Colormap cmap;
    GdkScreen *gscr;
    Screen *xscreen;
    gint depth;
    gint width;
    gint height;
    Visual *visual;

    GtkWidget *gtk_win;
    xfwmWindow sidewalk[4];
    Window xfwm4_win;
    Window xroot;
    Window shape_win;

    gint gnome_margins[4];
    gint margins[4];
    gint screen;
    guint current_ws;
    guint previous_ws;

    /* Monitor search caching */
    GdkRectangle cache_monitor;
    gint num_monitors;
    GArray *monitors_index;

    /* Workspace definitions */
    guint workspace_count;
    gchar **workspace_names;
    int workspace_names_items;
    NetWmDesktopLayout desktop_layout;

    /* Button handler for GTK */
    gulong button_handler_id;

    /* xfconf */
    XfconfChannel *xfwm4_channel;

    /* Shortcuts */
    XfceShortcutsProvider *shortcuts_provider;

    /* Per screen parameters */
    XfwmParams *params;

    /* show desktop flag */
    gboolean show_desktop;

#ifdef ENABLE_KDE_SYSTRAY_PROXY
    /* There can be one systray per screen */
    Atom net_system_tray_selection;
    Window systray;
#endif

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    /* Startup notification data, per screen */
    SnMonitorContext *sn_context;
    GSList *startup_sequences;
    guint startup_sequence_timeout;
#endif

#ifdef HAVE_COMPOSITOR
#if HAVE_OVERLAYS
    Window overlay;
    Window root_overlay;
#endif
    GList *cwindows;
    Window output;

    gaussian_conv *gaussianMap;
    gint gaussianSize;
    guchar *shadowCorner;
    guchar *shadowTop;

    Picture rootPicture;
    Picture rootBuffer;
    Picture blackPicture;
    Picture rootTile;
    XserverRegion allDamage;

    guint wins_unredirected;
    gboolean compositor_active;
    gboolean clipChanged;

    gboolean damages_pending;

    guint compositor_timeout_id;
#endif /* HAVE_COMPOSITOR */
};

gboolean                 myScreenCheckWMAtom                    (ScreenInfo *,
                                                                 Atom atom);
ScreenInfo              *myScreenInit                           (DisplayInfo *,
                                                                 GdkScreen *,
                                                                 unsigned long,
                                                                 gboolean);
ScreenInfo              *myScreenClose                          (ScreenInfo *);
Display                 *myScreenGetXDisplay                    (ScreenInfo *);
GtkWidget               *myScreenGetGtkWidget                   (ScreenInfo *);
GdkWindow               *myScreenGetGdkWindow                   (ScreenInfo *);
gboolean                 myScreenGrabKeyboard                   (ScreenInfo *,
                                                                 guint32);
gboolean                 myScreenGrabPointer                    (ScreenInfo *,
                                                                 unsigned int,
                                                                 Cursor,
                                                                 guint32);
gboolean                 myScreenChangeGrabPointer              (ScreenInfo *,
                                                                 unsigned int,
                                                                 Cursor,
                                                                 guint32);
unsigned int             myScreenUngrabKeyboard                 (ScreenInfo *,
                                                                 guint32);
unsigned int             myScreenUngrabPointer                  (ScreenInfo *,
                                                                 guint32);
void                     myScreenGrabKeys                       (ScreenInfo *);
void                     myScreenUngrabKeys                     (ScreenInfo *);
int                      myScreenGetKeyPressed                  (ScreenInfo *,
                                                                 XKeyEvent *);
Client                  *myScreenGetClientFromWindow            (ScreenInfo *,
                                                                 Window,
                                                                 unsigned short);
gboolean                 myScreenComputeSize                    (ScreenInfo *);
gint                     myScreenGetNumMonitors                 (ScreenInfo *);
gint                     myScreenGetMonitorIndex                (ScreenInfo *,
                                                                 gint);
gboolean                 myScreenRebuildMonitorIndex            (ScreenInfo *);
void                     myScreenInvalidateMonitorCache         (ScreenInfo *);
void                     myScreenFindMonitorAtPoint             (ScreenInfo *,
                                                                 gint,
                                                                 gint,
                                                                 GdkRectangle *);
gboolean                 myScreenUpdateFontHeight               (ScreenInfo *);

#endif /* INC_SCREEN_H */
