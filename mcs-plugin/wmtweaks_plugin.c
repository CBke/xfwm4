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

#define RCDIR    "mcs_settings"
#define OLDRCDIR "settings"
#define RCFILE    "wmtweaks.xml"
#define CHANNEL  "wmtweaks"
#define PLUGIN_NAME "wmtweaks"
#define BORDER 5

typedef struct _ValuePair ValuePair;
struct _ValuePair
{
    gchar *label;
    gchar *value;
};

static void xfwm4_create_channel (McsPlugin * mcs_plugin);
static void run_dialog (McsPlugin * mcs_plugin);

static gboolean is_running = FALSE;

static gboolean borderless_maximize = TRUE;
static gboolean cycle_minimum = TRUE;
static gboolean cycle_hidden = TRUE;
static gboolean cycle_workspaces = FALSE;
static gboolean focus_hint = TRUE;
static gboolean show_dock_shadow = FALSE;
static gboolean show_frame_shadow = FALSE;
static gboolean show_popup_shadow = FALSE;
static gboolean prevent_focus_stealing = FALSE;
static gboolean raise_with_any_button = FALSE;
static gboolean restore_on_move = TRUE;
static gboolean scroll_workspaces = TRUE;
static gboolean snap_resist = FALSE;
static gboolean toggle_workspaces = FALSE;
static gboolean unredirect_overlays = TRUE;
static gboolean use_compositing = FALSE;
static gboolean wrap_layout = FALSE;
static gboolean wrap_cycle = FALSE;

static int placement_ratio = 20;
static int inactive_opacity = 100;
static int move_opacity = 100;
static int resize_opacity = 100;
static int popup_opacity = 100;
static int frame_opacity = 100;

static char *easy_click = "Alt";
static char *activate_action = "bring";
static char *placement_mode = "center";


/*
    "Xfwm/ActivateAction"
    "Xfwm/BorderlessMaximize"
    "Xfwm/CycleHidden"
    "Xfwm/CycleMinimum"
    "Xfwm/CycleWorkspaces"
    "Xfwm/EasyClick"
    "Xfwm/FocusHint"
    "Xfwm/FrameOpacity"
    "Xfwm/InactiveOpacity"
    "Xfwm/MoveOpacity"
    "Xfwm/PlacementRatio"
    "Xfwm/PlacementMode"
    "Xfwm/PopupOpacity"
    "Xfwm/PreventFocusStealing"
    "Xfwm/RaiseWithAnyButton"
    "Xfwm/ResizeOpacity"
    "Xfwm/RestoreOnMove"
    "Xfwm/ScrollWorkspaces"
    "Xfwm/ScrollWorkspaces"
    "Xfwm/ShowDockShadow"
    "Xfwm/ShowFrameShadow"
    "Xfwm/ShowPopupShadow"
    "Xfwm/SnapResist"
    "Xfwm/UnredirectOverlays"
    "Xfwm/UseCompositing"
    "Xfwm/WrapCycle"
    "Xfwm/WrapLayout"
 */

typedef struct _Itf Itf;
struct _Itf
{
    McsPlugin *mcs_plugin;
    GtkWidget *tweaks_dialog;
};

static gboolean
write_options (McsPlugin * mcs_plugin)
{
    gchar *rcfile, *path;
    gboolean result = FALSE;

    path = g_build_filename ("xfce4", "mcs_settings", RCFILE, NULL);
    rcfile = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, path, TRUE);
    if (G_LIKELY (rcfile != NULL))
    {
        result = mcs_manager_save_channel_to_file (mcs_plugin->manager, CHANNEL, rcfile);
        g_free (rcfile);
    }
    g_free (path);

    return result;
}

static void
cb_gboolean_changed (GtkWidget * widget, gpointer user_data)
{
    gboolean *value = (gboolean *) user_data;
    gchar *setting_name = NULL;
    McsPlugin *mcs_plugin = NULL;

    *value = (gboolean) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
    setting_name = (gchar *) g_object_get_data (G_OBJECT (widget), "setting-name");
    mcs_plugin = (McsPlugin *) g_object_get_data (G_OBJECT (widget), "mcs-plugin");

    g_assert (setting_name);
    g_assert (mcs_plugin);

    mcs_manager_set_int (mcs_plugin->manager, setting_name, CHANNEL, *value ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_gint_changed (GtkWidget * widget, gpointer user_data)
{
    int *value = (int *) user_data;
    gchar *setting_name = NULL;
    McsPlugin *mcs_plugin = NULL;

    *value = (int) gtk_range_get_value (GTK_RANGE (widget));
    setting_name = (gchar *) g_object_get_data (G_OBJECT (widget), "setting-name");
    mcs_plugin = (McsPlugin *) g_object_get_data (G_OBJECT (widget), "mcs-plugin");

    g_assert (setting_name);
    g_assert (mcs_plugin);

    mcs_manager_set_int (mcs_plugin->manager, setting_name, CHANNEL, *value);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_string_changed (GtkWidget * widget, gpointer user_data)
{
    gchar *value = (gchar *) user_data;
    gchar *setting_name = NULL;
    McsPlugin *mcs_plugin = NULL;

    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
        return;

    setting_name = (gchar *) g_object_get_data (G_OBJECT (widget), "setting-name");
    mcs_plugin = (McsPlugin *) g_object_get_data (G_OBJECT (widget), "mcs-plugin");

    g_assert (setting_name);
    g_assert (mcs_plugin);

    mcs_manager_set_string (mcs_plugin->manager, setting_name, CHANNEL, value);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_menuitem_changed (GtkWidget * widget, gpointer user_data)
{
    const gchar **value = user_data;
    gchar *setting_name;
    McsPlugin *mcs_plugin;
    const gchar *const *values;

    values = g_object_get_data (G_OBJECT (widget), "setting-values");
    setting_name = (gchar *) g_object_get_data (G_OBJECT (widget), "setting-name");
    mcs_plugin = (McsPlugin *) g_object_get_data (G_OBJECT (widget), "mcs-plugin");

    g_assert (setting_name);
    g_assert (mcs_plugin);
    g_assert (values);

    *value = values[gtk_combo_box_get_active (GTK_COMBO_BOX (widget))];

    mcs_manager_set_string (mcs_plugin->manager, setting_name, CHANNEL, *value);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static GtkWidget *
create_gboolean_button (McsPlugin * mcs_plugin, const gchar * label, gchar * setting_name,
                        gboolean * value)
{
    GtkWidget *check_button;
    GtkWidget *label_widget;

    label_widget = gtk_label_new_with_mnemonic (label);
    gtk_label_set_line_wrap (GTK_LABEL (label_widget), TRUE);
    gtk_widget_show (label_widget);

    check_button = gtk_check_button_new ();
    gtk_container_add (GTK_CONTAINER (check_button), label_widget);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), *value);

    g_object_set_data (G_OBJECT (check_button), "setting-name", setting_name);
    g_object_set_data (G_OBJECT (check_button), "mcs-plugin", mcs_plugin);
    g_signal_connect (G_OBJECT (check_button), "toggled", G_CALLBACK (cb_gboolean_changed), value);

    return check_button;
}

static GtkWidget *
create_int_range (McsPlugin * mcs_plugin, gchar * label, const gchar * min_label, const gchar * max_label,
                  gchar * setting_name, gint min, gint max, gint step, int *value)
{
    GtkWidget *vbox;
    GtkObject *adjustment;
    GtkWidget *scale;
    GtkWidget *table;
    GtkWidget *label_widget;

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_widget_show (vbox);

    label_widget = gtk_label_new (label);
    gtk_label_set_justify (GTK_LABEL (label_widget), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label_widget), 0, 0.5);
    gtk_label_set_line_wrap (GTK_LABEL (label_widget), TRUE);
    gtk_box_pack_start (GTK_BOX (vbox), label_widget, FALSE, TRUE, 0);
    gtk_widget_show (label_widget);

    table = gtk_table_new (1, 3, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (table), 0);
    gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 0);
    gtk_widget_show (table);

    label_widget = xfce_create_small_label (min_label);
    gtk_table_attach (GTK_TABLE (table), label_widget, 0, 1, 0, 1, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label_widget), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label_widget), 1, 0.5);
    gtk_label_set_line_wrap (GTK_LABEL (label_widget), TRUE);
    gtk_widget_show (label_widget);

    label_widget = xfce_create_small_label (max_label);
    gtk_table_attach (GTK_TABLE (table), label_widget, 2, 3, 0, 1, (GtkAttachOptions) (GTK_FILL),
        (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label_widget), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);
    gtk_widget_show (label_widget);

    adjustment = gtk_adjustment_new ((gdouble) * value, (gdouble) min, (gdouble) max,
        (gdouble) step, (gdouble) 10 * step, 0);
    scale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
    gtk_table_attach (GTK_TABLE (table), scale, 1, 2, 0, 1,
        (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
        (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (scale), 0);
    gtk_range_set_update_policy (GTK_RANGE (scale), GTK_UPDATE_DISCONTINUOUS);
    gtk_widget_show (scale);

    g_object_set_data (G_OBJECT (scale), "setting-name", setting_name);
    g_object_set_data (G_OBJECT (scale), "mcs-plugin", mcs_plugin);
    g_signal_connect (G_OBJECT (scale), "value_changed", G_CALLBACK (cb_gint_changed), value);

    return vbox;
}

static GtkWidget *
create_option_menu (McsPlugin * mcs_plugin, const gchar *const values[],
                    const gchar * label, gchar * setting_name, gchar ** value)
{
    GtkWidget *hbox;
    GtkWidget *label_widget;
    GtkWidget *omenu;
    guint n;

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox);

    label_widget = gtk_label_new (label);
    gtk_label_set_line_wrap (GTK_LABEL (label_widget), FALSE);
    gtk_box_pack_start (GTK_BOX (hbox), label_widget, FALSE, FALSE, 2);
    gtk_label_set_justify (GTK_LABEL (label_widget), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label_widget), 0.0, 0.5);
    gtk_widget_show (label_widget);

    omenu = gtk_combo_box_new_text ();
    gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, TRUE, 2);
    gtk_widget_show (omenu);

    n = 0;
    while (values[n])
    {
        gtk_combo_box_append_text (GTK_COMBO_BOX (omenu), gettext (values[n]));

        if (!g_ascii_strcasecmp (*value, values[n]))
            gtk_combo_box_set_active (GTK_COMBO_BOX (omenu), n);
        n++;
    }

    g_object_set_data (G_OBJECT (omenu), "mcs-plugin", mcs_plugin);
    g_object_set_data (G_OBJECT (omenu), "setting-name", setting_name);
    g_object_set_data (G_OBJECT (omenu), "setting-values", (gpointer) values);
    g_signal_connect (G_OBJECT (omenu), "changed", G_CALLBACK (cb_menuitem_changed), value);

    return (hbox);
}

static GtkWidget *
create_string_radio_button (McsPlugin * mcs_plugin, const ValuePair values[],
                            const gchar * label, gchar * setting_name, gchar ** value)
{
    GtkWidget *vbox1, *vbox2;
    GtkWidget *radio_button;
    GtkWidget *label_widget;
    GSList *group;
    guint n;

    vbox1 = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox1), BORDER);
    gtk_widget_show (vbox1);

    label_widget = gtk_label_new (label);
    gtk_label_set_justify (GTK_LABEL (label_widget), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label_widget), 0, 0.5);
    gtk_label_set_line_wrap (GTK_LABEL (label_widget), TRUE);
    gtk_box_pack_start (GTK_BOX (vbox1), label_widget, FALSE, TRUE, 2);
    gtk_widget_show (label_widget);

    vbox2 = gtk_vbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox2), 0);
    gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, TRUE, 0);
    gtk_widget_show (vbox2);

    group = NULL;
    n = 0;
    while (values[n].label)
    {
        label_widget = gtk_label_new_with_mnemonic (values[n].label);
        gtk_label_set_line_wrap (GTK_LABEL (label_widget), TRUE);
        gtk_widget_show (label_widget);

        radio_button = gtk_radio_button_new (group);
        gtk_widget_show (radio_button);
        group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button));
        gtk_container_add (GTK_CONTAINER (radio_button), label_widget);

        if (!g_ascii_strcasecmp (*value, values[n].value))
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), TRUE);

        gtk_box_pack_start (GTK_BOX (vbox2), radio_button, FALSE, TRUE, 0);

        g_object_set_data (G_OBJECT (radio_button), "setting-name", setting_name);
        g_object_set_data (G_OBJECT (radio_button), "mcs-plugin", mcs_plugin);
        g_signal_connect (G_OBJECT (radio_button), "toggled", G_CALLBACK (cb_string_changed), values[n].value);

        n++;
    }

    return (vbox1);
}

#ifdef HAVE_COMPOSITOR
static void
cb_compositor_changed (GtkWidget * widget, gpointer user_data)
{
    gtk_widget_set_sensitive (GTK_WIDGET (user_data), gtk_toggle_button_get_active GTK_TOGGLE_BUTTON (widget));
}
#endif

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
#ifdef HAVE_COMPOSITOR
    static Atom composite = None;
    GtkWidget *compositor_options_vbox;
#endif
    Itf *dialog;
    GtkWidget *dialog_vbox;
    GtkWidget *notebook;
    GtkWidget *label;
    GtkWidget *vbox;
    GtkWidget *check_button;
    GtkWidget *radio_buttons;
    GtkWidget *option_menu;
    GtkWidget *range;
    GtkWidget *action_area;
    GtkWidget *button;
    guint nth = 0;

    static const gchar *const modifier_list[] = {
        "Alt",
        "Control",
        "Hyper",
        "Meta",
        "Shift",
        "Super",
        "Mod1",
        "Mod2",
        "Mod3",
        "Mod4",
        "Mod5",
        N_("None"),
        NULL
    };

    static const ValuePair activate_list[] = {
        {N_("Bring window on current workspace"), "bring"},
        {N_("Switch to window's workspace"), "switch"},
        {N_("Do nothing"), "none"},
        {NULL, NULL}
    };

    static const ValuePair placement_list[] = {
        {N_("Place window under the mouse"), "mouse"},
        {N_("Place window in the center"), "center"},
        {NULL, NULL}
    };

    dialog = g_new (Itf, 1);

    dialog->mcs_plugin = mcs_plugin;

    dialog->tweaks_dialog = xfce_titled_dialog_new ();
    gtk_window_set_title (GTK_WINDOW (dialog->tweaks_dialog), _("Window Manager Tweaks"));
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog->tweaks_dialog), FALSE);
    gtk_window_set_icon_name(GTK_WINDOW(dialog->tweaks_dialog), "wmtweaks");

    dialog_vbox = GTK_DIALOG (dialog->tweaks_dialog)->vbox;
    gtk_widget_show (dialog_vbox);

    notebook = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (notebook), BORDER + 1);
    gtk_widget_show (notebook);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), notebook, TRUE, TRUE, BORDER);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_container_add (GTK_CONTAINER (notebook), vbox);
    gtk_widget_show (vbox);

    check_button =
        create_gboolean_button (mcs_plugin,
        _("Skip windows that have \"skip pager\" or \"skip taskbar\" properties set"),
        "Xfwm/CycleMinimum", &cycle_minimum);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    check_button =
        create_gboolean_button (mcs_plugin, _("Include hidden (i.e. iconified) windows"),
        "Xfwm/CycleHidden", &cycle_hidden);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    check_button =
        create_gboolean_button (mcs_plugin, _("Cycle through windows from all workspaces"),
        "Xfwm/CycleWorkspaces", &cycle_workspaces);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    label = gtk_label_new (_("Cycling"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), nth++), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_container_add (GTK_CONTAINER (notebook), vbox);
    gtk_widget_show (vbox);

    check_button =
        create_gboolean_button (mcs_plugin, _("Activate focus stealing prevention"),
        "Xfwm/PreventFocusStealing", &prevent_focus_stealing);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    check_button =
        create_gboolean_button (mcs_plugin, _("Honor the standard ICCCM focus hint"),
        "Xfwm/FocusHint", &focus_hint);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    radio_buttons =
        create_string_radio_button (mcs_plugin, activate_list, _("When a window raises itself:"),
                                    "Xfwm/ActivateAction", &activate_action);
    gtk_box_pack_start (GTK_BOX (vbox), radio_buttons, FALSE, TRUE, 0);
    gtk_widget_show (radio_buttons);

    label = gtk_label_new (_("Focus"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), nth++), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_container_add (GTK_CONTAINER (notebook), vbox);
    gtk_widget_show (vbox);

    option_menu =
        create_option_menu (mcs_plugin, modifier_list, _("Key used to grab and move windows"),
        "Xfwm/EasyClick", &easy_click);
    gtk_box_pack_start (GTK_BOX (vbox), option_menu, FALSE, TRUE, 0);
    gtk_widget_show (option_menu);

    check_button =
        create_gboolean_button (mcs_plugin, _("Raise windows when any mouse button is pressed"),
        "Xfwm/RaiseWithAnyButton", &raise_with_any_button);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    check_button =
        create_gboolean_button (mcs_plugin,
        _("Hide frame of windows when maximized"), "Xfwm/BorderlessMaximize",
        &borderless_maximize);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    check_button =
        create_gboolean_button (mcs_plugin,
        _("Restore original size of maximized windows when moving"), "Xfwm/RestoreOnMove",
        &restore_on_move);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    check_button =
        create_gboolean_button (mcs_plugin, _("Use edge resistance instead of windows snapping"),
        "Xfwm/SnapResist", &snap_resist);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    label = gtk_label_new (_("Accessibility"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), nth++), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_container_add (GTK_CONTAINER (notebook), vbox);
    gtk_widget_show (vbox);

    check_button =
        create_gboolean_button (mcs_plugin,
        _("Switch workspaces using the mouse wheel over the desktop"), "Xfwm/ScrollWorkspaces",
        &scroll_workspaces);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    check_button =
        create_gboolean_button (mcs_plugin,
        _("Remember and recall previous workspace when switching via keyboard shortcuts"),
        "Xfwm/ToggleWorkspaces", &toggle_workspaces);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    check_button =
        create_gboolean_button (mcs_plugin,
        _("Wrap workspaces depending on the actual desktop layout"), "Xfwm/WrapLayout",
        &wrap_layout);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    check_button =
        create_gboolean_button (mcs_plugin,
        _("Wrap workspaces when the first or last workspace is reached"), "Xfwm/WrapCycle",
        &wrap_cycle);
    gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
    gtk_widget_show (check_button);

    label = gtk_label_new (_("Workspaces"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), nth++), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_container_add (GTK_CONTAINER (notebook), vbox);
    gtk_widget_show (vbox);

    range =
        create_int_range (mcs_plugin, _("Minimum size of windows to trigger smart placement"),
        Q_("Size|Small"), Q_("Size|Large"), "Xfwm/PlacementRatio", 0, 100, 5, &placement_ratio);
    gtk_box_pack_start (GTK_BOX (vbox), range, FALSE, TRUE, 0);
    gtk_widget_show (range);

    radio_buttons =
        create_string_radio_button (mcs_plugin, placement_list, _("Default positionning of windows without smart placement:"),
                                    "Xfwm/PlacementMode", &placement_mode);
    gtk_box_pack_start (GTK_BOX (vbox), radio_buttons, FALSE, TRUE, 0);
    gtk_widget_show (radio_buttons);

    label = gtk_label_new (_("Placement"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
        gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), nth++), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

#ifdef HAVE_COMPOSITOR
    if (G_UNLIKELY (!composite))
    {
        composite = XInternAtom (GDK_DISPLAY (), "XFWM4_COMPOSITING_MANAGER", False);
    }

    if (XGetSelectionOwner (GDK_DISPLAY (), composite))
    {
        vbox = gtk_vbox_new (FALSE, BORDER);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
        gtk_container_add (GTK_CONTAINER (notebook), vbox);
        gtk_widget_show (vbox);

        check_button =
            create_gboolean_button (mcs_plugin, _("Enable display compositing"),
            "Xfwm/UseCompositing", &use_compositing);
        gtk_box_pack_start (GTK_BOX (vbox), check_button, FALSE, TRUE, 0);
        gtk_widget_show (check_button);

        compositor_options_vbox = gtk_vbox_new (FALSE, BORDER);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
        gtk_container_add (GTK_CONTAINER (vbox), compositor_options_vbox);
        gtk_widget_show (compositor_options_vbox);

        gtk_widget_set_sensitive (compositor_options_vbox, gtk_toggle_button_get_active GTK_TOGGLE_BUTTON (check_button));
        g_signal_connect (G_OBJECT (check_button), "toggled", G_CALLBACK (cb_compositor_changed), compositor_options_vbox);

        check_button =
            create_gboolean_button (mcs_plugin, _("Display full screen overlay windows directly"),
            "Xfwm/UnredirectOverlays", &unredirect_overlays);
        gtk_box_pack_start (GTK_BOX (compositor_options_vbox), check_button, FALSE, TRUE, 0);
        gtk_widget_show (check_button);

        check_button =
            create_gboolean_button (mcs_plugin, _("Show shadows under dock windows"),
            "Xfwm/ShowDockShadow", &show_dock_shadow);
        gtk_box_pack_start (GTK_BOX (compositor_options_vbox), check_button, FALSE, TRUE, 0);
        gtk_widget_show (check_button);

        check_button =
            create_gboolean_button (mcs_plugin, _("Show shadows under regular windows"),
            "Xfwm/ShowFrameShadow", &show_frame_shadow);
        gtk_box_pack_start (GTK_BOX (compositor_options_vbox), check_button, FALSE, TRUE, 0);
        gtk_widget_show (check_button);

        check_button =
            create_gboolean_button (mcs_plugin, _("Show shadows under popup windows"),
            "Xfwm/ShowPopupShadow", &show_popup_shadow);
        gtk_box_pack_start (GTK_BOX (compositor_options_vbox), check_button, FALSE, TRUE, 0);
        gtk_widget_show (check_button);

        range =
            create_int_range (mcs_plugin, _("Opacity of window decorations"), _("Transparent"),
            _("Opaque"), "Xfwm/FrameOpacity", 50, 100, 5, &frame_opacity);
        gtk_box_pack_start (GTK_BOX (compositor_options_vbox), range, FALSE, TRUE, 0);
        gtk_widget_show (range);

        range =
            create_int_range (mcs_plugin, _("Opacity of inactive windows"), _("Transparent"),
            _("Opaque"), "Xfwm/InactiveOpacity", 10, 100, 5, &inactive_opacity);
        gtk_box_pack_start (GTK_BOX (compositor_options_vbox), range, FALSE, TRUE, 0);
        gtk_widget_show (range);

        range =
            create_int_range (mcs_plugin, _("Opacity of windows during move"), _("Transparent"),
            _("Opaque"), "Xfwm/MoveOpacity", 10, 100, 5, &move_opacity);
        gtk_box_pack_start (GTK_BOX (compositor_options_vbox), range, FALSE, TRUE, 0);
        gtk_widget_show (range);

        range =
            create_int_range (mcs_plugin, _("Opacity of windows during resize"), _("Transparent"),
            _("Opaque"), "Xfwm/ResizeOpacity", 10, 100, 5, &resize_opacity);
        gtk_box_pack_start (GTK_BOX (compositor_options_vbox), range, FALSE, TRUE, 0);
        gtk_widget_show (range);

        range =
            create_int_range (mcs_plugin, _("Opacity of popup windows"), _("Transparent"),
            _("Opaque"), "Xfwm/PopupOpacity", 10, 100, 5, &popup_opacity);
        gtk_box_pack_start (GTK_BOX (compositor_options_vbox), range, FALSE, TRUE, 0);
        gtk_widget_show (range);

        label = gtk_label_new (_("Compositor"));
        gtk_widget_show (label);
        gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook),
            gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), nth++), label);
        gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    }
#endif

    action_area = GTK_DIALOG (dialog->tweaks_dialog)->action_area;
    gtk_widget_show (action_area);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (action_area), GTK_BUTTONBOX_END);

    button = gtk_button_new_from_stock ("gtk-close");
    gtk_widget_show (button);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog->tweaks_dialog), button, GTK_RESPONSE_CLOSE);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

    gtk_widget_grab_focus (button);
    gtk_widget_grab_default (button);
    g_signal_connect (G_OBJECT (dialog->tweaks_dialog), "response", G_CALLBACK (cb_dialog_response),
        dialog->mcs_plugin);

    return dialog;
}

static void
setup_dialog (Itf * itf)
{
    xfce_gtk_window_center_on_monitor_with_pointer (GTK_WINDOW (itf->tweaks_dialog));
    gdk_x11_window_set_user_time (GTK_WIDGET (itf->tweaks_dialog)->window,
        gdk_x11_get_server_time (GTK_WIDGET (itf->tweaks_dialog)->window));
    gtk_widget_show (itf->tweaks_dialog);
}

McsPluginInitResult
mcs_plugin_init (McsPlugin * mcs_plugin)
{
    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    xfwm4_create_channel (mcs_plugin);
    mcs_plugin->plugin_name = g_strdup (PLUGIN_NAME);
    /* the button label in the xfce-mcs-manager dialog */
    mcs_plugin->caption = g_strdup (Q_ ("Button Label|Window Manager Tweaks"));
    mcs_plugin->run_dialog = run_dialog;
    mcs_plugin->icon = xfce_themed_icon_load ("wmtweaks", 48);
    if (G_LIKELY (mcs_plugin->icon != NULL))
      g_object_set_data_full (G_OBJECT (mcs_plugin->icon), "mcs-plugin-icon-name", g_strdup ("wmtweaks"), g_free);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);

    return (MCS_PLUGIN_INIT_OK);
}

static void
init_gboolean_setting (McsPlugin * mcs_plugin, gchar * setting_name, gboolean * value)
{
    McsSetting *setting;

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, setting_name, CHANNEL);
    if (setting)
    {
        *value = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        mcs_manager_set_int (mcs_plugin->manager, setting_name, CHANNEL, *value ? 1 : 0);
    }
}

static void
init_int_setting (McsPlugin * mcs_plugin, gchar * setting_name, int *value)
{
    McsSetting *setting;

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, setting_name, CHANNEL);
    if (setting)
    {
        *value = (setting->data.v_int);
    }
    else
    {
        mcs_manager_set_int (mcs_plugin->manager, setting_name, CHANNEL, *value);
    }
}

static void
init_string_setting (McsPlugin * mcs_plugin, gchar * setting_name, char **value)
{
    McsSetting *setting;

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, setting_name, CHANNEL);
    if ((setting) && (setting->type == MCS_TYPE_STRING))
    {
        *value = setting->data.v_string;
    }
    else
    {
        mcs_manager_set_string (mcs_plugin->manager, setting_name, CHANNEL, *value);
    }
}

static void
xfwm4_create_channel (McsPlugin * mcs_plugin)
{
    gchar *rcfile, *path;

    path = g_build_filename ("xfce4", RCDIR, RCFILE, NULL);
    rcfile = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, path);
    g_free (path);

    if (!rcfile)
        rcfile = xfce_get_userfile (OLDRCDIR, RCFILE, NULL);

    if (g_file_test (rcfile, G_FILE_TEST_EXISTS))
    {
        mcs_manager_add_channel_from_file (mcs_plugin->manager, CHANNEL, rcfile);
    }
    else
    {
        mcs_manager_add_channel (mcs_plugin->manager, CHANNEL);
    }
    g_free (rcfile);

    init_gboolean_setting (mcs_plugin, "Xfwm/BorderlessMaximize", &borderless_maximize);
    init_gboolean_setting (mcs_plugin, "Xfwm/CycleMinimum", &cycle_minimum);
    init_gboolean_setting (mcs_plugin, "Xfwm/CycleHidden", &cycle_hidden);
    init_gboolean_setting (mcs_plugin, "Xfwm/CycleWorkspaces", &cycle_workspaces);
    init_gboolean_setting (mcs_plugin, "Xfwm/FocusHint", &focus_hint);
    init_gboolean_setting (mcs_plugin, "Xfwm/ShowDockShadow", &show_dock_shadow);
    init_gboolean_setting (mcs_plugin, "Xfwm/ShowFrameShadow", &show_frame_shadow);
    init_gboolean_setting (mcs_plugin, "Xfwm/ShowPopupShadow", &show_popup_shadow);
    init_gboolean_setting (mcs_plugin, "Xfwm/PreventFocusStealing", &prevent_focus_stealing);
    init_gboolean_setting (mcs_plugin, "Xfwm/RaiseWithAnyButton", &raise_with_any_button);
    init_gboolean_setting (mcs_plugin, "Xfwm/RestoreOnMove", &restore_on_move);
    init_gboolean_setting (mcs_plugin, "Xfwm/SnapResist", &snap_resist);
    init_gboolean_setting (mcs_plugin, "Xfwm/ScrollWorkspaces", &scroll_workspaces);
    init_gboolean_setting (mcs_plugin, "Xfwm/ToggleWorkspaces", &toggle_workspaces);
    init_gboolean_setting (mcs_plugin, "Xfwm/UnredirectOverlays", &unredirect_overlays);
    init_gboolean_setting (mcs_plugin, "Xfwm/UseCompositing", &use_compositing);
    init_gboolean_setting (mcs_plugin, "Xfwm/WrapLayout", &wrap_layout);
    init_gboolean_setting (mcs_plugin, "Xfwm/WrapCycle", &wrap_cycle);

    init_int_setting (mcs_plugin, "Xfwm/PlacementRatio", &placement_ratio);
    init_int_setting (mcs_plugin, "Xfwm/InactiveOpacity", &inactive_opacity);
    init_int_setting (mcs_plugin, "Xfwm/FrameOpacity", &frame_opacity);
    init_int_setting (mcs_plugin, "Xfwm/MoveOpacity", &move_opacity);
    init_int_setting (mcs_plugin, "Xfwm/ResizeOpacity", &resize_opacity);
    init_int_setting (mcs_plugin, "Xfwm/PopupOpacity", &popup_opacity);

    init_string_setting (mcs_plugin, "Xfwm/ActivateAction", &activate_action);
    init_string_setting (mcs_plugin, "Xfwm/PlacementMode", &placement_mode);
    init_string_setting (mcs_plugin, "Xfwm/EasyClick", &easy_click);
}

static void
run_dialog (McsPlugin * mcs_plugin)
{
    const gchar *wm_name;
    static Itf *dialog = NULL;

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    wm_name = gdk_x11_screen_get_window_manager_name (gdk_screen_get_default ());
    if (g_ascii_strcasecmp (wm_name, "Xfwm4"))
    {
        xfce_err (_("These settings cannot work with your current window manager (%s)"), wm_name);
        return;
    }

    if (is_running)
    {
        if ((dialog) && (dialog->tweaks_dialog))
        {
            gtk_window_present (GTK_WINDOW (dialog->tweaks_dialog));
            gtk_window_set_focus (GTK_WINDOW (dialog->tweaks_dialog), NULL);
        }
        return;
    }

    is_running = TRUE;
    dialog = create_dialog (mcs_plugin);
    setup_dialog (dialog);
}

/* macro defined in manager-plugin.h */
MCS_PLUGIN_CHECK_INIT
