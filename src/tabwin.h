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


        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#ifndef INC_TABWIN_H
#define INC_TABWIN_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

typedef struct _Tabwin Tabwin;
typedef struct _TabwinWidget TabwinWidget;

struct _Tabwin
{
    GList *tabwin_list;
    GList **client_list;
    GList *selected;
    gboolean display_workspace;
};

struct _TabwinWidget
{
    /* The below must be freed when destroying */
    GtkWidget *window;
    GList *widgets;

    /* these don't have to be */
    Tabwin *tabwin;
    GtkWidget *class;
    GtkWidget *label;
    GtkWidget *container;
    GtkWidget *selected;

    gulong selected_callback;
    gint monitor_num;
    gint width, height;
    gint grid_cols;
    gint grid_rows;
};

Tabwin                  *tabwinCreate                           (GList **,
                                                                 GList *,
                                                                 gboolean);
Client                  *tabwinGetSelected                      (Tabwin *);
Client                  *tabwinSelectHead                       (Tabwin *);
Client                  *tabwinSelectNext                       (Tabwin *);
Client                  *tabwinSelectPrev                       (Tabwin *);
Client                  *tabwinRemoveClient                     (Tabwin *,
                                                                 Client *);
void                    tabwinDestroy                           (Tabwin *);

#endif /* INC_TABWIN_H */
