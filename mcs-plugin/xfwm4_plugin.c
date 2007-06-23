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
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

        xfce4 mcs plugin   - (c) 2002-2006 Olivier Fourdan
        Buttons DnD inspired by Michael Terry's implementation for xpad
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include <libxfce4mcs/mcs-common.h>
#include <libxfce4mcs/mcs-manager.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>

#include "xfwm4_plugin.h"
#include "xfwm4_shortcuteditor.h"

#define INDICATOR_SIZE 11

typedef struct _MenuTmpl MenuTmpl;
struct _MenuTmpl
{
    gchar *label;
    gchar *action;
};

typedef struct _TitleButton TitleButton;
struct _TitleButton
{
    gchar *key;
    gchar *desc;
    gchar *stock_icon;
};

static const TitleButton title_button[] = {
    {"O", N_("Menu"), "gtk-index"},
    {"T", N_("Stick"), "gtk-add"},
    {"S", N_("Shade"), "gtk-goto-top"},
    {"H", N_("Hide"), "gtk-undo"},
    {"M", N_("Maximize"), "gtk-zoom-100"},
    {"C", N_("Close"), "gtk-close"}
};
#define BUTTON_COUNT 6


static const MenuTmpl dbl_click_values[] = {
    {N_("Shade window"), "shade"},
    {N_("Hide window"), "hide"},
    {N_("Maximize window"), "maximize"},
    {N_("Fill window"), "fill"},
    {N_("Nothing"), "none"},
    {NULL, NULL}
};

static const MenuTmpl title_align_values[] = {
    {N_("Left"), "left"},
    {N_("Center"), "center"},
    {N_("Right"), "right"},
    {NULL, NULL}
};

static void xfwm4_create_channel (McsPlugin * mcs_plugin);
static void run_dialog (McsPlugin * mcs_plugin);

static gboolean setting_model = FALSE;
static gboolean is_running = FALSE;

static gchar *current_theme = NULL;
static gchar *current_layout = NULL;
static gchar *current_font = NULL;
static gchar *dbl_click_action = NULL;
static gchar *title_align = NULL;
static gboolean click_to_focus = TRUE;
static gboolean focus_new = TRUE;
static gboolean focus_raise = FALSE;
static gboolean raise_on_click = TRUE;
static gboolean snap_to_border = TRUE;
static gboolean snap_to_windows = FALSE;
static gboolean wrap_workspaces = FALSE;
static gboolean wrap_windows = TRUE;
static gboolean box_move = FALSE;
static gboolean box_resize = FALSE;
static int raise_delay;
static int focus_delay;
static int snap_width;
static int wrap_resistance;
gchar *xfwm4_plugin_current_key_theme = NULL;

static GList *decoration_theme_list = NULL;
GList *keybinding_theme_list = NULL;

static void
sensitive_cb (GtkWidget * widget, gpointer user_data)
{
    gtk_widget_set_sensitive (widget, (gboolean) GPOINTER_TO_INT (user_data));
}

static GtkWidget *
create_check_button_with_mnemonic (const gchar * label)
{
    GtkWidget *check_button;
    GtkWidget *label_widget;

    label_widget = gtk_label_new_with_mnemonic (label);
    gtk_label_set_line_wrap (GTK_LABEL (label_widget), TRUE);
    gtk_widget_show (label_widget);

    check_button = gtk_check_button_new ();
    gtk_container_add (GTK_CONTAINER (check_button), label_widget);

    return check_button;
}

static void
delete_motion_indicator (GtkWidget * widget)
{
    GdkWindow *indicator = NULL;

    indicator = g_object_get_data (G_OBJECT (widget), "indicator_window");
    if (indicator)
    {
        gdk_window_destroy (indicator);
        g_object_set_data (G_OBJECT (widget), "indicator_window", NULL);
    }
}

static GdkWindow *
create_motion_indicator (GtkWidget * widget, gint x, gint y, gint width, gint height)
{
    GdkWindow *win;
    GdkWindowAttr attributes;
    GdkBitmap *mask = NULL;
    guint attributes_mask;
    gint i, j = 1;
    GdkGC *gc;
    GdkColor col;

    delete_motion_indicator (widget);
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual (GTK_WIDGET (widget));
    attributes.colormap = gtk_widget_get_colormap (GTK_WIDGET (widget));
    attributes.event_mask = 0;
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    attributes.width = width;
    attributes.height = height;
    win = gdk_window_new (GDK_WINDOW (widget->window), &attributes, attributes_mask);
    gdk_window_set_user_data (win, GTK_WIDGET (widget));
    g_object_set_data (G_OBJECT (widget), "indicator_window", win);

    mask = gdk_pixmap_new (win, width, height, 1);
    gc = gdk_gc_new (mask);
    col.pixel = 1;
    gdk_gc_set_foreground (gc, &col);
    gdk_draw_rectangle (mask, gc, TRUE, 0, 0, width, height);

    col.pixel = 0;
    gdk_gc_set_foreground (gc, &col);
    for (i = 0; i < width; i++)
    {
        if (i == (width / 2 - 1))
        {
            continue;
        }
        gdk_draw_line (mask, gc, i, j, i, height - j);
        if (i < (width / 2 - 1))
        {
            j++;
        }
        else
        {
            j--;
        }
    }
    g_object_unref (gc);
    gdk_window_shape_combine_mask (win, mask, 0, 0);
    if (mask)
        g_object_unref (mask);

    gdk_window_move (win, x, y);
    gdk_window_show (win);
    gdk_window_raise (win);

    return win;
}

GdkPixbuf *
create_icon_from_widget (GtkWidget * widget)
{
    GdkPixbuf *dest, *src;

    dest =
        gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, widget->allocation.width,
        widget->allocation.height);

    src = gdk_pixbuf_get_from_drawable (NULL, GDK_DRAWABLE (widget->window), NULL,
        widget->allocation.x, widget->allocation.y, 0, 0, widget->allocation.width,
        widget->allocation.height);
    gdk_pixbuf_copy_area (src, 0, 0, widget->allocation.width, widget->allocation.height, dest, 0,
        0);

    g_object_unref (G_OBJECT (src));

    return dest;
}


static void
button_drag_begin (GtkWidget * widget, GdkDragContext * context, gpointer data)
{
    GdkPixbuf *pixbuf;

    pixbuf = create_icon_from_widget (widget);
    gtk_drag_source_set_icon_pixbuf (widget, pixbuf);
    g_object_unref (G_OBJECT (pixbuf));
    gtk_widget_hide (widget);
}

static void
button_drag_end (GtkWidget * widget, GdkDragContext * context, gpointer data)
{
    gtk_widget_show (GTK_WIDGET (widget));
}

static gboolean
signal_blocker (GtkWidget * widget, gpointer data)
{
    return TRUE;
}

static gboolean
layout_drag_motion (GtkWidget * widget, GdkDragContext * context, gint x, gint y, guint time,
    gpointer user_data)
{
    GtkWidget *child;
    GList *children, *item;
    gint posx, posy, delta;
    gint ix, iy, iwidth, iheight;
    GdkWindow *indicator = NULL;

    g_return_val_if_fail (GTK_IS_WIDGET (user_data), FALSE);

    children = gtk_container_get_children (GTK_CONTAINER (user_data));
    item = children;
    child = GTK_WIDGET (item->data);
    delta = widget->allocation.x;
    posx = child->allocation.x;
    posy = child->allocation.y;

    while (item)
    {
        gint total_x;

        child = GTK_WIDGET (item->data);
        if (GTK_WIDGET_VISIBLE (child))
        {
            total_x = child->allocation.x + (child->allocation.width / 2) - delta;
            posx = child->allocation.x;
            if (x < total_x)
            {
                break;
            }

            posx = child->allocation.x + child->allocation.width;
        }
        item = g_list_next (item);
    }
    g_list_free (children);

    ix = posx - (INDICATOR_SIZE / 2);
    iy = posy - (INDICATOR_SIZE / 2);
    iwidth = INDICATOR_SIZE;
    iheight = child->allocation.height + INDICATOR_SIZE;

    indicator = g_object_get_data (G_OBJECT (user_data), "indicator_window");
    if (!indicator)
    {
        create_motion_indicator (GTK_WIDGET (user_data), ix, iy, iwidth, iheight);
    }
    else
    {
        gdk_window_move (indicator, ix, iy);
    }

    return FALSE;
}

static gboolean
layout_drag_leave (GtkWidget * widget, GdkDragContext * context, guint time, gpointer user_data)
{
    g_return_val_if_fail (GTK_IS_WIDGET (user_data), FALSE);

    delete_motion_indicator (GTK_WIDGET (user_data));

    return FALSE;
}

static void
data_get (GtkWidget * widget, GdkDragContext * drag_context, GtkSelectionData * data, guint info,
    guint time, gpointer user_data)
{
    gtk_selection_data_set (data, gdk_atom_intern ("_XFWM4_BUTTON", FALSE), 8, (const guchar *) "",
        0);
}

static void
title_data_get (GtkWidget * widget, GdkDragContext * drag_context, GtkSelectionData * data, guint info,
    guint time, gpointer user_data)
{
    gtk_selection_data_set (data, gdk_atom_intern ("_XFWM4_TITLE", FALSE), 8, (const guchar *) "",
        0);
}

static gchar *
layout_get_semantic (GtkWidget * container)
{
    GList *children, *item;
    gchar *sem;
    gint p = 0;

    children = gtk_container_get_children (GTK_CONTAINER (container));
    sem = g_new0 (gchar, BUTTON_COUNT + 2);
    item = children;
    while (item)
    {
        GtkWidget *child;
        gchar *key;

        child = GTK_WIDGET (item->data);
        key = g_object_get_data (G_OBJECT (child), "key_char");
        if (key)
        {
            sem[p++] = *key;
            if (p >= BUTTON_COUNT + 1)
            {
                g_list_free (children);
                return (sem);
            }
        }
        item = g_list_next (item);
    }
    g_list_free (children);
    return (sem);
}

static void
layout_reorder_buttons (GtkWidget * container, GtkWidget * widget, gint drop_x)
{
    GList *children, *item;
    gint position, delta;

    children = gtk_container_get_children (GTK_CONTAINER (container));
    item = children;
    position = 0;
    delta = container->allocation.x;

    while (item)
    {
        GtkWidget *child;
        gint total_x;

        child = GTK_WIDGET (item->data);
        if (GTK_WIDGET_VISIBLE (child))
        {
            total_x = child->allocation.x + (child->allocation.width / 2) - delta;
            if (drop_x <= total_x)
            {
                gtk_box_reorder_child (GTK_BOX (container), widget, position);
                g_list_free (children);
                return;
            }
            position++;
        }
        item = g_list_next (item);
    }
    gtk_box_reorder_child (GTK_BOX (container), widget, position);
    g_list_free (children);
}

static void
layout_set_value (GtkWidget * layout, GtkWidget * hidden, gchar * semantic)
{
    GList *children, *item;
    GtkWidget *title;
    gchar *sem;

    /*
     * 1) Block redraw on boxes
     */
    gtk_widget_set_app_paintable (layout, FALSE);
    gtk_widget_set_app_paintable (hidden, FALSE);

    /*
     * 2) Send all buttons but the title back to the hidden frame
     */
    children = gtk_container_get_children (GTK_CONTAINER (layout));
    title = NULL;
    item = children;
    while (item)
    {
        GtkWidget *child;
        gchar *key;

        child = GTK_WIDGET (item->data);
        key = g_object_get_data (G_OBJECT (child), "key_char");
        if (*key != '|')
        {
            gtk_widget_ref (child);
            gtk_container_remove (GTK_CONTAINER (layout), child);
            gtk_box_pack_start (GTK_BOX (hidden), child, FALSE, FALSE, 0);
            gtk_widget_unref (child);
        }
        else
        {
            title = child;
        }
        item = g_list_next (item);
    }
    g_list_free (children);

    /*
     * 3) Move choosen buttons to the layout box in correct order
     */
    children = gtk_container_get_children (GTK_CONTAINER (hidden));
    sem = semantic;
    while (*sem)
    {
        item = children;
        if (*sem != '|')
        {
            while (item)
            {
                GtkWidget *child;
                gchar *key;

                child = GTK_WIDGET (item->data);
                key = g_object_get_data (G_OBJECT (child), "key_char");
                if (*key == *sem)
                {
                    gtk_widget_ref (child);
                    gtk_container_remove (GTK_CONTAINER (hidden), child);
                    gtk_box_pack_start (GTK_BOX (layout), child, FALSE, FALSE, 0);
                    gtk_widget_unref (child);
                }
                item = g_list_next (item);
            }
        }
        else if (title)
        {
            gtk_widget_ref (title);
            gtk_container_remove (GTK_CONTAINER (layout), title);
            gtk_box_pack_start (GTK_BOX (layout), title, FALSE, FALSE, 0);
            gtk_widget_unref (title);
        }

        sem++;
    }
    g_list_free (children);

    /*
     * 4) Unblock redraw on boxes
     */
    gtk_widget_set_app_paintable (layout, TRUE);
    gtk_widget_set_app_paintable (hidden, TRUE);
}

static void
hidden_data_receive (GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y,
    GtkSelectionData * data, guint info, guint time, gpointer user_data)
{
    GtkWidget *source = gtk_drag_get_source_widget (drag_context);
    GtkWidget *parent = gtk_widget_get_parent (source);
    McsPlugin *mcs_plugin = NULL;

    g_return_if_fail (GTK_IS_WIDGET (user_data));

    mcs_plugin = (McsPlugin *) g_object_get_data (G_OBJECT (user_data), "mcs");
    gtk_widget_ref (source);
    gtk_container_remove (GTK_CONTAINER (parent), source);
    gtk_box_pack_start (GTK_BOX (user_data), source, FALSE, FALSE, 0);
    gtk_widget_unref (source);

    if (parent == GTK_WIDGET (user_data))
    {
        return;
    }

    if (current_layout)
    {
        g_free (current_layout);
    }

    current_layout = layout_get_semantic (parent);
    mcs_manager_set_string (mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL1, current_layout);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
layout_data_receive (GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y,
    GtkSelectionData * data, guint info, guint time, gpointer user_data)
{
    GtkWidget *source = gtk_drag_get_source_widget (drag_context);
    GtkWidget *parent = gtk_widget_get_parent (source);
    McsPlugin *mcs_plugin = NULL;

    g_return_if_fail (GTK_IS_WIDGET (user_data));

    mcs_plugin = (McsPlugin *) g_object_get_data (G_OBJECT (user_data), "mcs");

    gtk_widget_set_app_paintable (GTK_WIDGET (user_data), FALSE);
    gtk_widget_ref (source);
    gtk_container_remove (GTK_CONTAINER (parent), source);
    gtk_box_pack_start (GTK_BOX (user_data), source, FALSE, FALSE, 0);
    gtk_widget_unref (source);
    delete_motion_indicator (GTK_WIDGET (user_data));
    layout_reorder_buttons (user_data, source, x);
    gtk_widget_set_app_paintable (GTK_WIDGET (user_data), TRUE);

    if (current_layout)
    {
        g_free (current_layout);
    }

    current_layout = layout_get_semantic (user_data);
    mcs_manager_set_string (mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL1, current_layout);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static GtkWidget *
create_layout_buttons (gchar * layout, gpointer user_data)
{
    GtkWidget *vbox;
    GtkWidget *layout_frame;
    GtkWidget *hidden_frame;
    GtkWidget *layout_box;
    GtkWidget *hidden_box;
    GtkWidget *title;
    GtkWidget *label;
    GtkTargetEntry entry[2];
    GtkTooltips *tooltips;
    gint i;

    entry[0].target = "_XFWM4_BUTTON";
    entry[0].flags = GTK_TARGET_SAME_APP;
    entry[0].info = 2;

    entry[1].target = "_XFWM4_TITLE";
    entry[1].flags = GTK_TARGET_SAME_APP;
    entry[1].info = 3;

    tooltips = gtk_tooltips_new ();

    vbox = gtk_vbox_new (TRUE, 0);

    label = gtk_label_new (_("Click and drag buttons to change the layout"));
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
    gtk_widget_show (label);

    layout_frame = gtk_frame_new (_("Active"));
    gtk_box_pack_start (GTK_BOX (vbox), layout_frame, TRUE, TRUE, 0);
    gtk_widget_show (layout_frame);

    layout_box = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (layout_box), 5);
    gtk_container_add (GTK_CONTAINER (layout_frame), layout_box);
    g_object_set_data (G_OBJECT (layout_box), "mcs", user_data);
    g_object_set_data (G_OBJECT (layout_box), "indicator_window", NULL);
    gtk_widget_show (layout_box);

    title = gtk_button_new_with_label (_("Title"));
    gtk_tooltips_set_tip (tooltips, title, _("The window title, it cannot be removed"), NULL);
    g_object_set_data (G_OBJECT (title), "key_char", "|");
    gtk_drag_source_set (title, GDK_BUTTON1_MASK, &entry[1], 1, GDK_ACTION_MOVE);
    g_signal_connect (title, "drag-data-get", G_CALLBACK (title_data_get), NULL);
    g_signal_connect (title, "drag_begin", G_CALLBACK (button_drag_begin), NULL);
    g_signal_connect (title, "drag_end", G_CALLBACK (button_drag_end), NULL);
    g_signal_connect (title, "button_press_event", G_CALLBACK (signal_blocker), NULL);
    g_signal_connect (title, "enter_notify_event", G_CALLBACK (signal_blocker), NULL);
    g_signal_connect (title, "focus", G_CALLBACK (signal_blocker), NULL);
    gtk_box_pack_start (GTK_BOX (layout_box), title, FALSE, FALSE, 0);
    gtk_widget_show (title);

    hidden_frame = gtk_frame_new (_("Hidden"));
    gtk_box_pack_start (GTK_BOX (vbox), hidden_frame, TRUE, TRUE, 0);
    gtk_widget_show (hidden_frame);

    hidden_box = gtk_hbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (hidden_box), 5);
    gtk_container_add (GTK_CONTAINER (hidden_frame), hidden_box);
    g_object_set_data (G_OBJECT (hidden_box), "mcs", user_data);
    gtk_widget_show (hidden_box);

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        GtkWidget *button;
        GtkWidget *image;

        image = gtk_image_new_from_stock (title_button[i].stock_icon, GTK_ICON_SIZE_MENU);
        button = gtk_button_new ();
        gtk_container_add (GTK_CONTAINER (button), image);
        gtk_tooltips_set_tip (tooltips, button, _(title_button[i].desc), _(title_button[i].desc));
        gtk_drag_source_set (button, GDK_BUTTON1_MASK, entry, 1, GDK_ACTION_MOVE);
        g_signal_connect (button, "drag-data-get", G_CALLBACK (data_get), NULL);
        g_signal_connect (button, "drag_begin", G_CALLBACK (button_drag_begin), NULL);
        g_signal_connect (button, "drag_end", G_CALLBACK (button_drag_end), NULL);
        g_signal_connect (button, "button_press_event", G_CALLBACK (signal_blocker), NULL);
        g_signal_connect (button, "enter_notify_event", G_CALLBACK (signal_blocker), NULL);
        g_signal_connect (button, "focus", G_CALLBACK (signal_blocker), NULL);
        g_object_set_data (G_OBJECT (button), "key_char", title_button[i].key);
        gtk_box_pack_start (GTK_BOX (hidden_box), button, FALSE, FALSE, 0);
        gtk_widget_show_all (button);
    }

    layout_set_value (layout_box, hidden_box, layout);
    gtk_drag_dest_set (hidden_frame, GTK_DEST_DEFAULT_ALL, entry, 1, GDK_ACTION_MOVE);
    gtk_drag_dest_set (layout_frame, GTK_DEST_DEFAULT_ALL, entry, 2, GDK_ACTION_MOVE);

    g_signal_connect (hidden_frame, "drag_data_received", G_CALLBACK (hidden_data_receive),
        hidden_box);
    g_signal_connect (layout_frame, "drag_data_received", G_CALLBACK (layout_data_receive),
        layout_box);
    g_signal_connect (layout_frame, "drag_motion", G_CALLBACK (layout_drag_motion), layout_box);
    g_signal_connect (layout_frame, "drag_leave", G_CALLBACK (layout_drag_leave), layout_box);

    gtk_widget_show (vbox);
    return (vbox);
}

static GtkWidget *
create_option_menu_box (const MenuTmpl template[], guint size, gchar * display_label, gchar * value,
    GCallback handler, gpointer user_data)
{
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *omenu;
    guint n;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    omenu = gtk_combo_box_new_text ();
    gtk_box_pack_start (GTK_BOX (hbox), omenu, TRUE, TRUE, 0);
    gtk_widget_show (omenu);

    for (n = 0; n < size; n++)
    {
        gtk_combo_box_append_text (GTK_COMBO_BOX (omenu), _(template[n].label));

        if (strcmp (value, template[n].action) == 0)
            gtk_combo_box_set_active (GTK_COMBO_BOX (omenu), n);
    }

    g_signal_connect (G_OBJECT (omenu), "changed", G_CALLBACK (handler), user_data);

    return (vbox);
}

void
xfwm4_plugin_theme_info_free (ThemeInfo * info)
{
    g_free (info->path);
    g_free (info->name);
    g_free (info);
}

ThemeInfo *
xfwm4_plugin_find_theme_info_by_name (const gchar * theme_name, GList * theme_list)
{
    GList *list;

    for (list = theme_list; list; list = list->next)
    {
        ThemeInfo *info = list->data;

        if (!strcmp (info->name, theme_name))
            return info;
    }

    return NULL;
}

static gboolean
parserc (const gchar * filename, gboolean * set_layout, gboolean * set_align, gboolean * set_font)
{
    gchar buf[80];
    gchar *lvalue, *rvalue;
    FILE *fp;

    *set_layout = FALSE;
    *set_align = FALSE;
    *set_font = FALSE;

    fp = fopen (filename, "r");
    if (!fp)
    {
        return FALSE;
    }
    while (fgets (buf, sizeof (buf), fp))
    {
        lvalue = strtok (buf, "=");
        rvalue = strtok (NULL, "\n");
        if ((lvalue) && (rvalue))
        {
            if (!g_ascii_strcasecmp (lvalue, "button_layout"))
            {
                *set_layout = TRUE;
            }
            else if (!g_ascii_strcasecmp (lvalue, "title_alignment"))
            {
                *set_align = TRUE;
            }
            else if (!g_ascii_strcasecmp (lvalue, "title_font"))
            {
                *set_font = TRUE;
            }
        }

    }
    fclose (fp);
    return TRUE;
}

static GList *
update_theme_dir (const gchar * theme_dir, GList * theme_list)
{
    ThemeInfo *info = NULL;
    gchar *theme_name;
    GList *list = theme_list;
    gboolean has_decoration = FALSE;
    gboolean has_keybinding = FALSE;
    gboolean set_layout = FALSE;
    gboolean set_align = FALSE;
    gboolean set_font = FALSE;
    gboolean user_writable = FALSE;

    gchar *tmp;

    tmp = g_build_filename (theme_dir, KEY_SUFFIX, KEYTHEMERC, NULL);
    if (g_file_test (tmp, G_FILE_TEST_IS_REGULAR)
        && parserc (tmp, &set_layout, &set_align, &set_font))
    {
        has_keybinding = TRUE;
        user_writable = (access (tmp, W_OK) == 0);
    }
    g_free (tmp);

    tmp = g_build_filename (theme_dir, SUFFIX, THEMERC, NULL);
    if (g_file_test (tmp, G_FILE_TEST_IS_REGULAR)
        && parserc (tmp, &set_layout, &set_align, &set_font))
    {
        has_decoration = TRUE;
    }
    g_free (tmp);

    theme_name = g_strdup (strrchr (theme_dir, G_DIR_SEPARATOR) + 1);
    info = xfwm4_plugin_find_theme_info_by_name (theme_name, list);

    if (info)
    {
        if (!strcmp (theme_dir, info->path))
        {
            if (!has_decoration && !has_keybinding)
            {
                TRACE ("Removing %s", theme_name);
                list = g_list_remove (list, info);
                xfwm4_plugin_theme_info_free (info);
            }
            else if ((info->has_keybinding != has_keybinding)
                || (info->has_decoration != has_decoration)
                || (info->set_layout != set_layout) || (info->set_align != set_align)
                || (info->set_font != set_font))
            {
                TRACE ("Updating %s", theme_name);
                info->has_keybinding = has_keybinding;
                info->has_decoration = has_decoration;
                info->set_layout = set_layout;
                info->set_align = set_align;
                info->set_font = set_font;
                info->user_writable = user_writable;
            }
        }
    }
    else
    {
        if (has_decoration || has_keybinding)
        {
            TRACE ("Adding %s", theme_name);
            info = g_new0 (ThemeInfo, 1);
            info->path = g_strdup (theme_dir);
            info->name = g_strdup (theme_name);
            info->has_decoration = has_decoration;
            info->has_keybinding = has_keybinding;
            info->set_layout = set_layout;
            info->set_align = set_align;
            info->set_font = set_font;
            info->user_writable = user_writable;
            list = g_list_prepend (list, info);
        }
    }

    g_free (theme_name);
    return list;
}

static GList *
themes_common_list_add_dir (const char *dirname, GList * theme_list)
{
#ifdef HAVE_OPENDIR
    struct dirent *de;
    gchar *tmp;
    DIR *dir;

    g_return_val_if_fail (dirname != NULL, theme_list);
    if ((dir = opendir (dirname)) != NULL)
    {
        while ((de = readdir (dir)))
        {
            if (de->d_name[0] == '.')
                continue;

            tmp = g_build_filename (dirname, de->d_name, NULL);
            theme_list = update_theme_dir (tmp, theme_list);
            g_free (tmp);
        }

        closedir (dir);
    }
#endif

    return (theme_list);
}


static GList *
theme_common_init (GList * theme_list)
{
    gchar **dirs, **d;
    GList *list = theme_list;

    xfce_resource_push_path (XFCE_RESOURCE_THEMES, DATADIR G_DIR_SEPARATOR_S "themes");
    dirs = xfce_resource_dirs (XFCE_RESOURCE_THEMES);
    xfce_resource_pop_path (XFCE_RESOURCE_THEMES);

    for (d = dirs; *d != NULL; ++d)
        list = themes_common_list_add_dir (*d, list);

    g_strfreev (dirs);

    return list;
}

static gboolean
dialog_update_from_theme (Itf * itf, const gchar * theme_name, GList * theme_list)
{
    ThemeInfo *info = NULL;

    g_return_val_if_fail (theme_name != NULL, FALSE);
    g_return_val_if_fail (theme_list != NULL, FALSE);

    info = xfwm4_plugin_find_theme_info_by_name (theme_name, theme_list);
    if (info)
    {
        gtk_container_foreach (GTK_CONTAINER (itf->frame_layout), sensitive_cb,
            GINT_TO_POINTER ((gint) ! (info->set_layout)));
        gtk_container_foreach (GTK_CONTAINER (itf->frame_align), sensitive_cb,
            GINT_TO_POINTER ((gint) ! (info->set_align)));
        gtk_widget_set_sensitive (itf->font_button, !(info->set_font));
        return TRUE;
    }
    return FALSE;
}

static void
decoration_selection_changed (GtkTreeSelection * selection, gpointer data)
{
    GtkTreeModel *model;
    gchar *new_theme;
    GtkTreeIter iter;
    Itf *itf;
    McsPlugin *mcs_plugin;

    g_return_if_fail (data != NULL);

    if (setting_model)
    {
        return;
    }

    itf = (Itf *) data;
    mcs_plugin = itf->mcs_plugin;

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &new_theme, -1);
    }
    else
    {
        new_theme = NULL;
    }

    if (new_theme != NULL)
    {
        if (current_theme && strcmp (current_theme, new_theme))
        {
            g_free (current_theme);
            current_theme = new_theme;
            dialog_update_from_theme (itf, current_theme, decoration_theme_list);
            mcs_manager_set_string (mcs_plugin->manager, "Xfwm/ThemeName", CHANNEL1, current_theme);
            mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
            xfwm4_plugin_write_options (mcs_plugin);
        }
    }
}

static void
keybinding_selection_changed (GtkTreeSelection * selection, gpointer data)
{
    GtkTreeModel *model;
    gchar *new_key_theme;
    GtkTreeIter iter;
    Itf *itf;
    McsPlugin *mcs_plugin;

    g_return_if_fail (data != NULL);

    if (setting_model)
    {
        return;
    }

    itf = (Itf *) data;
    mcs_plugin = itf->mcs_plugin;

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &new_key_theme, -1);
    }
    else
    {
        new_key_theme = NULL;
    }

    if (new_key_theme != NULL)
    {
        if (xfwm4_plugin_current_key_theme
            && strcmp (xfwm4_plugin_current_key_theme, new_key_theme))
        {
            ThemeInfo *ti;

            ti = xfwm4_plugin_find_theme_info_by_name (new_key_theme, keybinding_theme_list);

            if (ti)
            {
                gchar *theme_file;

                theme_file = g_build_filename (ti->path, KEY_SUFFIX, KEYTHEMERC, NULL);
                if (g_file_test (theme_file, G_FILE_TEST_EXISTS))
                {
                    g_free (xfwm4_plugin_current_key_theme);
                    xfwm4_plugin_current_key_theme = new_key_theme;
                    mcs_manager_set_string (mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL2,
                        xfwm4_plugin_current_key_theme);
                    mcs_manager_notify (mcs_plugin->manager, CHANNEL2);
                    xfwm4_plugin_write_options (mcs_plugin);

                    loadtheme_in_treeview (ti, itf);
                    gtk_widget_set_sensitive (itf->treeview3, ti->user_writable);
                    gtk_widget_set_sensitive (itf->del_button, ti->user_writable);
                }
                else
                {
                    g_warning ("The keytheme file doesn't exist !");

                    /* refresh list */
                    while (keybinding_theme_list)
                    {
                        xfwm4_plugin_theme_info_free ((ThemeInfo *) keybinding_theme_list->data);
                        keybinding_theme_list = g_list_next (keybinding_theme_list);
                    }
                    g_list_free (keybinding_theme_list);

                    g_free (xfwm4_plugin_current_key_theme);
                    xfwm4_plugin_current_key_theme = g_strdup ("Default");
                    keybinding_theme_list = NULL;
                    keybinding_theme_list =
                        xfwm4_plugin_read_themes (keybinding_theme_list, itf->treeview2,
                        itf->scrolledwindow2, KEYBINDING_THEMES, xfwm4_plugin_current_key_theme);
                    gtk_widget_set_sensitive (itf->treeview3, FALSE);
                    loadtheme_in_treeview (xfwm4_plugin_find_theme_info_by_name ("Default",
                            keybinding_theme_list), itf);

                    /* tell it to the mcs manager */
                    mcs_manager_set_string (itf->mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL2,
                        xfwm4_plugin_current_key_theme);
                    mcs_manager_notify (itf->mcs_plugin->manager, CHANNEL2);
                    xfwm4_plugin_write_options (itf->mcs_plugin);
                }

                g_free (theme_file);
            }
            else
                g_warning ("Cannot find the keytheme !!");
        }
    }
}

GList *
xfwm4_plugin_read_themes (GList * theme_list, GtkWidget * treeview, GtkWidget * swindow,
    ThemeType type, gchar * current_value)
{
    GList *list;
    GList *new_list = theme_list;
    GtkTreeModel *model;
    GtkTreeView *tree_view;
    GtkTreePath *path;
    GtkTreeIter iter;
    GtkTreeIter iter_found;
    gint i = 0;
    gboolean current_value_found = FALSE;

    new_list = theme_common_init (new_list);
    tree_view = GTK_TREE_VIEW (treeview);
    model = gtk_tree_view_get_model (tree_view);

    setting_model = TRUE;
    gtk_list_store_clear (GTK_LIST_STORE (model));

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_NEVER,
        GTK_POLICY_NEVER);
    gtk_widget_set_size_request (swindow, -1, -1);

    for (list = new_list; list; list = list->next)
    {
        ThemeInfo *info = list->data;
        GtkTreeIter iter;

        if (((type == DECORATION_THEMES) && !(info->has_decoration)) || ((type == KEYBINDING_THEMES)
                && !(info->has_keybinding)))
            continue;

        gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, THEME_NAME_COLUMN, info->name, -1);

        if (strcmp (current_value, info->name) == 0)
        {
            current_value_found = TRUE;
            iter_found = iter;
        }

        if (i == MAX_ELEMENTS_BEFORE_SCROLLING)
        {
            GtkRequisition rectangle;
            gtk_widget_size_request (GTK_WIDGET (tree_view), &rectangle);
            gtk_widget_set_size_request (swindow, -1, rectangle.height);
            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_NEVER,
                GTK_POLICY_AUTOMATIC);
        }
        i++;
    }

    if (!current_value_found)
    {
        gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, THEME_NAME_COLUMN, current_value, -1);
        iter_found = iter;
    }

    path = gtk_tree_model_get_path (model, &iter_found);
    {
        gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
        gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5, 0.0);
        gtk_tree_path_free (path);
    }
    setting_model = FALSE;

    return new_list;
}

static gint
sort_func (GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b, gpointer user_data)
{
    gchar *a_str = NULL;
    gchar *b_str = NULL;

    gtk_tree_model_get (model, a, 0, &a_str, -1);
    gtk_tree_model_get (model, b, 0, &b_str, -1);

    if (a_str == NULL)
        a_str = g_strdup ("");
    if (b_str == NULL)
        b_str = g_strdup ("");

    if (!strcmp (a_str, DEFAULT_THEME))
        return -1;
    if (!strcmp (b_str, DEFAULT_THEME))
        return 1;

    return g_utf8_collate (a_str, b_str);
}

static void
cb_click_to_focus_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    click_to_focus = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->click_focus_radio));
    gtk_widget_set_sensitive (itf->focus_delay_scale, !click_to_focus);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/ClickToFocus", CHANNEL1,
        click_to_focus ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_focus_new_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    focus_new = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->focus_new_check));

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusNewWindow", CHANNEL1, focus_new ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_raise_on_focus_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    focus_raise = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->raise_on_focus_check));
    gtk_widget_set_sensitive (itf->raise_delay_scale, focus_raise);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusRaise", CHANNEL1, focus_raise ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_raise_delay_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    raise_delay = (int) gtk_range_get_value (GTK_RANGE (itf->raise_delay_scale));
    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/RaiseDelay", CHANNEL1, raise_delay);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_focus_delay_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    focus_delay = (int) gtk_range_get_value (GTK_RANGE (itf->focus_delay_scale));
    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusDelay", CHANNEL1, focus_delay);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_raise_on_click_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    raise_on_click = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->click_raise_check));

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/RaiseOnClick", CHANNEL1,
        raise_on_click ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_snap_to_border_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    snap_to_border = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->snap_to_border_check));
    gtk_widget_set_sensitive (itf->snap_width_scale, snap_to_windows || snap_to_border);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapToBorder", CHANNEL1,
        snap_to_border ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_snap_to_windows_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    snap_to_windows = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->snap_to_windows_check));
    gtk_widget_set_sensitive (itf->snap_width_scale, snap_to_windows || snap_to_border);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapToWindows", CHANNEL1,
        snap_to_windows ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_snap_width_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    snap_width = (int) gtk_range_get_value (GTK_RANGE (itf->snap_width_scale));
    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapWidth", CHANNEL1, snap_width);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_wrap_resistance_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    wrap_resistance = (int) gtk_range_get_value (GTK_RANGE (itf->wrap_resistance_scale));
    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapResistance", CHANNEL1, wrap_resistance);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_wrap_workspaces_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    wrap_workspaces = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->wrap_workspaces_check));
    gtk_widget_set_sensitive (itf->wrap_resistance_scale, wrap_workspaces || wrap_windows);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapWorkspaces", CHANNEL1,
        wrap_workspaces ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_wrap_windows_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    wrap_windows = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->wrap_windows_check));
    gtk_widget_set_sensitive (itf->wrap_resistance_scale, wrap_workspaces || wrap_windows);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapWindows", CHANNEL1, wrap_windows ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_box_move_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    box_move = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->box_move_check));

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/BoxMove", CHANNEL1, box_move ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_box_resize_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    box_resize = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->box_resize_check));

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/BoxResize", CHANNEL1, box_resize ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_dblclick_action_value_changed (GtkWidget * widget, gpointer user_data)
{
    McsPlugin *mcs_plugin = (McsPlugin *) user_data;
    gint active;

    g_free (dbl_click_action);
    active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
    dbl_click_action = g_strdup (dbl_click_values[active].action);

    mcs_manager_set_string (mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL1, dbl_click_action);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
cb_title_align_value_changed (GtkWidget * widget, gpointer user_data)
{
    McsPlugin *mcs_plugin = (McsPlugin *) user_data;
    gint active;

    g_free (title_align);
    active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
    title_align = g_strdup (title_align_values[active].action);

    mcs_manager_set_string (mcs_plugin->manager, "Xfwm/TitleAlign", CHANNEL1, title_align);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
    xfwm4_plugin_write_options (mcs_plugin);
}

static void
font_selection_ok (GtkWidget * w, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    gchar *new_font =
        gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG (itf->font_selection));
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    if (new_font != NULL)
    {
        if (current_font && strcmp (current_font, new_font))
        {
            g_free (current_font);
            current_font = new_font;
            gtk_button_set_label (GTK_BUTTON (itf->font_button), current_font);
            mcs_manager_set_string (mcs_plugin->manager, "Xfwm/TitleFont", CHANNEL1, current_font);
            mcs_manager_notify (mcs_plugin->manager, CHANNEL1);
            xfwm4_plugin_write_options (mcs_plugin);
        }
    }

    gtk_widget_destroy (GTK_WIDGET (itf->font_selection));
    itf->font_selection = NULL;
}

static void
show_font_selection (GtkWidget * widget, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;

    if (!(itf->font_selection))
    {
        itf->font_selection = gtk_font_selection_dialog_new (_("Font Selection Dialog"));

        gtk_window_set_position (GTK_WINDOW (itf->font_selection), GTK_WIN_POS_MOUSE);
        gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG (itf->font_selection),
            current_font);
        g_signal_connect (itf->font_selection, "destroy", G_CALLBACK (gtk_widget_destroyed),
            &itf->font_selection);

        g_signal_connect (GTK_FONT_SELECTION_DIALOG (itf->font_selection)->ok_button, "clicked",
            G_CALLBACK (font_selection_ok), user_data);
        g_signal_connect_swapped (GTK_FONT_SELECTION_DIALOG (itf->font_selection)->cancel_button,
            "clicked", G_CALLBACK (gtk_widget_destroy), itf->font_selection);

        gtk_widget_show (itf->font_selection);
    }
    else
    {
        gtk_widget_destroy (itf->font_selection);
        itf->font_selection = NULL;
    }
}

static void
cb_dialog_response (GtkWidget * dialog, gint response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
    {
        GError *error = NULL;
        xfce_exec ("xfhelp4 xfwm4.html", FALSE, FALSE, &error);
        if (error)
        {
            char *msg = g_strcompress (error->message);
            xfce_warn ("%s", msg);
            g_free (msg);
            g_error_free (error);
        }
    }
    else
    {
        is_running = FALSE;
        gtk_widget_destroy (dialog);
    }
}

static Itf *
create_dialog (McsPlugin * mcs_plugin)
{
    Itf *dialog;
    GtkWidget *frame;
    GtkWidget *hbox;
    GtkWidget *label;
    GtkWidget *notebook;
    GtkWidget *table;
    GtkWidget *vbox;
    GtkWidget *vbox_page;
    GtkTreeViewColumn *hidden_column;

    GtkCellRenderer *renderer;
    GtkListStore *model;

    dialog = g_new (Itf, 1);

    dialog->mcs_plugin = mcs_plugin;

    dialog->xfwm4_dialog = xfce_titled_dialog_new ();
    gtk_window_set_title (GTK_WINDOW (dialog->xfwm4_dialog), _("Window Manager"));
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog->xfwm4_dialog), FALSE);
    gtk_window_set_icon_name(GTK_WINDOW(dialog->xfwm4_dialog), "xfwm4");

    dialog->font_selection = NULL;

    dialog->click_focus_radio_group = NULL;

    dialog->dialog_vbox = GTK_DIALOG (dialog->xfwm4_dialog)->vbox;
    gtk_widget_show (dialog->dialog_vbox);

    notebook = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (notebook), BORDER + 1);
    gtk_widget_show (notebook);
    gtk_box_pack_start (GTK_BOX (dialog->dialog_vbox), notebook, TRUE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), BORDER);
    gtk_widget_show (hbox);
    gtk_container_add (GTK_CONTAINER (notebook), hbox);

    dialog->scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (dialog->scrolledwindow1);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->scrolledwindow1), BORDER);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->scrolledwindow1, FALSE, TRUE, 0);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dialog->scrolledwindow1),
        GTK_SHADOW_IN);

    dialog->treeview1 = gtk_tree_view_new ();
    gtk_widget_show (dialog->treeview1);
    gtk_container_add (GTK_CONTAINER (dialog->scrolledwindow1), dialog->treeview1);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->treeview1), FALSE);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);

    frame = xfce_create_framebox_with_content (_("Title font"), hbox);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, FALSE, 0);

    dialog->font_button = gtk_button_new ();
    gtk_button_set_label (GTK_BUTTON (dialog->font_button), current_font);
    gtk_widget_show (dialog->font_button);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->font_button, TRUE, TRUE, 0);

    dialog->frame_align = xfce_create_framebox_with_content (_("Title Alignment"),
        create_option_menu_box (title_align_values, 3,
            /*XXX*/ _("Text alignment inside title bar :"), title_align,
            G_CALLBACK (cb_title_align_value_changed), mcs_plugin));
    gtk_widget_show (dialog->frame_align);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->frame_align, TRUE, TRUE, 0);

    dialog->frame_layout =
        xfce_create_framebox_with_content (_("Button layout"),
        create_layout_buttons (current_layout, mcs_plugin));
    gtk_widget_show (dialog->frame_layout);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->frame_layout, TRUE, FALSE, 0);

    label = gtk_label_new (_("Style"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), BORDER);
    gtk_widget_show (hbox);
    gtk_container_add (GTK_CONTAINER (notebook), hbox);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
    gtk_widget_show (vbox);

    dialog->scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->scrolledwindow2), BORDER);
    gtk_widget_show (dialog->scrolledwindow2);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dialog->scrolledwindow2),
        GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->scrolledwindow2, TRUE, TRUE, 0);

    dialog->treeview2 = gtk_tree_view_new ();
    gtk_widget_show (dialog->treeview2);
    gtk_container_add (GTK_CONTAINER (dialog->scrolledwindow2), dialog->treeview2);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->treeview2), FALSE);

    dialog->add_button = gtk_button_new_from_stock (GTK_STOCK_ADD);
    gtk_widget_show (dialog->add_button);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->add_button, FALSE, FALSE, 0);
    dialog->del_button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    gtk_widget_show (dialog->del_button);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->del_button, FALSE, FALSE, 0);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_show (vbox);

    dialog->scrolledwindow3 = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (dialog->scrolledwindow3),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->scrolledwindow3), BORDER);
    gtk_widget_show (dialog->scrolledwindow3);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dialog->scrolledwindow3),
        GTK_SHADOW_IN);

    frame = xfce_create_framebox_with_content (_("Window shortcuts"), dialog->scrolledwindow3);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

    model = gtk_list_store_new (KEY_SHORTCUT_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    dialog->treeview3 = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_widget_show (dialog->treeview3);
    gtk_container_add (GTK_CONTAINER (dialog->scrolledwindow3), dialog->treeview3);

    /* command column */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set_data (G_OBJECT (renderer), "column", (gint *) COLUMN_COMMAND);

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->treeview3), -1,
        _("Command"), renderer, "text", COLUMN_COMMAND, NULL);
    /* shortcut column */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set_data (G_OBJECT (renderer), "column", (gint *) COLUMN_SHORTCUT);

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->treeview3), -1,
        _("Shortcut"), renderer, "text", COLUMN_SHORTCUT, NULL);
    /* command name hidden column */
    hidden_column =
        gtk_tree_view_column_new_with_attributes ("name", renderer, "text", COLUMN_NAME, NULL);
    gtk_tree_view_column_set_visible (hidden_column, FALSE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->treeview3), hidden_column);

    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->treeview3), TRUE);

    /* popup menu */
    dialog->popup_menu = gtk_menu_new ();
    dialog->popup_add_menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_ADD, NULL);
    gtk_container_add (GTK_CONTAINER (dialog->popup_menu), dialog->popup_add_menuitem);
    dialog->popup_del_menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_REMOVE, NULL);
    gtk_container_add (GTK_CONTAINER (dialog->popup_menu), dialog->popup_del_menuitem);
    gtk_widget_show_all (dialog->popup_menu);

    label = gtk_label_new (_("Keyboard"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    vbox_page = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox_page);
    gtk_container_set_border_width (GTK_CONTAINER (vbox_page), BORDER);
    gtk_widget_show (vbox_page);
    gtk_container_add (GTK_CONTAINER (notebook), vbox_page);

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);

    frame = xfce_create_framebox_with_content (_("Focus model"), hbox);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox_page), frame, TRUE, TRUE, 0);

    dialog->click_focus_radio = gtk_radio_button_new_with_mnemonic (NULL, _("Click to focus"));
    gtk_widget_show (dialog->click_focus_radio);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->click_focus_radio, TRUE, FALSE, 0);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (dialog->click_focus_radio),
        dialog->click_focus_radio_group);
    dialog->click_focus_radio_group =
        gtk_radio_button_get_group (GTK_RADIO_BUTTON (dialog->click_focus_radio));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->click_focus_radio), click_to_focus);

    dialog->focus_follow_mouse_radio =
        gtk_radio_button_new_with_mnemonic (NULL, _("Focus follows mouse"));
    gtk_widget_show (dialog->focus_follow_mouse_radio);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->focus_follow_mouse_radio, TRUE, FALSE, 0);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (dialog->focus_follow_mouse_radio),
        dialog->click_focus_radio_group);
    dialog->click_focus_radio_group =
        gtk_radio_button_get_group (GTK_RADIO_BUTTON (dialog->focus_follow_mouse_radio));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->focus_follow_mouse_radio),
        !click_to_focus);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox);

    frame = xfce_create_framebox_with_content (_("Delay before window receives focus"), vbox);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox_page), frame, TRUE, TRUE, 0);

    table = gtk_table_new (1, 3, FALSE);
    gtk_widget_show (table);
    gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (table), BORDER);

    label = xfce_create_small_label (_("Slow"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

    label = xfce_create_small_label (_("Fast"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    dialog->focus_delay_scale =
        gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (focus_delay, 100, 2000, 10, 100, 0)));
    gtk_widget_show (dialog->focus_delay_scale);

    gtk_table_attach (GTK_TABLE (table), dialog->focus_delay_scale, 1, 2, 0,
        1, (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), (GtkAttachOptions) (GTK_FILL),
        0, 0);
    gtk_scale_set_draw_value (GTK_SCALE (dialog->focus_delay_scale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (dialog->focus_delay_scale), 0);
    gtk_range_set_update_policy (GTK_RANGE (dialog->focus_delay_scale), GTK_UPDATE_DISCONTINUOUS);
    gtk_range_set_inverted (GTK_RANGE (dialog->focus_delay_scale), TRUE);
    gtk_widget_set_sensitive (dialog->focus_delay_scale, !click_to_focus);

    dialog->focus_new_check =
        create_check_button_with_mnemonic (_("Automatically give focus to newly created windows"));
    gtk_widget_show (dialog->focus_new_check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->focus_new_check), focus_new);

    frame = xfce_create_framebox_with_content (_("New window focus"), dialog->focus_new_check);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox_page), frame, TRUE, TRUE, 0);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox);

    frame = xfce_create_framebox_with_content (_("Raise on focus"), vbox);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox_page), frame, TRUE, TRUE, 0);

    dialog->raise_on_focus_check =
        create_check_button_with_mnemonic (
        _("Automatically raise windows when they receive focus"));
    gtk_widget_show (dialog->raise_on_focus_check);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->raise_on_focus_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->raise_on_focus_check), focus_raise);

    table = gtk_table_new (2, 3, FALSE);
    gtk_widget_show (table);
    gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (table), BORDER);

    label = gtk_label_new (_("Delay before raising focused window :"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 3, 0, 1, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = xfce_create_small_label (_("Slow"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

    label = xfce_create_small_label (_("Fast"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    dialog->raise_delay_scale =
        gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (raise_delay, 100, 2000, 10, 100, 0)));
    gtk_widget_show (dialog->raise_delay_scale);
    gtk_table_attach (GTK_TABLE (table), dialog->raise_delay_scale, 1, 2, 1,
        2, (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), (GtkAttachOptions) (GTK_FILL),
        0, 0);
    gtk_scale_set_draw_value (GTK_SCALE (dialog->raise_delay_scale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (dialog->raise_delay_scale), 0);
    gtk_range_set_update_policy (GTK_RANGE (dialog->raise_delay_scale), GTK_UPDATE_DISCONTINUOUS);
    gtk_range_set_inverted (GTK_RANGE (dialog->raise_delay_scale), TRUE);
    gtk_widget_set_sensitive (dialog->raise_delay_scale, focus_raise);

    dialog->click_raise_check =
        create_check_button_with_mnemonic (
        _("Raise window when clicking inside application window"));
    gtk_widget_show (dialog->click_raise_check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->click_raise_check), raise_on_click);

    frame = xfce_create_framebox_with_content (_("Raise on click"), dialog->click_raise_check);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox_page), frame, TRUE, TRUE, 0);

    label = gtk_label_new (_("Focus"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 2), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    vbox_page = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox_page), BORDER);
    gtk_widget_show (vbox_page);
    gtk_container_add (GTK_CONTAINER (notebook), vbox_page);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox);

    frame = xfce_create_framebox_with_content (_("Windows snapping"), vbox);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox_page), frame, TRUE, TRUE, 0);

    dialog->snap_to_border_check =
        create_check_button_with_mnemonic (_("Snap windows to screen border"));
    gtk_widget_show (dialog->snap_to_border_check);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->snap_to_border_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->snap_to_border_check), snap_to_border);

    dialog->snap_to_windows_check =
        create_check_button_with_mnemonic (_("Snap windows to other windows"));
    gtk_widget_show (dialog->snap_to_windows_check);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->snap_to_windows_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->snap_to_windows_check),
        snap_to_windows);

    table = gtk_table_new (2, 3, FALSE);
    gtk_widget_show (table);
    gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (table), BORDER);

    label = gtk_label_new (_("Distance :"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 3, 0, 1, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = xfce_create_small_label (Q_("Distance|Small"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

    label = xfce_create_small_label (Q_("Distance|Wide"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    dialog->snap_width_scale =
        gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (snap_width, 5, 50, 5, 10, 0)));
    gtk_widget_show (dialog->snap_width_scale);
    gtk_table_attach (GTK_TABLE (table), dialog->snap_width_scale, 1, 2, 1,
        2, (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), (GtkAttachOptions) (GTK_FILL),
        0, 0);
    gtk_scale_set_draw_value (GTK_SCALE (dialog->snap_width_scale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (dialog->snap_width_scale), 0);
    gtk_range_set_update_policy (GTK_RANGE (dialog->snap_width_scale), GTK_UPDATE_DISCONTINUOUS);
    gtk_widget_set_sensitive (dialog->snap_width_scale, snap_to_border || snap_to_windows);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox);

    frame = xfce_create_framebox_with_content (_("Wrap workspaces"), vbox);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox_page), frame, TRUE, TRUE, 0);

    dialog->wrap_workspaces_check =
        create_check_button_with_mnemonic (
        _("Wrap workspaces when the pointer reaches a screen edge"));
    gtk_widget_show (dialog->wrap_workspaces_check);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->wrap_workspaces_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->wrap_workspaces_check),
        wrap_workspaces);

    dialog->wrap_windows_check =
        create_check_button_with_mnemonic (
        _("Wrap workspaces when dragging a window off the screen"));
    gtk_widget_show (dialog->wrap_windows_check);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->wrap_windows_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->wrap_windows_check), wrap_windows);

    table = gtk_table_new (2, 3, FALSE);
    gtk_widget_show (table);
    gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (table), BORDER);

    label = gtk_label_new (_("Edge Resistance :"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 3, 0, 1, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = xfce_create_small_label (Q_("Resistance|Small"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

    label = xfce_create_small_label (Q_("Resistance|Wide"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table), label, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    dialog->wrap_resistance_scale =
        gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (wrap_resistance, 1, 50, 5, 10, 0)));
    gtk_widget_show (dialog->wrap_resistance_scale);
    gtk_table_attach (GTK_TABLE (table), dialog->wrap_resistance_scale, 1, 2,
        1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
        (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_scale_set_draw_value (GTK_SCALE (dialog->wrap_resistance_scale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (dialog->wrap_resistance_scale), 0);
    gtk_range_set_update_policy (GTK_RANGE (dialog->wrap_resistance_scale),
        GTK_UPDATE_DISCONTINUOUS);
    gtk_widget_set_sensitive (dialog->wrap_resistance_scale, wrap_workspaces || wrap_windows);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox);

    frame = xfce_create_framebox_with_content (_("Opaque move and resize"), vbox);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox_page), frame, TRUE, TRUE, 0);

    dialog->box_resize_check =
        create_check_button_with_mnemonic (_("Display content of windows when resizing"));
    gtk_widget_show (dialog->box_resize_check);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->box_resize_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->box_resize_check), !box_resize);

    dialog->box_move_check =
        create_check_button_with_mnemonic (_("Display content of windows when moving"));
    gtk_widget_show (dialog->box_move_check);
    gtk_box_pack_start (GTK_BOX (vbox), dialog->box_move_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->box_move_check), !box_move);

    frame = xfce_create_framebox_with_content (_("Double click action"),
        create_option_menu_box (dbl_click_values, 5,
            _("Action to perform when double clicking on title bar :"), dbl_click_action,
            G_CALLBACK (cb_dblclick_action_value_changed), mcs_plugin));
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox_page), frame, TRUE, TRUE, 0);

    label = gtk_label_new (_("Advanced"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    dialog->dialog_action_area1 = GTK_DIALOG (dialog->xfwm4_dialog)->action_area;
    gtk_widget_show (dialog->dialog_action_area1);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog->dialog_action_area1), GTK_BUTTONBOX_END);

    dialog->closebutton1 = gtk_button_new_from_stock ("gtk-close");
    gtk_widget_show (dialog->closebutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog->xfwm4_dialog), dialog->closebutton1,
        GTK_RESPONSE_CLOSE);
    GTK_WIDGET_SET_FLAGS (dialog->closebutton1, GTK_CAN_DEFAULT);

    dialog->helpbutton1 = gtk_button_new_from_stock ("gtk-help");
    gtk_widget_show (dialog->helpbutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog->xfwm4_dialog), dialog->helpbutton1,
        GTK_RESPONSE_HELP);

    gtk_widget_grab_focus (dialog->closebutton1);
    gtk_widget_grab_default (dialog->closebutton1);

    return dialog;
}

static void
setup_dialog (Itf * itf)
{
    GtkTreeModel *model1, *model2, *model3;
    GtkTreeSelection *selection;
    ThemeInfo *ti;

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (itf->treeview1), -1, NULL,
        gtk_cell_renderer_text_new (), "text", THEME_NAME_COLUMN, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (itf->treeview2), -1, NULL,
        gtk_cell_renderer_text_new (), "text", THEME_NAME_COLUMN, NULL);

    model1 = (GtkTreeModel *) gtk_list_store_new (THEME_NAME_COLUMNS, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model1), 0, sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model1), 0, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (itf->treeview1), model1);

    model2 = (GtkTreeModel *) gtk_list_store_new (THEME_NAME_COLUMNS, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model2), 0, sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model2), 0, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (itf->treeview2), model2);

    model3 = gtk_tree_view_get_model (GTK_TREE_VIEW (itf->treeview3));
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model3), 0, sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model3), 0, GTK_SORT_ASCENDING);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview1));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
    g_signal_connect (G_OBJECT (selection), "changed", (GCallback) decoration_selection_changed,
        itf);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview2));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    g_signal_connect (G_OBJECT (selection), "changed", (GCallback) keybinding_selection_changed,
        itf);

    g_signal_connect (G_OBJECT (itf->treeview2), "button-press-event", G_CALLBACK (cb_popup_menu),
        itf);
    g_signal_connect (G_OBJECT (itf->popup_add_menuitem), "activate",
        G_CALLBACK (cb_popup_add_menu), itf);
    g_signal_connect (G_OBJECT (itf->popup_del_menuitem), "activate",
        G_CALLBACK (cb_popup_del_menu), itf);
    g_signal_connect (G_OBJECT (itf->add_button), "clicked", G_CALLBACK (cb_popup_add_menu), itf);
    g_signal_connect (G_OBJECT (itf->del_button), "clicked", G_CALLBACK (cb_popup_del_menu), itf);

    g_signal_connect (G_OBJECT (itf->treeview3), "row-activated",
        G_CALLBACK (cb_activate_treeview3), itf);


    decoration_theme_list =
        xfwm4_plugin_read_themes (decoration_theme_list, itf->treeview1, itf->scrolledwindow1,
        DECORATION_THEMES, current_theme);
    keybinding_theme_list =
        xfwm4_plugin_read_themes (keybinding_theme_list, itf->treeview2, itf->scrolledwindow2,
        KEYBINDING_THEMES, xfwm4_plugin_current_key_theme);
    dialog_update_from_theme (itf, current_theme, decoration_theme_list);

    /* load the theme */
    ti = xfwm4_plugin_find_theme_info_by_name (xfwm4_plugin_current_key_theme,
        keybinding_theme_list);

    if (ti)
    {
        gtk_widget_set_sensitive (itf->treeview3, ti->user_writable);
        loadtheme_in_treeview (ti, itf);
    }
    else
    {
        g_warning ("Cannot find the keytheme !");
    }
    g_signal_connect (G_OBJECT (itf->xfwm4_dialog), "response", G_CALLBACK (cb_dialog_response),
        itf->mcs_plugin);
    g_signal_connect (G_OBJECT (itf->font_button), "clicked", G_CALLBACK (show_font_selection),
        itf);
    g_signal_connect (G_OBJECT (itf->click_focus_radio), "toggled",
        G_CALLBACK (cb_click_to_focus_changed), itf);
    g_signal_connect (G_OBJECT (itf->focus_new_check), "toggled", G_CALLBACK (cb_focus_new_changed),
        itf);
    g_signal_connect (G_OBJECT (itf->raise_on_focus_check), "toggled",
        G_CALLBACK (cb_raise_on_focus_changed), itf);
    g_signal_connect (G_OBJECT (itf->raise_delay_scale), "value_changed",
        (GCallback) cb_raise_delay_changed, itf);
    g_signal_connect (G_OBJECT (itf->focus_delay_scale), "value_changed",
        (GCallback) cb_focus_delay_changed, itf);
    g_signal_connect (G_OBJECT (itf->click_raise_check), "toggled",
        G_CALLBACK (cb_raise_on_click_changed), itf);
    g_signal_connect (G_OBJECT (itf->snap_to_border_check), "toggled",
        G_CALLBACK (cb_snap_to_border_changed), itf);
    g_signal_connect (G_OBJECT (itf->snap_to_windows_check), "toggled",
        G_CALLBACK (cb_snap_to_windows_changed), itf);
    g_signal_connect (G_OBJECT (itf->snap_width_scale), "value_changed",
        (GCallback) cb_snap_width_changed, itf);
    g_signal_connect (G_OBJECT (itf->wrap_workspaces_check), "toggled",
        G_CALLBACK (cb_wrap_workspaces_changed), itf);
    g_signal_connect (G_OBJECT (itf->wrap_windows_check), "toggled",
        G_CALLBACK (cb_wrap_windows_changed), itf);
    g_signal_connect (G_OBJECT (itf->wrap_resistance_scale), "value_changed",
        (GCallback) cb_wrap_resistance_changed, itf);
    g_signal_connect (G_OBJECT (itf->box_move_check), "toggled", (GCallback) cb_box_move_changed,
        itf);
    g_signal_connect (G_OBJECT (itf->box_resize_check), "toggled",
        G_CALLBACK (cb_box_resize_changed), itf);

    xfce_gtk_window_center_on_monitor_with_pointer (GTK_WINDOW (itf->xfwm4_dialog));
    gdk_x11_window_set_user_time (GTK_WIDGET (itf->xfwm4_dialog)->window,
        gdk_x11_get_server_time (GTK_WIDGET (itf->xfwm4_dialog)->window));
    gtk_widget_show (itf->xfwm4_dialog);
}

McsPluginInitResult
mcs_plugin_init (McsPlugin * mcs_plugin)
{
    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    xfwm4_create_channel (mcs_plugin);
    mcs_plugin->plugin_name = g_strdup (PLUGIN_NAME);
    /* the button label in the xfce-mcs-manager dialog */
    mcs_plugin->caption = g_strdup (Q_ ("Button Label|Window Manager"));
    mcs_plugin->run_dialog = run_dialog;
    mcs_plugin->icon = xfce_themed_icon_load ("xfwm4", 48);
    if (G_LIKELY (mcs_plugin->icon != NULL))
      g_object_set_data_full (G_OBJECT (mcs_plugin->icon), "mcs-plugin-icon-name", g_strdup ("xfwm4"), g_free);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL1);

    return (MCS_PLUGIN_INIT_OK);
}

static void
xfwm4_create_channel (McsPlugin * mcs_plugin)
{
    McsSetting *setting;
    gchar *rcfile, *path;

    path = g_build_filename ("xfce4", RCDIR, RCFILE1, NULL);
    rcfile = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, path);
    g_free (path);

    if (!rcfile)
        rcfile = xfce_get_userfile (OLDRCDIR, RCFILE1, NULL);

    if (g_file_test (rcfile, G_FILE_TEST_EXISTS))
    {
        mcs_manager_add_channel_from_file (mcs_plugin->manager, CHANNEL1, rcfile);
    }
    else
    {
        mcs_manager_add_channel (mcs_plugin->manager, CHANNEL1);
    }
    g_free (rcfile);

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/ThemeName", CHANNEL1);
    if (setting)
    {
        if (current_theme)
        {
            g_free (current_theme);
        }
        current_theme = g_strdup (setting->data.v_string);
    }
    else
    {
        if (current_theme)
        {
            g_free (current_theme);
        }

        current_theme = g_strdup (DEFAULT_THEME);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/ThemeName", CHANNEL1, current_theme);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/TitleFont", CHANNEL1);
    if (setting)
    {
        if (current_font)
        {
            g_free (current_font);
        }
        current_font = g_strdup (setting->data.v_string);
    }
    else
    {
        if (current_font)
        {
            g_free (current_font);
        }

        current_font = g_strdup (DEFAULT_FONT);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/TitleFont", CHANNEL1, current_font);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/TitleAlign", CHANNEL1);
    if (setting)
    {
        if (title_align)
        {
            g_free (title_align);
        }
        title_align = g_strdup (setting->data.v_string);
    }
    else
    {
        if (title_align)
        {
            g_free (title_align);
        }

        title_align = g_strdup (DEFAULT_ALIGN);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/TitleAlign", CHANNEL1, title_align);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL1);
    if (setting)
    {
        if (current_layout)
        {
            g_free (current_layout);
        }
        current_layout = g_strdup (setting->data.v_string);
    }
    else
    {
        if (current_layout)
        {
            g_free (current_layout);
        }

        current_layout = g_strdup (DEFAULT_LAYOUT);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL1, current_layout);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/ClickToFocus", CHANNEL1);
    if (setting)
    {
        click_to_focus = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        click_to_focus = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/ClickToFocus", CHANNEL1,
            click_to_focus ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/FocusNewWindow", CHANNEL1);
    if (setting)
    {
        focus_new = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        focus_new = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusNewWindow", CHANNEL1,
            focus_new ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/FocusRaise", CHANNEL1);
    if (setting)
    {
        focus_raise = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        focus_raise = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusRaise", CHANNEL1, focus_raise ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/RaiseDelay", CHANNEL1);
    if (setting)
    {
        raise_delay = setting->data.v_int;
    }
    else
    {
        raise_delay = 250;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/RaiseDelay", CHANNEL1, raise_delay);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/FocusDelay", CHANNEL1);
    if (setting)
    {
        focus_delay = setting->data.v_int;
    }
    else
    {
        focus_delay = 0;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusDelay", CHANNEL1, raise_delay);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/RaiseOnClick", CHANNEL1);
    if (setting)
    {
        raise_on_click = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        raise_on_click = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/RaiseOnClick", CHANNEL1,
            raise_on_click ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/SnapToBorder", CHANNEL1);
    if (setting)
    {
        snap_to_border = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        snap_to_border = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapToBorder", CHANNEL1,
            snap_to_border ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/SnapToWindows", CHANNEL1);
    if (setting)
    {
        snap_to_windows = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        snap_to_windows = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapToWindows", CHANNEL1,
            snap_to_windows ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/SnapWidth", CHANNEL1);
    if (setting)
    {
        snap_width = setting->data.v_int;
    }
    else
    {
        snap_width = 10;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapWidth", CHANNEL1, snap_width);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/WrapResistance", CHANNEL1);
    if (setting)
    {
        wrap_resistance = setting->data.v_int;
    }
    else
    {
        wrap_resistance = 10;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapResistance", CHANNEL1, wrap_resistance);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/WrapWorkspaces", CHANNEL1);
    if (setting)
    {
        wrap_workspaces = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        wrap_workspaces = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapWorkspaces", CHANNEL1,
            wrap_workspaces ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/WrapWindows", CHANNEL1);
    if (setting)
    {
        wrap_windows = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        wrap_windows = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapWindows", CHANNEL1,
            wrap_windows ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/BoxMove", CHANNEL1);
    if (setting)
    {
        box_move = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        box_move = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/BoxMove", CHANNEL1, box_move ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/BoxResize", CHANNEL1);
    if (setting)
    {
        box_resize = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        box_resize = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/BoxResize", CHANNEL1, box_resize ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL1);
    if (setting)
    {
        if (dbl_click_action)
        {
            g_free (dbl_click_action);
        }
        dbl_click_action = g_strdup (setting->data.v_string);
    }
    else
    {
        if (dbl_click_action)
        {
            g_free (dbl_click_action);
        }

        dbl_click_action = g_strdup (DEFAULT_ACTION);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL1,
            dbl_click_action);
    }

    /*
     * Second separate channel for keytheme (used as a raw channel from the client)
     */

    path = g_build_filename ("xfce4", RCDIR, RCFILE2, NULL);
    rcfile = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, path);
    g_free (path);

    if (!rcfile)
        rcfile = xfce_get_userfile (OLDRCDIR, RCFILE2, NULL);

    if (g_file_test (rcfile, G_FILE_TEST_EXISTS))
    {
        mcs_manager_add_channel_from_file (mcs_plugin->manager, CHANNEL2, rcfile);
    }
    else
    {
        mcs_manager_add_channel (mcs_plugin->manager, CHANNEL2);
    }
    g_free (rcfile);

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL2);
    if (setting)
    {
        if (xfwm4_plugin_current_key_theme)
        {
            g_free (xfwm4_plugin_current_key_theme);
        }
        xfwm4_plugin_current_key_theme = g_strdup (setting->data.v_string);
    }
    else
    {
        if (xfwm4_plugin_current_key_theme)
        {
            g_free (xfwm4_plugin_current_key_theme);
        }

        xfwm4_plugin_current_key_theme = g_strdup (DEFAULT_KEY_THEME);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL2,
            xfwm4_plugin_current_key_theme);
    }

#if 0
    /* I fail to see why we need to save the options here, during startup... */
    xfwm4_plugin_write_options (mcs_plugin);
#endif
}

gboolean
xfwm4_plugin_write_options (McsPlugin * mcs_plugin)
{
    gchar *rcfile, *path;
    gboolean result = FALSE;

    path = g_build_filename ("xfce4", "mcs_settings", RCFILE1, NULL);
    rcfile = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, path, TRUE);
    if (G_LIKELY (rcfile != NULL))
    {
        result = mcs_manager_save_channel_to_file (mcs_plugin->manager, CHANNEL1, rcfile);
        g_free (rcfile);
    }
    g_free (path);

    path = g_build_filename ("xfce4", "mcs_settings", RCFILE2, NULL);
    rcfile = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, path, TRUE);
    if (G_LIKELY (rcfile != NULL))
    {
        result = mcs_manager_save_channel_to_file (mcs_plugin->manager, CHANNEL2, rcfile);
        g_free (rcfile);
    }
    g_free (path);

    return result;
}

static void
run_dialog (McsPlugin * mcs_plugin)
{
    const gchar *wm_name;
    static Itf *dialog = NULL;

    wm_name = gdk_x11_screen_get_window_manager_name (gdk_screen_get_default ());
    if (g_ascii_strcasecmp (wm_name, "Xfwm4"))
    {
        xfce_err (_("These settings cannot work with your current window manager (%s)"), wm_name);
        return;
    }

    if (is_running)
    {
        if ((dialog) && (dialog->xfwm4_dialog))
        {
            gtk_window_present (GTK_WINDOW (dialog->xfwm4_dialog));
            gtk_window_set_focus (GTK_WINDOW (dialog->xfwm4_dialog), NULL);
        }
        return;
    }

    is_running = TRUE;

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    dialog = create_dialog (mcs_plugin);
    setup_dialog (dialog);
}

/* macro defined in manager-plugin.h */
MCS_PLUGIN_CHECK_INIT
