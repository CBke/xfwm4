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
        xfwm4    - (c) 2002-2011 Olivier Fourdan,
                       2008 Jannis Pohlmann

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <pango/pango.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>
#include <libxfce4kbd-private/xfce-shortcuts-provider.h>

#include "screen.h"
#include "hints.h"
#include "parserc.h"
#include "client.h"
#include "focus.h"
#include "workspaces.h"
#include "compositor.h"
#include "ui_style.h"

#define CHANNEL_XFWM            "xfwm4"
#define THEMERC                 "themerc"
#define XPM_COLOR_SYMBOL_SIZE   22

/* Forward static decls. */

static void              update_grabs      (ScreenInfo *);
static void              set_settings_margin  (ScreenInfo *,
                                               int ,
                                               int);
static void              loadRcData           (ScreenInfo *,
                                               Settings *);
static void              loadTheme            (ScreenInfo *,
                                               Settings *);
static void              loadKeyBindings      (ScreenInfo *);
static void              unloadTheme          (ScreenInfo *);
static void              unloadKeyBindings    (ScreenInfo *);
static void              unloadSettings       (ScreenInfo *);
static gboolean          reloadScreenSettings (ScreenInfo *,
                                               int);
static void              parseShortcut        (ScreenInfo *,
                                               int,
                                               const gchar *,
                                               GList *);
static const gchar      *getShortcut          (const gchar *,
                                               GList *);
static void              cb_xfwm4_channel_property_changed (XfconfChannel *,
                                                           const gchar *,
                                                           const GValue *,
                                                           ScreenInfo *);
static void              cb_keys_changed      (GdkKeymap *,
                                               ScreenInfo *);
static void              cb_shortcut_added    (XfceShortcutsProvider *,
                                               const gchar *,
                                               ScreenInfo *);
static void              cb_shortcut_removed  (XfceShortcutsProvider *,
                                               const gchar *,
                                               ScreenInfo *);

static void
update_grabs (ScreenInfo *screen_info)
{
    clientUngrabMouseButtonForAll (screen_info);
    if ((screen_info->params->raise_on_click) || (screen_info->params->click_to_focus))
    {
        clientGrabMouseButtonForAll (screen_info);
    }
}

static void
set_settings_margin (ScreenInfo *screen_info, int idx, int value)
{
    int val;

    switch (idx)
    {
        case STRUTS_LEFT:
        case STRUTS_RIGHT:
            if (value < 0)
            {
                val = 0;
            }
            else if (value > screen_info->width / 4)
            {
                val = screen_info->width / 4;
            }
            else
            {
                val = value;
            }
            screen_info->params->xfwm_margins[idx] = val;
            break;
        case STRUTS_TOP:
        case STRUTS_BOTTOM:
            if (value < 0)
            {
                val = 0;
            }
            else if (value > screen_info->height / 4)
            {
                val = screen_info->height / 4;
            }
            else
            {
                val = value;
            }
            screen_info->params->xfwm_margins[idx] = val;
            break;
        default:
            break;
    }
}

static void
set_easy_click (ScreenInfo *screen_info, const char *modifier)
{
    gchar *modstr;

    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (modifier != NULL);

    if (!g_ascii_strcasecmp (modifier, "true"))
    {
        screen_info->params->easy_click = AltMask;
    }
    else
    {
        modstr = g_strdup_printf ("<%s>", modifier);
        screen_info->params->easy_click = getModifierMap (modstr);
        g_free (modstr);
    }
}

static void
set_activate_action (ScreenInfo *screen_info, const char *value)
{
    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (value != NULL);

    if (!g_ascii_strcasecmp ("bring", value))
    {
        screen_info->params->activate_action = ACTIVATE_ACTION_BRING;
    }
    else if (!g_ascii_strcasecmp ("switch", value))
    {
        screen_info->params->activate_action = ACTIVATE_ACTION_SWITCH;
    }
    else
    {
        screen_info->params->activate_action = ACTIVATE_ACTION_NONE;
    }
}

static void
set_placement_mode (ScreenInfo *screen_info, const char *value)
{
    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (value != NULL);

    if (!g_ascii_strcasecmp ("mouse", value))
    {
        screen_info->params->placement_mode = PLACE_MOUSE;
    }
    else
    {
        screen_info->params->placement_mode = PLACE_CENTER;
    }
}

static void
loadRcData (ScreenInfo *screen_info, Settings *rc)
{
    gchar *homedir;

    if (!parseRc ("defaults", PACKAGE_DATADIR, rc))
    {
        g_warning ("Missing defaults file");
        exit (1);
    }
    homedir = xfce_resource_save_location (XFCE_RESOURCE_CONFIG,
                                           "xfce4" G_DIR_SEPARATOR_S "xfwm4",
                                           FALSE);
    parseRc ("xfwm4rc", homedir, rc);
    g_free (homedir);
}

static void
loadXfconfData (ScreenInfo *screen_info, Settings *rc)
{
    gint i;
    for (i = XPM_COLOR_SYMBOL_SIZE; rc[i].option; ++i)
    {
        gchar *property_name = g_strconcat("/general/", rc[i].option, NULL);
        if(xfconf_channel_has_property(screen_info->xfwm4_channel, property_name))
        {
            if(rc[i].value)
            {
                g_value_unset(rc[i].value);
                g_free(rc[i].value);
            }
            rc[i].value = g_new0(GValue, 1);

            if(!xfconf_channel_get_property(screen_info->xfwm4_channel, property_name, rc[i].value))
            {
               g_warning("get prop failed");
            }
        }
        else
        {
            if (rc[i].value)
                xfconf_channel_set_property(screen_info->xfwm4_channel, property_name, rc[i].value);
        }
        g_free(property_name);
    }

}

/* Simple helper function to avoid copy/paste of code */
static void
setXfwmColor (ScreenInfo *screen_info, XfwmColor *color, Settings *rc, int id, const gchar * name, const gchar * state)
{
    if (color->allocated)
    {
        gdk_colormap_free_colors (gdk_screen_get_rgb_colormap (screen_info->gscr), &color->col, 1);
        color->allocated = FALSE;
    }

    /** do a direct value_get_string */
    if (gdk_color_parse (g_value_get_string(rc[id].value), &color->col))
    {
        if (gdk_colormap_alloc_color (gdk_screen_get_rgb_colormap (screen_info->gscr),
                                      &color->col, FALSE, FALSE))
        {
            color->allocated = TRUE;
            if (color->gc)
            {
                g_object_unref (G_OBJECT (color->gc));
            }
            color->gc = gdk_gc_new (myScreenGetGdkWindow (screen_info));
            gdk_gc_copy (color->gc, getUIStyle_gc (myScreenGetGtkWidget (screen_info), name, state));
            gdk_gc_set_foreground (color->gc, &color->col);
        }
        else
        {
            gdk_beep ();
            if (G_VALUE_TYPE(rc[id].value) == G_TYPE_STRING)
                g_message (_("%s: Cannot allocate color %s\n"), g_get_prgname (), g_value_get_string(rc[id].value));
            else
                g_critical (_("%s: Cannot allocate color: GValue for color is not of type STRING"), g_get_prgname ());
        }
    }
    else
    {
        gdk_beep ();
        if (G_VALUE_TYPE(rc[id].value) == G_TYPE_STRING)
            g_message (_("%s: Cannot parse color %s\n"), g_get_prgname (), g_value_get_string(rc[id].value));
        else
            g_critical (_("%s: Cannot parse color: GValue for color is not of type STRING"), g_get_prgname ());
    }
}

static int
getTitleShadow (Settings *rc, const gchar * name)
{
    const gchar *val;

    val = getStringValue (name, rc);
    if (!g_ascii_strcasecmp ("true", val) || !g_ascii_strcasecmp ("under", val))
    {
        return TITLE_SHADOW_UNDER;
    }
    else if (!g_ascii_strcasecmp ("frame", val))
    {
        return TITLE_SHADOW_FRAME;
    }
    return TITLE_SHADOW_NONE;
}

static void
loadTheme (ScreenInfo *screen_info, Settings *rc)
{

    static const char *side_names[] = {
        "left",
        "right",
        "bottom"
    };

    static const char *corner_names[] = {
        "bottom-left",
        "bottom-right",
        "top-left",
        "top-right"
    };

    static const char *button_names[] = {
        "menu",
        "stick",
        "shade",
        "hide",
        "maximize",
        "close"
    };

    static const char *button_state_names[] = {
        "active",
        "inactive",
        "prelight",
        "pressed",
        "toggled-active",
        "toggled-inactive",
        "toggled-prelight",
        "toggled-pressed"
    };

    static const char *ui_part[] = {
        "fg",
        "fg",
        "dark",
        "dark",
        "fg",
        "fg",
        "bg",
        "light",
        "dark",
        "mid",
        "bg",
        "light",
        "dark",
        "mid",
        "bg",
        "light",
        "dark",
        "mid",
        "bg",
        "light",
        "dark",
        "mid",
        NULL
    };

    static const char *ui_state[] = {
        "selected",
        "insensitive",
        "selected",
        "insensitive",
        "normal",
        "normal",
        "selected",
        "selected",
        "selected",
        "selected",
        "normal",
        "normal",
        "normal",
        "normal",
        "insensitive",
        "insensitive",
        "insensitive",
        "insensitive",
        "normal",
        "normal",
        "normal",
        "normal",
        NULL
    };

    gchar imagename[30];
    GValue tmp_val = { 0, };
    GValue tmp_val2 = { 0, };
    DisplayInfo *display_info;
    xfwmColorSymbol colsym[ XPM_COLOR_SYMBOL_SIZE + 1 ];
    GtkWidget *widget;
    gchar *theme;
    const gchar *font;
    PangoFontDescription *desc;
    guint i, j;

    widget = myScreenGetGtkWidget (screen_info);
    display_info = screen_info->display_info;

    desc = NULL;
    i = 0;

    while (ui_part[i] && ui_state[i])
    {
        gchar *color;

        color = getUIStyle  (widget, ui_part[i], ui_state[i]);
        setStringValue (rc[i].option, color, rc);
        g_free (color);
        ++i;
    }

    theme = getThemeDir (getStringValue ("theme", rc), THEMERC);
    parseRc (THEMERC, theme, rc);

    screen_info->params->shadow_delta_x =
        - getIntValue ("shadow_delta_x", rc);
    screen_info->params->shadow_delta_y =
        - getIntValue ("shadow_delta_y", rc);
    screen_info->params->shadow_delta_width =
        - getIntValue ("shadow_delta_width", rc);
    screen_info->params->shadow_delta_height =
        - getIntValue ("shadow_delta_height", rc);

    for (i = 0; i < XPM_COLOR_SYMBOL_SIZE; i++)
    {
        colsym[i].name = rc[i].option;
        colsym[i].value = g_value_get_string(rc[i].value);
    }
    colsym[XPM_COLOR_SYMBOL_SIZE].name = NULL;
    colsym[XPM_COLOR_SYMBOL_SIZE].value = NULL;

    /* Standard double click time ... */
    display_info->double_click_time = abs (getIntValue ("double_click_time", rc));
    g_value_init (&tmp_val, G_TYPE_INT);
    if (gdk_setting_get ("gtk-double-click-time", &tmp_val))
    {
        display_info->double_click_time = abs (g_value_get_int (&tmp_val));
        g_value_unset (&tmp_val);
    }

    /* ... and distance */
    display_info->double_click_distance = abs (getIntValue ("double_click_distance", rc));
    g_value_init (&tmp_val2, G_TYPE_INT);
    if (gdk_setting_get ("gtk-double-click-distance", &tmp_val2))
    {
        display_info->double_click_distance = abs (g_value_get_int (&tmp_val2));
        g_value_unset (&tmp_val2);
    }

    screen_info->font_height = 0;
    font = getStringValue ("title_font", rc);
    if (font && strlen (font))
    {
        desc = pango_font_description_from_string (font);
        if (desc)
        {
            gtk_widget_modify_font (widget, desc);
            pango_font_description_free (desc);
            myScreenUpdateFontHeight (screen_info);
        }
    }

    setXfwmColor (screen_info, &screen_info->title_colors[ACTIVE], rc, 0, "fg", "selected");
    setXfwmColor (screen_info, &screen_info->title_colors[INACTIVE], rc, 1, "fg", "insensitive");
    setXfwmColor (screen_info, &screen_info->title_shadow_colors[ACTIVE], rc, 2, "dark", "selected");
    setXfwmColor (screen_info, &screen_info->title_shadow_colors[INACTIVE], rc, 3, "dark", "insensitive");

    if (screen_info->black_gc)
    {
        g_object_unref (G_OBJECT (screen_info->black_gc));
    }
    screen_info->black_gc = widget->style->black_gc;
    g_object_ref (G_OBJECT (widget->style->black_gc));

    if (screen_info->white_gc)
    {
        g_object_unref (G_OBJECT (screen_info->white_gc));
    }
    screen_info->white_gc = widget->style->white_gc;
    g_object_ref (G_OBJECT (widget->style->white_gc));

    for (i = 0; i < SIDE_TOP; i++) /* Keep SIDE_TOP for later */
    {
        g_snprintf(imagename, sizeof (imagename), "%s-active", side_names[i]);
        xfwmPixmapLoad (screen_info, &screen_info->sides[i][ACTIVE], theme, imagename, colsym);

        g_snprintf(imagename, sizeof (imagename), "%s-inactive", side_names[i]);
        xfwmPixmapLoad (screen_info, &screen_info->sides[i][INACTIVE], theme, imagename, colsym);
    }
    for (i = 0; i < CORNER_COUNT; i++)
    {
        g_snprintf(imagename, sizeof (imagename), "%s-active", corner_names[i]);
        xfwmPixmapLoad (screen_info, &screen_info->corners[i][ACTIVE], theme, imagename, colsym);

        g_snprintf(imagename, sizeof (imagename), "%s-inactive", corner_names[i]);
        xfwmPixmapLoad (screen_info, &screen_info->corners[i][INACTIVE], theme, imagename, colsym);
    }
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        for (j = 0; j < STATE_COUNT; j++)
        {
            g_snprintf(imagename, sizeof (imagename), "%s-%s", button_names[i], button_state_names[j]);
            xfwmPixmapLoad (screen_info, &screen_info->buttons[i][j], theme, imagename, colsym);
        }
    }
    for (i = 0; i < TITLE_COUNT; i++)
    {
        g_snprintf(imagename, sizeof (imagename), "title-%d-active", i + 1);
        xfwmPixmapLoad (screen_info, &screen_info->title[i][ACTIVE], theme, imagename, colsym);

        g_snprintf(imagename, sizeof (imagename), "title-%d-inactive", i + 1);
        xfwmPixmapLoad (screen_info, &screen_info->title[i][INACTIVE], theme, imagename, colsym);

        g_snprintf(imagename, sizeof (imagename), "top-%d-active", i + 1);
        xfwmPixmapLoad (screen_info, &screen_info->top[i][ACTIVE], theme, imagename, colsym);

        g_snprintf(imagename, sizeof (imagename), "top-%d-inactive", i + 1);
        xfwmPixmapLoad (screen_info, &screen_info->top[i][INACTIVE], theme, imagename, colsym);
    }

    screen_info->box_gc = createGC (screen_info, "#FFFFFF", GXxor, NULL, 2, TRUE);

    if (!g_ascii_strcasecmp ("left", getStringValue ("title_alignment", rc)))
    {
        screen_info->params->title_alignment = ALIGN_LEFT;
    }
    else if (!g_ascii_strcasecmp ("right", getStringValue ("title_alignment", rc)))
    {
        screen_info->params->title_alignment = ALIGN_RIGHT;
    }
    else
    {
        screen_info->params->title_alignment = ALIGN_CENTER;
    }

    screen_info->params->full_width_title =
        getBoolValue ("full_width_title", rc);
    screen_info->params->title_shadow[ACTIVE] = getTitleShadow (rc, "title_shadow_active");
    screen_info->params->title_shadow[INACTIVE] = getTitleShadow (rc, "title_shadow_inactive");

    strncpy (screen_info->params->button_layout, getStringValue ("button_layout", rc), BUTTON_STRING_COUNT);
    screen_info->params->button_spacing = getIntValue ("button_spacing", rc);
    screen_info->params->button_offset = getIntValue ("button_offset", rc);
    screen_info->params->maximized_offset = getIntValue ("maximized_offset", rc);
    screen_info->params->title_vertical_offset_active =
        getIntValue ("title_vertical_offset_active", rc);
    screen_info->params->title_vertical_offset_inactive =
        getIntValue ("title_vertical_offset_inactive", rc);
    screen_info->params->title_horizontal_offset =
        getIntValue ("title_horizontal_offset", rc);

    g_free (theme);
}

static void
loadKeyBindings (ScreenInfo *screen_info)
{
    GList *shortcuts;
    gchar keyname[30];
    guint i;

    shortcuts = xfce_shortcuts_provider_get_shortcuts (screen_info->shortcuts_provider);

    parseShortcut (screen_info, KEY_CANCEL, "cancel_key", shortcuts);
    parseShortcut (screen_info, KEY_DOWN, "down_key", shortcuts);
    parseShortcut (screen_info, KEY_LEFT, "left_key", shortcuts);
    parseShortcut (screen_info, KEY_RIGHT, "right_key", shortcuts);
    parseShortcut (screen_info, KEY_UP, "up_key", shortcuts);
    parseShortcut (screen_info, KEY_ADD_WORKSPACE, "add_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_ADD_ADJACENT_WORKSPACE, "add_adjacent_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_CLOSE_WINDOW, "close_window_key", shortcuts);
    parseShortcut (screen_info, KEY_CYCLE_WINDOWS, "cycle_windows_key", shortcuts);
    parseShortcut (screen_info, KEY_CYCLE_REVERSE_WINDOWS, "cycle_reverse_windows_key", shortcuts);
    parseShortcut (screen_info, KEY_DEL_WORKSPACE, "del_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_DEL_ACTIVE_WORKSPACE, "del_active_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_DOWN_WORKSPACE, "down_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_FILL_HORIZ, "fill_horiz_key", shortcuts);
    parseShortcut (screen_info, KEY_FILL_VERT, "fill_vert_key", shortcuts);
    parseShortcut (screen_info, KEY_FILL_WINDOW, "fill_window_key", shortcuts);
    parseShortcut (screen_info, KEY_HIDE_WINDOW, "hide_window_key", shortcuts);
    parseShortcut (screen_info, KEY_LEFT_WORKSPACE, "left_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_LOWER_WINDOW, "lower_window_key", shortcuts);
    parseShortcut (screen_info, KEY_MOVE, "move_window_key", shortcuts);
    parseShortcut (screen_info, KEY_MAXIMIZE_HORIZ, "maximize_horiz_key", shortcuts);
    parseShortcut (screen_info, KEY_MAXIMIZE_VERT, "maximize_vert_key", shortcuts);
    parseShortcut (screen_info, KEY_MAXIMIZE_WINDOW, "maximize_window_key", shortcuts);
    parseShortcut (screen_info, KEY_MOVE_DOWN_WORKSPACE, "move_window_down_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_MOVE_LEFT_WORKSPACE, "move_window_left_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_MOVE_NEXT_WORKSPACE, "move_window_next_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_MOVE_PREV_WORKSPACE, "move_window_prev_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_MOVE_RIGHT_WORKSPACE, "move_window_right_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_MOVE_UP_WORKSPACE, "move_window_up_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_NEXT_WORKSPACE, "next_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_POPUP_MENU, "popup_menu_key", shortcuts);
    parseShortcut (screen_info, KEY_PREV_WORKSPACE, "prev_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_RAISE_WINDOW, "raise_window_key", shortcuts);
    parseShortcut (screen_info, KEY_RAISELOWER_WINDOW, "raiselower_window_key", shortcuts);
    parseShortcut (screen_info, KEY_RESIZE, "resize_window_key", shortcuts);
    parseShortcut (screen_info, KEY_RIGHT_WORKSPACE, "right_workspace_key", shortcuts);
    parseShortcut (screen_info, KEY_SHADE_WINDOW, "shade_window_key", shortcuts);
    parseShortcut (screen_info, KEY_SHOW_DESKTOP, "show_desktop_key", shortcuts);
    parseShortcut (screen_info, KEY_STICK_WINDOW, "stick_window_key", shortcuts);
    parseShortcut (screen_info, KEY_SWITCH_APPLICATION, "switch_application_key", shortcuts);
    parseShortcut (screen_info, KEY_SWITCH_WINDOW, "switch_window_key", shortcuts);
    parseShortcut (screen_info, KEY_TILE_DOWN, "tile_down_key", shortcuts);
    parseShortcut (screen_info, KEY_TILE_LEFT, "tile_left_key", shortcuts);
    parseShortcut (screen_info, KEY_TILE_RIGHT, "tile_right_key", shortcuts);
    parseShortcut (screen_info, KEY_TILE_UP, "tile_up_key", shortcuts);
    parseShortcut (screen_info, KEY_TOGGLE_ABOVE, "above_key", shortcuts);
    parseShortcut (screen_info, KEY_TOGGLE_FULLSCREEN, "fullscreen_key", shortcuts);
    parseShortcut (screen_info, KEY_UP_WORKSPACE, "up_workspace_key", shortcuts);

    for (i = 0; i < 12; i++)
    {
        g_snprintf(keyname, sizeof (keyname), "move_window_workspace_%d_key", i + 1);
        parseShortcut (screen_info, KEY_MOVE_WORKSPACE_1 + i, keyname, shortcuts);

        g_snprintf(keyname, sizeof (keyname), "workspace_%d_key", i + 1);
        parseShortcut (screen_info, KEY_WORKSPACE_1 + i, keyname, shortcuts);
    }

    xfce_shortcuts_free (shortcuts);

    myScreenUngrabKeys (screen_info);
    myScreenGrabKeys (screen_info);

    return;
}

gboolean
loadSettings (ScreenInfo *screen_info)
{
    const gchar *value;
    Settings rc[] = {
        /* Do not change the order of the following parameters */
        {"active_text_color", NULL, G_TYPE_STRING, FALSE},
        {"inactive_text_color", NULL, G_TYPE_STRING, FALSE},
        {"active_text_shadow_color", NULL, G_TYPE_STRING, FALSE},
        {"inactive_text_shadow_color", NULL, G_TYPE_STRING, FALSE},
        {"active_border_color", NULL, G_TYPE_STRING, FALSE},
        {"inactive_border_color", NULL, G_TYPE_STRING, FALSE},
        {"active_color_1", NULL, G_TYPE_STRING, FALSE},
        {"active_hilight_1", NULL, G_TYPE_STRING, FALSE},
        {"active_shadow_1", NULL, G_TYPE_STRING, FALSE},
        {"active_mid_1", NULL, G_TYPE_STRING, FALSE},
        {"active_color_2", NULL, G_TYPE_STRING, FALSE},
        {"active_hilight_2", NULL, G_TYPE_STRING, FALSE},
        {"active_shadow_2", NULL, G_TYPE_STRING, FALSE},
        {"active_mid_2", NULL, G_TYPE_STRING, FALSE},
        {"inactive_color_1", NULL, G_TYPE_STRING, FALSE},
        {"inactive_hilight_1", NULL, G_TYPE_STRING, FALSE},
        {"inactive_shadow_1", NULL, G_TYPE_STRING, FALSE},
        {"inactive_mid_1", NULL, G_TYPE_STRING, FALSE},
        {"inactive_color_2", NULL, G_TYPE_STRING, FALSE},
        {"inactive_hilight_2", NULL, G_TYPE_STRING, FALSE},
        {"inactive_shadow_2", NULL, G_TYPE_STRING, FALSE},
        {"inactive_mid_2", NULL, G_TYPE_STRING, FALSE},
        /* You can change the order of the following parameters */
        {"activate_action", NULL, G_TYPE_STRING, TRUE},
        {"borderless_maximize", NULL, G_TYPE_BOOLEAN, TRUE},
        {"box_move", NULL, G_TYPE_BOOLEAN, TRUE},
        {"box_resize", NULL, G_TYPE_BOOLEAN, TRUE},
        {"button_layout", NULL, G_TYPE_STRING, TRUE},
        {"button_offset", NULL, G_TYPE_INT, TRUE},
        {"button_spacing", NULL, G_TYPE_INT, TRUE},
        {"click_to_focus", NULL, G_TYPE_BOOLEAN, TRUE},
        {"focus_delay", NULL, G_TYPE_INT, TRUE},
        {"cycle_apps_only", NULL, G_TYPE_BOOLEAN, TRUE},
        {"cycle_draw_frame", NULL, G_TYPE_BOOLEAN, TRUE},
        {"cycle_hidden", NULL, G_TYPE_BOOLEAN, TRUE},
        {"cycle_minimum", NULL, G_TYPE_BOOLEAN, TRUE},
        {"cycle_workspaces", NULL, G_TYPE_BOOLEAN, TRUE},
        {"double_click_time", NULL, G_TYPE_INT, TRUE},
        {"double_click_distance", NULL, G_TYPE_INT, TRUE},
        {"double_click_action", NULL, G_TYPE_STRING, TRUE},
        {"easy_click", NULL, G_TYPE_STRING, TRUE},
        {"focus_hint", NULL, G_TYPE_BOOLEAN, TRUE},
        {"focus_new", NULL, G_TYPE_BOOLEAN,TRUE},
        {"frame_opacity", NULL, G_TYPE_INT, TRUE},
        {"full_width_title", NULL, G_TYPE_BOOLEAN, TRUE},
        {"inactive_opacity", NULL, G_TYPE_INT, TRUE},
        {"margin_bottom", NULL, G_TYPE_INT, FALSE},
        {"margin_left", NULL, G_TYPE_INT, FALSE},
        {"margin_right", NULL, G_TYPE_INT, FALSE},
        {"margin_top", NULL, G_TYPE_INT, FALSE},
        {"maximized_offset", NULL, G_TYPE_INT, TRUE},
        {"move_opacity", NULL, G_TYPE_INT, TRUE},
        {"placement_ratio", NULL, G_TYPE_INT, TRUE},
        {"placement_mode", NULL, G_TYPE_STRING, TRUE},
        {"popup_opacity", NULL, G_TYPE_INT, TRUE},
        {"lower_on_middleclick", NULL, G_TYPE_BOOLEAN, TRUE},
        {"mousewheel_rollup", NULL, G_TYPE_BOOLEAN, TRUE},
        {"prevent_focus_stealing", NULL, G_TYPE_BOOLEAN, TRUE},
        {"raise_delay", NULL, G_TYPE_INT, TRUE},
        {"raise_on_click", NULL, G_TYPE_BOOLEAN, TRUE},
        {"raise_on_focus", NULL, G_TYPE_BOOLEAN, TRUE},
        {"raise_with_any_button", NULL, G_TYPE_BOOLEAN, TRUE},
        {"repeat_urgent_blink", NULL, G_TYPE_BOOLEAN, TRUE},
        {"resize_opacity", NULL, G_TYPE_INT, TRUE},
        {"restore_on_move", NULL, G_TYPE_BOOLEAN, TRUE},
        {"scroll_workspaces", NULL, G_TYPE_BOOLEAN, TRUE},
        {"shadow_delta_height", NULL, G_TYPE_INT, TRUE},
        {"shadow_delta_width", NULL, G_TYPE_INT, TRUE},
        {"shadow_delta_x", NULL, G_TYPE_INT, TRUE},
        {"shadow_delta_y", NULL, G_TYPE_INT, TRUE},
        {"shadow_opacity", NULL, G_TYPE_INT, TRUE},
        {"show_app_icon", NULL, G_TYPE_BOOLEAN, TRUE},
        {"show_dock_shadow", NULL, G_TYPE_BOOLEAN, TRUE},
        {"show_frame_shadow", NULL, G_TYPE_BOOLEAN, TRUE},
        {"show_popup_shadow", NULL, G_TYPE_BOOLEAN, TRUE},
        {"snap_resist", NULL, G_TYPE_BOOLEAN, TRUE},
        {"snap_to_border", NULL, G_TYPE_BOOLEAN, TRUE},
        {"snap_to_windows", NULL, G_TYPE_BOOLEAN, TRUE},
        {"snap_width", NULL, G_TYPE_INT, TRUE},
        {"sync_to_vblank", NULL, G_TYPE_BOOLEAN, TRUE},
        {"theme", NULL, G_TYPE_STRING, TRUE},
        {"tile_on_move", NULL, G_TYPE_BOOLEAN, TRUE},
        {"title_alignment", NULL, G_TYPE_STRING, TRUE},
        {"title_font", NULL, G_TYPE_STRING, FALSE},
        {"title_horizontal_offset", NULL, G_TYPE_INT, TRUE},
        {"title_shadow_active", NULL, G_TYPE_STRING, TRUE},
        {"title_shadow_inactive", NULL, G_TYPE_STRING, TRUE},
        {"title_vertical_offset_active", NULL, G_TYPE_INT, TRUE},
        {"title_vertical_offset_inactive", NULL, G_TYPE_INT, TRUE},
        {"toggle_workspaces", NULL, G_TYPE_BOOLEAN, TRUE},
        {"unredirect_overlays", NULL, G_TYPE_BOOLEAN, TRUE},
        {"urgent_blink", NULL, G_TYPE_BOOLEAN, TRUE},
        {"use_compositing", NULL, G_TYPE_BOOLEAN, TRUE},
        {"workspace_count", NULL, G_TYPE_INT, TRUE},
        {"wrap_cycle", NULL, G_TYPE_BOOLEAN, TRUE},
        {"wrap_layout", NULL, G_TYPE_BOOLEAN, TRUE},
        {"wrap_resistance", NULL, G_TYPE_INT, TRUE},
        {"wrap_windows", NULL, G_TYPE_BOOLEAN, TRUE},
        {"wrap_workspaces", NULL, G_TYPE_BOOLEAN, TRUE},
        {NULL, NULL, G_TYPE_INVALID, FALSE}
    };

    TRACE ("entering loadSettings");

    loadRcData (screen_info, rc);
    loadXfconfData (screen_info, rc);
    loadTheme (screen_info, rc);
    update_grabs (screen_info);

    loadKeyBindings (screen_info);

    screen_info->params->borderless_maximize =
        getBoolValue ("borderless_maximize", rc);
    screen_info->params->box_resize =
        getBoolValue ("box_resize", rc);
    screen_info->params->box_move =
        getBoolValue ("box_move", rc);
    screen_info->params->click_to_focus =
        getBoolValue ("click_to_focus", rc);
    screen_info->params->cycle_apps_only =
        getBoolValue ("cycle_apps_only", rc);
    screen_info->params->cycle_minimum =
        getBoolValue ("cycle_minimum", rc);
    screen_info->params->cycle_draw_frame =
        getBoolValue ("cycle_draw_frame", rc);
    screen_info->params->cycle_hidden =
        getBoolValue ("cycle_hidden", rc);
    screen_info->params->cycle_workspaces =
        getBoolValue ("cycle_workspaces", rc);
    screen_info->params->focus_hint =
        getBoolValue ("focus_hint", rc);
    screen_info->params->focus_new =
        getBoolValue ("focus_new", rc);
    screen_info->params->lower_on_middleclick =
        getBoolValue ("lower_on_middleclick", rc);	   
    screen_info->params->mousewheel_rollup =
        getBoolValue ("mousewheel_rollup", rc);
    screen_info->params->prevent_focus_stealing =
        getBoolValue ("prevent_focus_stealing", rc);
    screen_info->params->raise_delay =
        getIntValue ("raise_delay", rc);
    screen_info->params->raise_on_focus =
        getBoolValue ("raise_on_focus", rc);
    screen_info->params->focus_delay =
        getIntValue ("focus_delay", rc);
    screen_info->params->raise_on_click =
        getBoolValue ("raise_on_click", rc);
    screen_info->params->raise_with_any_button =
        getBoolValue ("raise_with_any_button", rc);
    screen_info->params->repeat_urgent_blink =
        getBoolValue ("repeat_urgent_blink", rc);
    screen_info->params->urgent_blink =
        getBoolValue ("urgent_blink", rc);
    screen_info->params->restore_on_move =
        getBoolValue ("restore_on_move", rc);
    screen_info->params->frame_opacity =
        CLAMP (getIntValue ("frame_opacity", rc), 0, 100);
    screen_info->params->inactive_opacity =
        CLAMP (getIntValue ("inactive_opacity", rc), 0, 100);
    screen_info->params->move_opacity =
        CLAMP (getIntValue ("move_opacity", rc), 0, 100);
    screen_info->params->resize_opacity =
        CLAMP (getIntValue ("resize_opacity", rc), 0, 100);
    screen_info->params->popup_opacity =
        CLAMP (getIntValue ("popup_opacity", rc), 0, 100);
    screen_info->params->placement_ratio =
        CLAMP (getIntValue ("placement_ratio", rc), 0, 100);
    screen_info->params->shadow_opacity =
        CLAMP (getIntValue ("shadow_opacity", rc), 0, 100);
    screen_info->params->show_app_icon =
        getBoolValue ("show_app_icon", rc);
    screen_info->params->show_dock_shadow =
        getBoolValue ("show_dock_shadow", rc);
    screen_info->params->show_frame_shadow =
        getBoolValue ("show_frame_shadow", rc);
    screen_info->params->show_popup_shadow =
        getBoolValue ("show_popup_shadow", rc);
    screen_info->params->snap_to_border =
        getBoolValue ("snap_to_border", rc);
    screen_info->params->snap_to_windows =
        getBoolValue ("snap_to_windows", rc);
    screen_info->params->snap_resist =
        getBoolValue ("snap_resist", rc);
    screen_info->params->snap_width =
        getIntValue ("snap_width", rc);
    screen_info->params->sync_to_vblank =
        getBoolValue ("sync_to_vblank", rc);
    screen_info->params->tile_on_move =
        getBoolValue ("tile_on_move", rc);
    screen_info->params->toggle_workspaces =
        getBoolValue ("toggle_workspaces", rc);
    screen_info->params->unredirect_overlays =
        getBoolValue ("unredirect_overlays", rc);
    screen_info->params->use_compositing =
        getBoolValue ("use_compositing", rc);
    screen_info->params->wrap_workspaces =
        getBoolValue ("wrap_workspaces", rc);

    screen_info->params->wrap_layout =
        getBoolValue ("wrap_layout", rc);
    screen_info->params->wrap_windows =
        getBoolValue ("wrap_windows", rc);
    screen_info->params->wrap_cycle =
        getBoolValue ("wrap_cycle", rc);
    screen_info->params->scroll_workspaces =
        getBoolValue ("scroll_workspaces", rc);
    screen_info->params->wrap_resistance =
        getIntValue ("wrap_resistance", rc);

    set_settings_margin (screen_info, STRUTS_LEFT,   getIntValue ("margin_left", rc));
    set_settings_margin (screen_info, STRUTS_RIGHT,  getIntValue ("margin_right", rc));
    set_settings_margin (screen_info, STRUTS_BOTTOM, getIntValue ("margin_bottom", rc));
    set_settings_margin (screen_info, STRUTS_TOP,    getIntValue ("margin_top", rc));

    set_easy_click (screen_info, getStringValue ("easy_click", rc));

    value = getStringValue ("placement_mode", rc);
    set_placement_mode (screen_info, value);

    value = getStringValue ("activate_action", rc);
    set_activate_action (screen_info, value);

    value = getStringValue ("double_click_action", rc);
    if (!g_ascii_strcasecmp ("shade", value))
    {
        screen_info->params->double_click_action = DOUBLE_CLICK_ACTION_SHADE;
    }
    else if (!g_ascii_strcasecmp ("hide", value))
    {
        screen_info->params->double_click_action = DOUBLE_CLICK_ACTION_HIDE;
    }
    else if (!g_ascii_strcasecmp ("maximize", value))
    {
        screen_info->params->double_click_action = DOUBLE_CLICK_ACTION_MAXIMIZE;
    }
    else if (!g_ascii_strcasecmp ("fill", value))
    {
        screen_info->params->double_click_action = DOUBLE_CLICK_ACTION_FILL;
    }
    else
    {
        screen_info->params->double_click_action = DOUBLE_CLICK_ACTION_NONE;
    }

    if (screen_info->workspace_count == 0)
    {
        workspaceSetCount (screen_info, (guint) MAX (getIntValue ("workspace_count", rc), 1));
    }

    freeRc (rc);
    return TRUE;
}

static void
unloadTheme (ScreenInfo *screen_info)
{
    int i, j;

    TRACE ("entering unloadTheme");

    for (i = 0; i < SIDE_COUNT; i++)
    {
        xfwmPixmapFree (&screen_info->sides[i][ACTIVE]);
        xfwmPixmapFree (&screen_info->sides[i][INACTIVE]);
    }
    for (i = 0; i < CORNER_COUNT; i++)
    {
        xfwmPixmapFree (&screen_info->corners[i][ACTIVE]);
        xfwmPixmapFree (&screen_info->corners[i][INACTIVE]);
    }
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        for (j = 0; j < STATE_COUNT; j++)
        {
            xfwmPixmapFree (&screen_info->buttons[i][j]);
        }
    }
    for (i = 0; i < TITLE_COUNT; i++)
    {
        xfwmPixmapFree (&screen_info->title[i][ACTIVE]);
        xfwmPixmapFree (&screen_info->title[i][INACTIVE]);
        xfwmPixmapFree (&screen_info->top[i][ACTIVE]);
        xfwmPixmapFree (&screen_info->top[i][INACTIVE]);
    }
    if (screen_info->box_gc != None)
    {
        XFreeGC (myScreenGetXDisplay (screen_info), screen_info->box_gc);
        screen_info->box_gc = None;
    }
}


static void
unloadKeyBindings (ScreenInfo *screen_info)
{
    int i;

    g_return_if_fail (screen_info);

    for (i = 0; i < KEY_COUNT; ++i)
    {
        g_free (screen_info->params->keys[i].internal_name);
    }
}

static void
unloadSettings (ScreenInfo *screen_info)
{
    g_return_if_fail (screen_info);

    TRACE ("entering unloadSettings");

    unloadTheme (screen_info);
    unloadKeyBindings (screen_info);
}

static gboolean
reloadScreenSettings (ScreenInfo *screen_info, int mask)
{
    g_return_val_if_fail (screen_info, FALSE);

    unloadSettings (screen_info);
    if (!loadSettings (screen_info))
    {
        return FALSE;
    }
    if (mask)
    {
        clientUpdateAllFrames (screen_info, mask);
        if (mask & UPDATE_FRAME)
        {
            compositorRebuildScreen (screen_info);
        }
    }

    return TRUE;
}

gboolean
reloadSettings (DisplayInfo *display_info, int mask)
{
    GSList *screens;

    g_return_val_if_fail (display_info, FALSE);

    TRACE ("entering reloadSettings");

    /* Refresh all screens, not just one */
    for (screens = display_info->screens; screens; screens = g_slist_next (screens))
    {
        ScreenInfo *screen_info = (ScreenInfo *) screens->data;
        if (!reloadScreenSettings (screen_info, mask))
        {
             return FALSE;
        }
    }

    return TRUE;
}

gboolean
initSettings (ScreenInfo *screen_info)
{
    GdkKeymap *keymap;
    DisplayInfo *display_info;
    char **names;
    long val;
    guint i;

    g_return_val_if_fail (screen_info, FALSE);

    TRACE ("entering initSettings");

    if (!xfconf_init (NULL))
    {
        g_critical ("Xfconf could not be initialized");
        return FALSE;
    }

    display_info = screen_info->display_info;
    names = NULL;
    val = 0;
    i = 0;

    screen_info->xfwm4_channel = xfconf_channel_new(CHANNEL_XFWM);
    g_signal_connect (screen_info->xfwm4_channel, "property-changed",
                      G_CALLBACK (cb_xfwm4_channel_property_changed), screen_info);

    keymap = gdk_keymap_get_default ();
    g_signal_connect (keymap, "keys-changed",
                      G_CALLBACK (cb_keys_changed), screen_info);

    screen_info->shortcuts_provider = xfce_shortcuts_provider_new ("xfwm4");
    g_signal_connect (screen_info->shortcuts_provider, "shortcut-added",
                      G_CALLBACK (cb_shortcut_added), screen_info);
    g_signal_connect (screen_info->shortcuts_provider, "shortcut-removed",
                      G_CALLBACK (cb_shortcut_removed), screen_info);

    if (!loadSettings (screen_info))
    {
        return FALSE;
    }
    if (getHint (display_info, screen_info->xroot, NET_NUMBER_OF_DESKTOPS, &val))
    {
        workspaceSetCount (screen_info, (guint) MAX (val, 1));
    }

    if (getUTF8StringList (display_info, screen_info->xroot, NET_DESKTOP_NAMES, &names, &i))
    {
        workspaceSetNames (screen_info, names, i);
    }
    else
    {
        screen_info->workspace_names = NULL;
        screen_info->workspace_names_items = 0;
    }

    getDesktopLayout(display_info, screen_info->xroot, screen_info->workspace_count, &screen_info->desktop_layout);
    placeSidewalks (screen_info, screen_info->params->wrap_workspaces);

    return TRUE;
}

void
closeSettings (ScreenInfo *screen_info)
{
    g_return_if_fail (screen_info);

    unloadSettings (screen_info);
}

static void
cb_xfwm4_channel_property_changed(XfconfChannel *channel, const gchar *property_name, const GValue *value, ScreenInfo *screen_info)
{
    if (g_str_has_prefix(property_name, "/general/") == TRUE)
    {
        const gchar *name = &property_name[9];
        switch (G_VALUE_TYPE(value))
        {
            case G_TYPE_STRING:
                if (!strcmp (name, "double_click_action"))
                {
                    reloadScreenSettings (screen_info, NO_UPDATE_FLAG);
                }
                else if (!strcmp (name, "theme"))
                {
                    reloadScreenSettings(screen_info, UPDATE_MAXIMIZE | UPDATE_GRAVITY | UPDATE_CACHE);
                }
                else if (!strcmp (name, "button_layout"))
                {
                    reloadScreenSettings (screen_info, UPDATE_FRAME | UPDATE_CACHE);
                }
                else if (!strcmp (name, "title_alignment"))
                {
                    reloadScreenSettings (screen_info, UPDATE_FRAME | UPDATE_CACHE);
                }
                else if (!strcmp (name, "title_font"))
                {
                    reloadScreenSettings (screen_info, UPDATE_FRAME | UPDATE_CACHE);
                }
                 else if (!strcmp (name, "easy_click"))
                {
                    reloadScreenSettings (screen_info, UPDATE_BUTTON_GRABS);
                }
                else if (!strcmp (name, "activate_action"))
                {
                    set_activate_action (screen_info, g_value_get_string (value));
                }
                else if (!strcmp (name, "placement_mode"))
                {
                    set_placement_mode (screen_info, g_value_get_string (value));
                }
                else if ((!strcmp (name, "title_shadow_active"))
                      || (!strcmp (name, "title_shadow_inactive")))
                {
                    /* These properties are not configurable via xfconf */
                }
                else
                {
                    g_warning("The property '%s' of type string is not supported", property_name);
                }
                break;
            case G_TYPE_INT:
                if (!strcmp (name, "raise_delay"))
                {
                    screen_info->params->raise_delay = CLAMP (g_value_get_int (value), 5, 2000);
                }
                else if (!strcmp (name, "focus_delay"))
                {
                    screen_info->params->focus_delay = CLAMP (g_value_get_int (value), 5, 2000);
                }
                else if (!strcmp (name, "snap_width"))
                {
                    screen_info->params->snap_width = CLAMP (g_value_get_int (value), 5, 100);
                }
                else if (!strcmp (name, "wrap_resistance"))
                {
                    screen_info->params->wrap_resistance = CLAMP (g_value_get_int (value), 5, 100);
                }
                else if (!strcmp (name, "margin_left"))
                {
                    set_settings_margin (screen_info, STRUTS_LEFT, g_value_get_int (value));
                }
                else if (!strcmp (name, "margin_right"))
                {
                    set_settings_margin (screen_info, STRUTS_RIGHT, g_value_get_int (value));
                }
                else if (!strcmp (name, "margin_bottom"))
                {
                    set_settings_margin (screen_info, STRUTS_BOTTOM, g_value_get_int (value));
                }
                else if (!strcmp (name, "margin_top"))
                {
                    set_settings_margin (screen_info, STRUTS_TOP, g_value_get_int (value));
                }
                else if (!strcmp (name, "workspace_count"))
                {
                    workspaceSetCount(screen_info, (guint) MAX (g_value_get_int (value), 1));
                }
                else if (!strcmp (name, "frame_opacity"))
                {
                    screen_info->params->frame_opacity = CLAMP (g_value_get_int(value), 0, 100);
                    reloadScreenSettings (screen_info, UPDATE_FRAME);
                }
                else if (!strcmp (name, "inactive_opacity"))
                {
                    screen_info->params->inactive_opacity = CLAMP (g_value_get_int(value), 0, 100);
                    reloadScreenSettings (screen_info, UPDATE_FRAME);
                    clientUpdateAllOpacity (screen_info);
                }
                else if (!strcmp (name, "move_opacity"))
                {
                    screen_info->params->move_opacity = CLAMP (g_value_get_int(value), 0, 100);
                }
                else if (!strcmp (name, "resize_opacity"))
                {
                    screen_info->params->resize_opacity = CLAMP (g_value_get_int(value), 0, 100);
                }
                else if (!strcmp (name, "popup_opacity"))
                {
                    screen_info->params->popup_opacity = CLAMP (g_value_get_int(value), 0, 100);
                    reloadScreenSettings (screen_info, UPDATE_FRAME);
                }
                else if (!strcmp (name, "placement_ratio"))
                {
                    screen_info->params->placement_ratio = CLAMP (g_value_get_int(value), 0, 100);
                }
                else if ((!strcmp (name, "button_offset"))
                      || (!strcmp (name, "button_spacing"))
                      || (!strcmp (name, "double_click_time"))
                      || (!strcmp (name, "double_DOUBLE_CLICKclick_distance"))
                      || (!strcmp (name, "maximized_offset"))
                      || (!strcmp (name, "shadow_delta_height"))
                      || (!strcmp (name, "shadow_delta_width"))
                      || (!strcmp (name, "shadow_delta_x"))
                      || (!strcmp (name, "shadow_delta_y"))
                      || (!strcmp (name, "shadow_opacity"))
                      || (!strcmp (name, "title_horizontal_offset"))
                      || (!strcmp (name, "title_vertical_offset_active"))
                      || (!strcmp (name, "title_vertical_offset_inactive")))
                {
                    /* These properties are not configurable via xfconf */
                }
                else
                {
                    g_warning("The property '%s' of type int is not supported", property_name);
                }
                break;
            case G_TYPE_BOOLEAN:
                if (!strcmp (name, "box_move"))
                {
                    screen_info->params->box_move = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "box_resize"))
                {
                    screen_info->params->box_resize = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "click_to_focus"))
                {
                    screen_info->params->click_to_focus = g_value_get_boolean (value);
                    update_grabs (screen_info);
                }
                else if (!strcmp (name, "focus_new"))
                {
                    screen_info->params->focus_new = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "raise_on_focus"))
                {
                    screen_info->params->raise_on_focus = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "raise_on_click"))
                {
                    screen_info->params->raise_on_click = g_value_get_boolean (value);
                    update_grabs (screen_info);
                }
                else if (!strcmp (name, "repeat_urgent_blink"))
                {
                    screen_info->params->repeat_urgent_blink = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "urgent_blink"))
                {
                    screen_info->params->urgent_blink = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "snap_to_border"))
                {
                    screen_info->params->snap_to_border = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "snap_to_windows"))
                {
                    screen_info->params->snap_to_windows = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "wrap_workspaces"))
                {
                    screen_info->params->wrap_workspaces = g_value_get_boolean (value);
                    placeSidewalks (screen_info, screen_info->params->wrap_workspaces);
                }
                else if (!strcmp (name, "wrap_windows"))
                {
                    screen_info->params->wrap_windows = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "borderless_maximize"))
                {
                    screen_info->params->borderless_maximize = g_value_get_boolean (value);
                    reloadScreenSettings (screen_info, UPDATE_MAXIMIZE);
                }
                else if (!strcmp (name, "cycle_minimum"))
                {
                    screen_info->params->cycle_minimum = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "cycle_draw_frame"))
                {
                    screen_info->params->cycle_draw_frame = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "cycle_hidden"))
                {
                    screen_info->params->cycle_hidden = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "cycle_workspaces"))
                {
                    screen_info->params->cycle_workspaces = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "cycle_apps_only"))
                {
                    screen_info->params->cycle_apps_only = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "focus_hint"))
                {
                    screen_info->params->focus_hint = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "lower_on_middleclick"))
                {
                    screen_info->params->lower_on_middleclick = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "mousewheel_rollup"))
                {
                    screen_info->params->mousewheel_rollup = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "prevent_focus_stealing"))
                {
                    screen_info->params->prevent_focus_stealing = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "raise_with_any_button"))
                {
                    screen_info->params->raise_with_any_button = g_value_get_boolean (value);
                    update_grabs (screen_info);
                }
                else if (!strcmp (name, "restore_on_move"))
                {
                    screen_info->params->restore_on_move = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "scroll_workspaces"))
                {
                    screen_info->params->scroll_workspaces = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "show_dock_shadow"))
                {
                    screen_info->params->show_dock_shadow = g_value_get_boolean (value);
                    reloadScreenSettings (screen_info, UPDATE_FRAME);
                }
                else if (!strcmp (name, "show_frame_shadow"))
                {
                    screen_info->params->show_frame_shadow = g_value_get_boolean (value);
                    reloadScreenSettings (screen_info, UPDATE_FRAME);
                }
                else if (!strcmp (name, "show_popup_shadow"))
                {
                    screen_info->params->show_popup_shadow = g_value_get_boolean (value);
                    reloadScreenSettings (screen_info, UPDATE_FRAME);
                }
                else if (!strcmp (name, "snap_resist"))
                {
                    screen_info->params->snap_resist = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "tile_on_move"))
                {
                    screen_info->params->tile_on_move = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "sync_to_vblank"))
                {
                    screen_info->params->sync_to_vblank = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "toggle_workspaces"))
                {
                    screen_info->params->toggle_workspaces = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "unredirect_overlays"))
                {
                    screen_info->params->unredirect_overlays = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "use_compositing"))
                {
                    screen_info->params->use_compositing = g_value_get_boolean (value);
                    compositorActivateScreen (screen_info,
                                              screen_info->params->use_compositing);
                }
                else if (!strcmp (name, "wrap_layout"))
                {
                    screen_info->params->wrap_layout = g_value_get_boolean (value);
                }
                else if (!strcmp (name, "wrap_cycle"))
                {
                    screen_info->params->wrap_cycle = g_value_get_boolean (value);
                }
                else if ((!strcmp (name, "full_width_title"))
                      || (!strcmp (name, "show_app_icon")))
                {
                    /* These properties are not configurable via xfconf */
                }
                else
                {
                    g_warning("The property '%s' of type boolean is not supported", property_name);
                }
                break;
            default:
                if (!strcmp (name, "workspace_names"))
                {
                    /* These properties are not configurable via xfconf */
                }
                else
                {
                    g_warning("The property '%s' is not supported", property_name);
                }
                break;
        }
    }
}

static void
cb_keys_changed (GdkKeymap *keymap, ScreenInfo *screen_info)
{
    /* Recompute modifiers mask in case it changed */
    initModifiers (myScreenGetXDisplay (screen_info));

    /* Ungrab/regrab shortcuts */
    myScreenUngrabKeys (screen_info);
    myScreenGrabKeys (screen_info);

    /* Uupdate all grabs for mouse buttons */
    clientUpdateAllFrames (screen_info, UPDATE_BUTTON_GRABS);
}

static void
cb_shortcut_added (XfceShortcutsProvider *provider, const gchar *shortcut,
                   ScreenInfo *screen_info)
{
    XfceShortcut *sc;
    Display *dpy;
    int i;

    g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider));
    g_return_if_fail (shortcut);
    g_return_if_fail (screen_info);

    sc = xfce_shortcuts_provider_get_shortcut (provider, shortcut);

    if (sc == NULL)
    {
        return;
    }

    dpy = myScreenGetXDisplay (screen_info);

    for (i = 0; i < KEY_COUNT; ++i)
    {
        if (g_str_equal (screen_info->params->keys[i].internal_name, sc->command))
        {
            parseKeyString (dpy, &screen_info->params->keys[i], shortcut);

            myScreenUngrabKeys (screen_info);
            myScreenGrabKeys (screen_info);
            break;
        }
    }

    xfce_shortcut_free (sc);
}

static void
cb_shortcut_removed (XfceShortcutsProvider *provider, const gchar *shortcut,
                     ScreenInfo *screen_info)
{
    MyKey key;
    Display *dpy;
    int i;

    g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (provider));
    g_return_if_fail (screen_info);
    g_return_if_fail (shortcut);

    dpy = myScreenGetXDisplay (screen_info);

    parseKeyString (dpy, &key, shortcut);

    for (i = 0; i < KEY_COUNT; ++i)
    {
        if (screen_info->params->keys[i].keycode == key.keycode &&
            screen_info->params->keys[i].modifier == key.modifier)
        {
            screen_info->params->keys[i].keycode = 0;
            screen_info->params->keys[i].modifier = 0;

            myScreenUngrabKeys (screen_info);
            myScreenGrabKeys (screen_info);
            break;
        }
    }
}

static void
parseShortcut (ScreenInfo *screen_info, int id, const gchar *name,
               GList *shortcuts)
{
    Display *dpy;
    const gchar *shortcut;

    g_return_if_fail (screen_info);
    g_return_if_fail (id >= 0 && id < KEY_COUNT);

    dpy = myScreenGetXDisplay (screen_info);
    shortcut = getShortcut (name, shortcuts);
    parseKeyString (dpy, &screen_info->params->keys[id], shortcut);

    screen_info->params->keys[id].internal_name = g_strdup (name);
}

static const gchar *
getShortcut (const gchar *name, GList *shortcuts)
{
    XfceShortcut *shortcut;
    GList *iter;
    const gchar *result = NULL;

    for (iter = shortcuts; iter != NULL; iter = g_list_next (iter))
    {
        shortcut = iter->data;

        if (g_str_equal (shortcut->command, name))
        {
            result = shortcut->shortcut;
            break;
        }
    }

    return result;
}
