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

        xfce4 mcs plugin   - (c) 2005-2006 Olivier Fourdan

 */

#ifndef INC_XFWM4_PLUGIN_H
#define INC_XFWM4_PLUGIN_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define RCDIR    "mcs_settings"
#define OLDRCDIR "settings"
#define RCFILE1   "xfwm4.xml"
#define RCFILE2   "xfwm4_keys.xml"
#define CHANNEL1  "xfwm4"
#define CHANNEL2  "xfwm4_keys"
#define PLUGIN_NAME "xfwm4"

#define DEFAULT_THEME "Default"
#define DEFAULT_KEY_THEME "Default"
#define DEFAULT_LAYOUT "OT|SHMC"
#define DEFAULT_ACTION "maximize"
#define DEFAULT_ALIGN "center"
#define DEFAULT_FONT "Sans Bold 9"

#define MAX_ELEMENTS_BEFORE_SCROLLING 6

#define SUFFIX      "xfwm4"
#define KEY_SUFFIX  "xfwm4"
#define KEYTHEMERC  "keythemerc"
#define THEMERC     "themerc"

#define STATES 8
#define STATE_HIDDEN (STATES - 1)

#define BORDER 5


typedef struct _ThemeInfo ThemeInfo;
struct _ThemeInfo
{
    gchar *path;
    gchar *name;
    gboolean has_decoration;
    gboolean has_keybinding;
    gboolean set_layout;
    gboolean set_align;
    gboolean set_font;
    gboolean user_writable;
};


typedef struct _Itf Itf;
struct _Itf
{
    McsPlugin *mcs_plugin;

    GSList *click_focus_radio_group;

    GtkWidget *box_move_check;
    GtkWidget *box_resize_check;
    GtkWidget *click_focus_radio;
    GtkWidget *click_raise_check;
    GtkWidget *closebutton1;
    GtkWidget *helpbutton1;
    GtkWidget *dialog_action_area1;
    GtkWidget *dialog_vbox;
    GtkWidget *focus_follow_mouse_radio;
    GtkWidget *focus_new_check;
    GtkWidget *font_button;
    GtkWidget *font_selection;
    GtkWidget *frame_layout;
    GtkWidget *frame_align;
    GtkWidget *raise_delay_scale;
    GtkWidget *focus_delay_scale;
    GtkWidget *raise_on_focus_check;
    GtkWidget *scrolledwindow1;
    GtkWidget *scrolledwindow2;
    GtkWidget *scrolledwindow3;
    GtkWidget *scrolledwindow4;
    GtkWidget *snap_to_border_check;
    GtkWidget *snap_to_windows_check;
    GtkWidget *snap_width_scale;
    GtkWidget *treeview1;
    GtkWidget *treeview2;
    GtkWidget *treeview3;
    GtkWidget *wrap_workspaces_check;
    GtkWidget *wrap_windows_check;
    GtkWidget *wrap_resistance_scale;
    GtkWidget *xfwm4_dialog;
    GtkWidget *popup_menu;
    GtkWidget* popup_add_menuitem;
    GtkWidget* popup_del_menuitem;
    GtkWidget* add_button;
    GtkWidget* del_button;

};

enum
{
    THEME_NAME_COLUMN,
    THEME_NAME_COLUMNS
};

enum
{
    COLUMN_COMMAND,
    COLUMN_SHORTCUT,
    COLUMN_NAME,
    KEY_SHORTCUT_COLUMNS
};

typedef enum
{
    DECORATION_THEMES = 0,
    KEYBINDING_THEMES = 1
}
ThemeType;

extern gchar *xfwm4_plugin_current_key_theme;
extern GList *keybinding_theme_list;

gboolean xfwm4_plugin_write_options (McsPlugin *);
void xfwm4_plugin_theme_info_free (ThemeInfo *);
ThemeInfo *xfwm4_plugin_find_theme_info_by_name (const gchar *, GList *);
GList *xfwm4_plugin_read_themes (GList *, GtkWidget *, GtkWidget *, ThemeType, gchar *);

#endif /* INC_XFWM4_PLUGIN_H */
