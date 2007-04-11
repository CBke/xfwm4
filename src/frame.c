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

        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2006 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <pango/pango.h>
#include <libxfce4util/libxfce4util.h>

#include "screen.h"
#include "client.h"
#include "settings.h"
#include "mywindow.h"
#include "focus.h"
#include "frame.h"
#include "compositor.h"

#ifndef ShapeInput
#define ShapeInput 2
#endif

int
frameDecorationLeft (ScreenInfo *screen_info)
{
    TRACE ("entering frameDecorationLeft");

    g_return_val_if_fail (screen_info != NULL, 0);
    return screen_info->sides[SIDE_LEFT][ACTIVE].width;
}

int
frameDecorationRight (ScreenInfo *screen_info)
{
    TRACE ("entering frameDecorationRight");

    g_return_val_if_fail (screen_info != NULL, 0);
    return screen_info->sides[SIDE_RIGHT][ACTIVE].width;
}

int
frameDecorationTop (ScreenInfo *screen_info)
{
    TRACE ("entering frameDecorationTop");

    g_return_val_if_fail (screen_info != NULL, 0);
    return screen_info->title[TITLE_3][ACTIVE].height;
}

int
frameDecorationBottom (ScreenInfo *screen_info)
{
    TRACE ("entering frameDecorationBottom");

    g_return_val_if_fail (screen_info != NULL, 0);
    return screen_info->sides[SIDE_BOTTOM][ACTIVE].height;
}

int
frameLeft (Client * c)
{
    TRACE ("entering frameLeft");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN)
        && (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
            || !(c->screen_info->params->borderless_maximize)))
    {
        return c->screen_info->sides[SIDE_LEFT][ACTIVE].width;
    }
    return 0;
}

int
frameRight (Client * c)
{
    TRACE ("entering frameRight");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN)
        && (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
            || !(c->screen_info->params->borderless_maximize)))
    {
        return c->screen_info->sides[SIDE_RIGHT][ACTIVE].width;
    }
    return 0;
}

int
frameTop (Client * c)
{
    TRACE ("entering frameTop");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        return c->screen_info->title[TITLE_3][ACTIVE].height;
    }
    return 0;
}

int
frameBottom (Client * c)
{
    TRACE ("entering frameBottom");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN)
        && (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
            || !(c->screen_info->params->borderless_maximize)))
    {
        return c->screen_info->sides[SIDE_BOTTOM][ACTIVE].height;
    }
    return 0;
}

int
frameX (Client * c)
{
    TRACE ("entering frameX");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN)
        && (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
            || !(c->screen_info->params->borderless_maximize)))
    {
        return c->x - frameLeft (c);
    }
    return c->x;
}

int
frameY (Client * c)
{
    TRACE ("entering frameY");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
         && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        return c->y - frameTop (c);
    }
    return c->y;
}

int
frameWidth (Client * c)
{
    TRACE ("entering frameWidth");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN)
        && (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
            || !(c->screen_info->params->borderless_maximize)))
    {
        return c->width + frameLeft (c) + frameRight (c);
    }
    return c->width;
}

int
frameHeight (Client * c)
{
    TRACE ("entering frameHeight");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        && FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        return frameTop (c) + frameBottom (c);
    }
    else if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
             && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        return c->height + frameTop (c) + frameBottom (c);
    }
    return c->height;
}

static int
frameTopLeftWidth (Client * c, int state)
{
    TRACE ("entering frameTopLeftWidth");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
        && c->screen_info->params->borderless_maximize)
    {
        return 0;
    }
    return c->screen_info->corners[CORNER_TOP_LEFT][state].width;

}

static int
frameTopRightWidth (Client * c, int state)
{
    TRACE ("entering frameTopRightWidth");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
        && c->screen_info->params->borderless_maximize)
    {
        return 0;
    }
    return c->screen_info->corners[CORNER_TOP_RIGHT][state].width;
}

static int
frameButtonOffset (Client *c)
{
    TRACE ("entering frameButtonOffset");

    g_return_val_if_fail (c != NULL, 0);
    if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
        && c->screen_info->params->borderless_maximize)
    {
        return MAX (0, c->screen_info->params->maximized_offset);
    }
    return c->screen_info->params->button_offset;
}

static void
frameCreateTitlePixmap (Client * c, int state, int left, int right, xfwmPixmap * pm)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    GdkPixmap *gpixmap;
    GdkGCValues values;
    GdkGC *gc;
    PangoLayout *layout;
    PangoRectangle logical_rect;
    int width, x, hoffset, w1, w2, w3, w4, w5, temp;
    int voffset, title_x, title_y;

    TRACE ("entering frameCreateTitlePixmap");

    g_return_if_fail (c);
    g_return_if_fail (pm);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (left > right)
    {
        temp = left;
        left = right;
        right = temp;
    }

    width = frameWidth (c) - frameTopLeftWidth (c, state) - frameTopRightWidth (c, state);
    if (width < 1)
    {
        pm->pixmap = None;
        pm->mask = None;
        pm->width = 0;
        pm->height = 0;
        return;
    }

    if (left < frameTopLeftWidth (c, state))
    {
        left = frameTopLeftWidth (c, state);
    }
    if (right > frameWidth (c) - frameTopRightWidth (c, state))
    {
        right = frameWidth (c) - frameTopRightWidth (c, state);
    }
    if (right < frameTopLeftWidth (c, state))
    {
        right = frameTopLeftWidth (c, state);
    }

    left = left - frameTopLeftWidth (c, state);
    right = right - frameTopLeftWidth (c, state);

    x = 0;
    hoffset = 0;
    voffset = 0;

    w1 = 0;
    w2 = screen_info->title[TITLE_2][state].width;
    w4 = screen_info->title[TITLE_4][state].width;

    layout = gtk_widget_create_pango_layout (myScreenGetGtkWidget (screen_info), c->name);
    pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

    if (screen_info->params->full_width_title)
    {
        w1 = left;
        w5 = width - right;
        w3 = width - w1 - w2 - w4 - w5;
        if (w3 < 0)
        {
            w3 = 0;
        }
        switch (screen_info->params->title_alignment)
        {
            case ALIGN_LEFT:
                hoffset = screen_info->params->title_horizontal_offset;
                break;
            case ALIGN_RIGHT:
                hoffset = w3 - logical_rect.width - screen_info->params->title_horizontal_offset;
                break;
            case ALIGN_CENTER:
                hoffset = (w3 / 2) - (logical_rect.width / 2);
                break;
        }
        if (hoffset < screen_info->params->title_horizontal_offset)
        {
            hoffset = screen_info->params->title_horizontal_offset;
        }
    }
    else
    {
        w3 = logical_rect.width;
        w5 = width;
        if (w3 > width - w2 - w4)
        {
            w3 = width - w2 - w4;
        }
        if (w3 < 0)
        {
            w3 = 0;
        }
        switch (screen_info->params->title_alignment)
        {
            case ALIGN_LEFT:
                w1 = left + screen_info->params->title_horizontal_offset;
                break;
            case ALIGN_RIGHT:
                w1 = right - w2 - w3 - w4 - screen_info->params->title_horizontal_offset;
                break;
            case ALIGN_CENTER:
                w1 = left + ((right - left) / 2) - (w3 / 2) - w2;
                break;
        }
        if (w1 < left)
        {
            w1 = left;
        }
    }

    xfwmPixmapCreate (screen_info, pm, width, frameTop (c));
    gpixmap = gdk_pixmap_foreign_new (pm->pixmap);
    gdk_drawable_set_colormap (gpixmap, gdk_screen_get_rgb_colormap (screen_info->gscr));
    gc = gdk_gc_new (gpixmap);

    if (w1 > 0)
    {
        xfwmPixmapFill (&screen_info->title[TITLE_1][state], pm, 0, 0, w1, frameTop (c));
        x = x + w1;
    }

    xfwmPixmapFill (&screen_info->title[TITLE_2][state], pm, x, 0, w2, frameTop (c));
    x = x + w2;

    if (w3 > 0)
    {
        if (state == ACTIVE)
        {
            voffset = screen_info->params->title_vertical_offset_active;
        }
        else
        {
            voffset = screen_info->params->title_vertical_offset_inactive;
        }
        xfwmPixmapFill (&screen_info->title[TITLE_3][state], pm, x, 0, w3, frameTop (c));
        title_x = hoffset + x;
        title_y = voffset + (frameTop (c) - logical_rect.height) / 2;
        if (title_y + logical_rect.height > frameTop (c))
        {
            title_y = MAX (0, frameTop (c) - logical_rect.height);
        }
        if (screen_info->params->title_shadow[state])
        {
            gdk_gc_get_values (screen_info->title_shadow_colors[state].gc, &values);
            gdk_gc_set_values (gc, &values, GDK_GC_FOREGROUND);
            if (screen_info->params->title_shadow[state] == TITLE_SHADOW_UNDER)
            {
                gdk_draw_layout (gpixmap, gc, title_x + 1, title_y + 1, layout);
            }
            else
            {
                gdk_draw_layout (gpixmap, gc, title_x - 1, title_y, layout);
                gdk_draw_layout (gpixmap, gc, title_x, title_y - 1, layout);
                gdk_draw_layout (gpixmap, gc, title_x + 1, title_y, layout);
                gdk_draw_layout (gpixmap, gc, title_x, title_y + 1, layout);
            }
        }
        gdk_gc_get_values (screen_info->title_colors[state].gc, &values);
        gdk_gc_set_values (gc, &values, GDK_GC_FOREGROUND);
        gdk_draw_layout (gpixmap, gc, title_x, title_y, layout);
        x = x + w3;
    }

    if (x > right - w4)
    {
        x = right - w4;
    }
    xfwmPixmapFill (&screen_info->title[TITLE_4][state], pm, x, 0, w4, frameTop (c));
    x = x + w4;

    if (w5 > 0)
    {
        xfwmPixmapFill (&screen_info->title[TITLE_5][state], pm, x, 0, w5, frameTop (c));
    }
    g_object_unref (G_OBJECT (gc));
    g_object_unref (G_OBJECT (gpixmap));
    g_object_unref (G_OBJECT (layout));
}

static int
getButtonFromLetter (char chr, Client * c)
{
    int b;

    TRACE ("entering getButtonFromLetter");

    b = -1;
    switch (chr)
    {
        case 'H':
            if (CLIENT_CAN_HIDE_WINDOW (c))
            {
                b = HIDE_BUTTON;
            }
            break;
        case 'C':
            if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_CLOSE))
            {
                b = CLOSE_BUTTON;
            }
            break;
        case 'M':
            if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
            {
                b = MAXIMIZE_BUTTON;
            }
            break;
        case 'S':
            b = SHADE_BUTTON;
            break;
        case 'T':
            if (FLAG_TEST_ALL (c->xfwm_flags, XFWM_FLAG_HAS_STICK | XFWM_FLAG_HAS_MENU))
            {
                b = STICK_BUTTON;
            }
            break;
        case 'O':
            if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MENU))
            {
                b = MENU_BUTTON;
            }
            break;
        case '|':
            b = TITLE_SEPARATOR;
            break;
        default:
            b = -1;
    }
    return b;
}

static char
getLetterFromButton (int i, Client * c)
{
    char chr;

    TRACE ("entering getLetterFromButton");

    chr = 0;
    switch (i)
    {
        case HIDE_BUTTON:
            if (CLIENT_CAN_HIDE_WINDOW (c))
            {
                chr = 'H';
            }
            break;
        case CLOSE_BUTTON:
            if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_CLOSE))
            {
                chr = 'C';
            }
            break;
        case MAXIMIZE_BUTTON:
            if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
            {
                chr = 'M';
            }
            break;
        case SHADE_BUTTON:
            chr = 'S';
            break;
        case STICK_BUTTON:
            if (FLAG_TEST_ALL (c->xfwm_flags, XFWM_FLAG_HAS_STICK | XFWM_FLAG_HAS_MENU))
            {
                chr = 'T';
            }
            break;
        case MENU_BUTTON:
            if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MENU))
            {
                chr = 'O';
            }
            break;
        default:
            chr = 0;
    }
    return chr;
}

static xfwmPixmap *
frameGetPixmap (Client * c, int button, int state)
{
    ScreenInfo *screen_info;

    screen_info = c->screen_info;
    switch (button)
    {
        case MENU_BUTTON:
            if (screen_info->params->show_app_icon)
            {
                return &c->appmenu[state];
            }
            break;
        case SHADE_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && screen_info->buttons[SHADE_BUTTON][state + 3].pixmap)
            {
                return &screen_info->buttons[SHADE_BUTTON][state + 3];
            }
            return &screen_info->buttons[SHADE_BUTTON][state];
            break;
        case STICK_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY)
                && screen_info->buttons[STICK_BUTTON][state + 3].pixmap)
            {
                return &screen_info->buttons[STICK_BUTTON][state + 3];
            }
            return &screen_info->buttons[STICK_BUTTON][state];
            break;
        case MAXIMIZE_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED)
                && screen_info->buttons[MAXIMIZE_BUTTON][state + 3].pixmap)
            {
                return &screen_info->buttons[MAXIMIZE_BUTTON][state + 3];
            }
            return &screen_info->buttons[MAXIMIZE_BUTTON][state];
            break;
        default:
            break;
    }
    return &screen_info->buttons[button][state];
}


static void
frameSetShape (Client * c, int state, FramePixmap * frame_pix, int button_x[BUTTON_COUNT])
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Window shape_win;
    XRectangle rect;
    xfwmPixmap *my_pixmap;
    int i;

    TRACE ("entering frameSetShape");
    TRACE ("setting shape for client (0x%lx)", c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (!display_info->have_shape)
    {
        return;
    }

    shape_win = XCreateSimpleWindow (display_info->dpy, c->frame, 0, 0, frameWidth (c), frameHeight (c), 0, 0, 0);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        rect.x = 0;
        rect.y = 0;
        rect.width  = frameWidth (c);
        rect.height = frameHeight (c);
        XShapeCombineRectangles (display_info->dpy, shape_win, ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, Unsorted);
    }
    else if (!FLAG_TEST (c->flags, CLIENT_FLAG_HAS_SHAPE))
    {
        rect.x = frameLeft (c);
        rect.y = frameTop (c);
        rect.width  = c->width;
        rect.height = c->height;
        XShapeCombineRectangles (display_info->dpy, shape_win, ShapeBounding, 0, 0, &rect, 1, ShapeSet, Unsorted);
    }
    else
    {
        XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding, frameLeft (c),
                            frameTop (c), c->window, ShapeBounding, ShapeSet);
    }
    if (frame_pix)
    {
        XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->title), ShapeBounding,
                           0, 0, frame_pix->pm_title.mask, ShapeSet);
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            if (xfwmWindowVisible (&c->sides[SIDE_LEFT]))
            {
                XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->sides[SIDE_LEFT]),
                                   ShapeBounding, 0, 0, frame_pix->pm_sides[SIDE_LEFT].mask, ShapeSet);
            }

            if (xfwmWindowVisible (&c->sides[SIDE_RIGHT]))
            {
                XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->sides[SIDE_RIGHT]),
                                   ShapeBounding, 0, 0, frame_pix->pm_sides[SIDE_RIGHT].mask, ShapeSet);
            }
        }

        if (xfwmWindowVisible (&c->sides[SIDE_BOTTOM]))
        {
            XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->sides[SIDE_BOTTOM]),
                               ShapeBounding, 0, 0, frame_pix->pm_sides[SIDE_BOTTOM].mask, ShapeSet);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_BOTTOM_LEFT]))
        {
            XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_LEFT]),
                               ShapeBounding, 0, 0, screen_info->corners[CORNER_BOTTOM_LEFT][state].mask, ShapeSet);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_BOTTOM_RIGHT]))
        {
            XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_RIGHT]),
                               ShapeBounding, 0, 0, screen_info->corners[CORNER_BOTTOM_RIGHT][state].mask, ShapeSet);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_TOP_LEFT]))
        {
            XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->corners[CORNER_TOP_LEFT]),
                               ShapeBounding, 0, 0, screen_info->corners[CORNER_TOP_LEFT][state].mask, ShapeSet);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_TOP_RIGHT]))
        {
            XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->corners[CORNER_TOP_RIGHT]),
                               ShapeBounding, 0, 0, screen_info->corners[CORNER_TOP_RIGHT][state].mask, ShapeSet);
        }

        for (i = 0; i < BUTTON_LAST; i++)
        {
            if (xfwmWindowVisible (&c->buttons[i]))
            {
                my_pixmap = frameGetPixmap (c, i, c->button_pressed[i] ? PRESSED : state);
                XShapeCombineMask (display_info->dpy, MYWINDOW_XWINDOW (c->buttons[i]),
                                   ShapeBounding, 0, 0, my_pixmap->mask, ShapeSet);
            }
        }

        if (xfwmWindowVisible (&c->corners[CORNER_TOP_LEFT]) &&
            (screen_info->corners[CORNER_TOP_LEFT][state].height > frameHeight (c) - frameBottom (c) + 1))
        {
            rect.x      = 0;
            rect.y      = frameHeight (c) - frameBottom (c) + 1;
            rect.width  = frameTopLeftWidth (c, state);
            rect.height = screen_info->corners[CORNER_TOP_LEFT][state].height
                           - (frameHeight (c) - frameBottom (c) + 1);
            XShapeCombineRectangles (display_info->dpy, MYWINDOW_XWINDOW (c->corners[CORNER_TOP_LEFT]),
                                     ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_TOP_RIGHT]) &&
            (screen_info->corners[CORNER_TOP_RIGHT][state].height > frameHeight (c) - frameBottom (c) + 1))
        {
            rect.x      = 0;
            rect.y      = frameHeight (c) - frameBottom (c) + 1;
            rect.width  = frameTopRightWidth (c, state);
            rect.height = screen_info->corners[CORNER_TOP_RIGHT][state].height
                           - (frameHeight (c) - frameBottom (c) + 1);
            XShapeCombineRectangles (display_info->dpy, MYWINDOW_XWINDOW (c->corners[CORNER_TOP_RIGHT]),
                                     ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_BOTTOM_LEFT]) &&
            (screen_info->corners[CORNER_BOTTOM_LEFT][state].height > frameHeight (c) - frameTop (c) + 1))
        {
            rect.x      = 0;
            rect.y      = 0;
            rect.width  = screen_info->corners[CORNER_BOTTOM_LEFT][state].width;
            rect.height = screen_info->corners[CORNER_BOTTOM_LEFT][state].height
                           - (frameHeight (c) - frameTop (c) + 1);
            XShapeCombineRectangles (display_info->dpy, MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_LEFT]),
                                     ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_BOTTOM_RIGHT]) &&
            (screen_info->corners[CORNER_BOTTOM_RIGHT][state].height > frameHeight (c) - frameTop (c) + 1))
        {
            rect.x      = 0;
            rect.y      = 0;
            rect.width  = screen_info->corners[CORNER_BOTTOM_RIGHT][state].width;
            rect.height = screen_info->corners[CORNER_BOTTOM_RIGHT][state].height
                           - (frameHeight (c) - frameTop (c) + 1);
            XShapeCombineRectangles (display_info->dpy, MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_RIGHT]),
                                     ShapeBounding, 0, 0, &rect, 1, ShapeSubtract, 0);
        }

        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            if (xfwmWindowVisible (&c->sides[SIDE_LEFT]))
            {
                XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding, 0, frameTop (c),
                                    MYWINDOW_XWINDOW (c->sides[SIDE_LEFT]), ShapeBounding, ShapeUnion);
            }

            if (xfwmWindowVisible (&c->sides[SIDE_RIGHT]))
            {
                XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding, frameWidth (c) - frameRight (c), frameTop (c),
                                    MYWINDOW_XWINDOW (c->sides[SIDE_RIGHT]), ShapeBounding, ShapeUnion);
            }
        }

        if (xfwmWindowVisible (&c->title))
        {
            XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding,
                                frameTopLeftWidth (c, state), 0,
                                MYWINDOW_XWINDOW (c->title), ShapeBounding, ShapeUnion);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_TOP_LEFT]))
        {

            XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding, 0, 0,
                                MYWINDOW_XWINDOW (c->corners[CORNER_TOP_LEFT]), ShapeBounding, ShapeUnion);
        }

        if (xfwmWindowVisible (&c->sides[SIDE_BOTTOM]))
        {
            XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding,
                                screen_info->corners[CORNER_BOTTOM_LEFT][state].width,
                                frameHeight (c) - frameBottom (c),
                                MYWINDOW_XWINDOW (c->sides[SIDE_BOTTOM]), ShapeBounding, ShapeUnion);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_BOTTOM_LEFT]))
        {
            XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding, 0,
                                frameHeight (c) - screen_info->corners[CORNER_BOTTOM_LEFT][state].height,
                                MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_LEFT]), ShapeBounding, ShapeUnion);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_BOTTOM_RIGHT]))
        {
            XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding,
                                frameWidth (c) - screen_info->corners[CORNER_BOTTOM_RIGHT][state].width,
                                frameHeight (c) - screen_info->corners[CORNER_BOTTOM_RIGHT][state].height,
                                MYWINDOW_XWINDOW (c->corners[CORNER_BOTTOM_RIGHT]), ShapeBounding, ShapeUnion);
        }

        if (xfwmWindowVisible (&c->corners[CORNER_TOP_RIGHT]))
        {
            XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding,
                                frameWidth (c) - frameTopRightWidth (c, state),
                                0, MYWINDOW_XWINDOW (c->corners[CORNER_TOP_RIGHT]), ShapeBounding, ShapeUnion);
        }

        for (i = 0; i < BUTTON_LAST; i++)
        {
            if (xfwmWindowVisible (&c->buttons[i]))
            {
                XShapeCombineShape (display_info->dpy, shape_win, ShapeBounding, button_x[i],
                                    (frameTop (c) - screen_info->buttons[i][state].height + 1) / 2,
                                    MYWINDOW_XWINDOW (c->buttons[i]), ShapeBounding, ShapeUnion);
            }
        }
    }
    rect.x = 0;
    rect.y = 0;
    rect.width  = frameWidth (c);
    rect.height = frameHeight (c);
    XShapeCombineRectangles (display_info->dpy, shape_win, ShapeBounding, 0, 0, &rect, 1, ShapeIntersect, Unsorted);
    XShapeCombineShape (display_info->dpy, c->frame, ShapeBounding, 0, 0, shape_win, ShapeBounding, ShapeSet);

    /* Set Input shape when using XShape extension 1.1 and later */
    if (display_info->shape_version >= 1001)
    {
        XShapeCombineShape (display_info->dpy, shape_win, ShapeInput, frameLeft (c), frameTop (c), c->window, ShapeBounding, ShapeSubtract);
        XShapeCombineShape (display_info->dpy, shape_win, ShapeInput, frameLeft (c), frameTop (c), c->window, ShapeInput, ShapeUnion);
        XShapeCombineShape (display_info->dpy, c->frame, ShapeInput, 0, 0, shape_win, ShapeInput, ShapeSet);
    }
    XDestroyWindow (display_info->dpy, shape_win);
}

void
frameDraw (Client * c, gboolean clear_all)
{
    ScreenInfo *screen_info;
    FramePixmap frame_pix;
    xfwmPixmap *my_pixmap;
    int state, i, j, x, button, left, right;
    int top_width, bottom_width, left_height, right_height;
    int button_x[BUTTON_COUNT];
    gboolean requires_clearing;
    gboolean width_changed;
    gboolean height_changed;

    TRACE ("entering frameDraw");
    TRACE ("drawing frame for \"%s\" (0x%lx)", c->name, c->window);

    g_return_if_fail (c != NULL);

    screen_info = c->screen_info;
    requires_clearing = FALSE;
    width_changed = FALSE;
    height_changed = FALSE;
    state = ACTIVE;

    if (c != clientGetFocus ())
    {
        TRACE ("\"%s\" is not the active window", c->name);
        if (FLAG_TEST (c->wm_flags, WM_FLAG_URGENT))
        {
            if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_SEEN_ACTIVE))
            {
                state = ACTIVE;
            }
            else
            {
                state = INACTIVE;
            }
        }
        else
        {
            state = INACTIVE;
        }
    }

    if ((state == INACTIVE)
        && FLAG_TEST(c->xfwm_flags, XFWM_FLAG_DRAW_ACTIVE | XFWM_FLAG_FIRST_MAP))
    {
        requires_clearing = TRUE;
        FLAG_UNSET (c->xfwm_flags,  XFWM_FLAG_DRAW_ACTIVE);
    }
    else if ((state == ACTIVE)
             && (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_DRAW_ACTIVE)
                 || FLAG_TEST (c->xfwm_flags, XFWM_FLAG_FIRST_MAP)))
    {
        requires_clearing = TRUE;
        FLAG_SET (c->xfwm_flags,  XFWM_FLAG_DRAW_ACTIVE);
    }
    /* Flag clearance */
    FLAG_UNSET (c->xfwm_flags,  XFWM_FLAG_FIRST_MAP);

    /* Cache mgmt */
    if (clear_all)
    {
        width_changed = TRUE;
        height_changed = TRUE;
        requires_clearing = TRUE;
        frameClearQueueDraw (c);
        if (c->frame_timeout_id)
        {
            g_source_remove (c->frame_timeout_id);
            c->frame_timeout_id = 0;
        }

    }
    else
    {
        if (c->previous_width != c->width)
        {
            width_changed = TRUE;
            c->previous_width = c->width;
        }
        if (c->previous_height != c->height)
        {
            height_changed = TRUE;
            c->previous_height = c->height;
        }
    }

    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        /* First, hide the buttons that we don't have... */
        for (i = 0; i < BUTTON_LAST; i++)
        {
            char b = getLetterFromButton (i, c);
            if ((!b) || !strchr (screen_info->params->button_layout, b))
            {
                xfwmWindowHide (&c->buttons[i]);
            }
        }

        /* Then, show the ones that we do have on left... */
        x = frameLeft (c) + frameButtonOffset (c);
        if (x < 0)
        {
            x = 0;
        }
        right = frameWidth (c) - frameRight (c) - frameButtonOffset (c);
        for (i = 0; i < strlen (screen_info->params->button_layout); i++)
        {
            button = getButtonFromLetter (screen_info->params->button_layout[i], c);
            if (button == TITLE_SEPARATOR)
            {
                break;
            }
            else if (button >= 0)
            {
                if (x + screen_info->buttons[button][state].width + screen_info->params->button_spacing < right)
                {
                    my_pixmap = frameGetPixmap (c, button, c->button_pressed[button] ? PRESSED : state);
                    if (my_pixmap->pixmap)
                    {
                        xfwmWindowSetBG (&c->buttons[button], my_pixmap);
                    }
                    xfwmWindowShow (&c->buttons[button], x,
                        (frameTop (c) - screen_info->buttons[button][state].height + 1) / 2,
                        screen_info->buttons[button][state].width,
                        screen_info->buttons[button][state].height, TRUE);
                    button_x[button] = x;
                    x = x + screen_info->buttons[button][state].width +
                        screen_info->params->button_spacing;
                }
                else
                {
                    xfwmWindowHide (&c->buttons[button]);
                }
            }
        }
        left = x + screen_info->params->button_spacing;

        /* and those that we do have on right... */
        x = frameWidth (c) - frameRight (c) + screen_info->params->button_spacing -
            frameButtonOffset (c);
        for (j = strlen (screen_info->params->button_layout) - 1; j >= i; j--)
        {
            button = getButtonFromLetter (screen_info->params->button_layout[j], c);
            if (button == TITLE_SEPARATOR)
            {
                break;
            }
            else if (button >= 0)
            {
                if (x - screen_info->buttons[button][state].width - screen_info->params->button_spacing > left)
                {
                    my_pixmap = frameGetPixmap (c, button, c->button_pressed[button] ? PRESSED : state);
                    if (my_pixmap->pixmap)
                    {
                        xfwmWindowSetBG (&c->buttons[button], my_pixmap);
                    }
                    x = x - screen_info->buttons[button][state].width -
                        screen_info->params->button_spacing;
                    xfwmWindowShow (&c->buttons[button], x,
                        (frameTop (c) - screen_info->buttons[button][state].height + 1) / 2,
                        screen_info->buttons[button][state].width,
                        screen_info->buttons[button][state].height, TRUE);
                    button_x[button] = x;
                }
                else
                {
                    xfwmWindowHide (&c->buttons[button]);
                }
            }
        }
        left = left - 2 * screen_info->params->button_spacing;
        right = x;

        top_width = frameWidth (c) - frameTopLeftWidth (c, state) - frameTopRightWidth (c, state);
        bottom_width = frameWidth (c) -
            screen_info->corners[CORNER_BOTTOM_LEFT][state].width -
            screen_info->corners[CORNER_BOTTOM_RIGHT][state].width;
        left_height = frameHeight (c) - frameTop (c) -
            screen_info->corners[CORNER_BOTTOM_LEFT][state].height;
        right_height = frameHeight (c) - frameTop (c) -
            screen_info->corners[CORNER_BOTTOM_RIGHT][state].height;

        xfwmPixmapInit (screen_info, &frame_pix.pm_title);
        xfwmPixmapInit (screen_info, &frame_pix.pm_sides[SIDE_LEFT]);
        xfwmPixmapInit (screen_info, &frame_pix.pm_sides[SIDE_RIGHT]);
        xfwmPixmapInit (screen_info, &frame_pix.pm_sides[SIDE_BOTTOM]);

        /* The title is always visible */
        frameCreateTitlePixmap (c, state, left, right, &frame_pix.pm_title);
        xfwmWindowSetBG (&c->title, &frame_pix.pm_title);
        xfwmWindowShow (&c->title,
            frameTopLeftWidth (c, state), 0, top_width,
            frameTop (c), (requires_clearing | width_changed));

        /* Corners are never resized, we need to update them separately */
        if (requires_clearing)
        {
            xfwmWindowSetBG (&c->corners[CORNER_TOP_LEFT],
                &screen_info->corners[CORNER_TOP_LEFT][state]);
            xfwmWindowSetBG (&c->corners[CORNER_TOP_RIGHT],
                &screen_info->corners[CORNER_TOP_RIGHT][state]);
            xfwmWindowSetBG (&c->corners[CORNER_BOTTOM_LEFT],
                &screen_info->corners[CORNER_BOTTOM_LEFT][state]);
            xfwmWindowSetBG (&c->corners[CORNER_BOTTOM_RIGHT],
                &screen_info->corners[CORNER_BOTTOM_RIGHT][state]);
        }

        if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
            && (c->screen_info->params->borderless_maximize))
        {
            xfwmWindowHide (&c->sides[SIDE_LEFT]);
            xfwmWindowHide (&c->sides[SIDE_RIGHT]);
            xfwmWindowHide (&c->sides[SIDE_BOTTOM]);
            xfwmWindowHide (&c->corners[CORNER_TOP_LEFT]);
            xfwmWindowHide (&c->corners[CORNER_TOP_RIGHT]);
            xfwmWindowHide (&c->corners[CORNER_BOTTOM_LEFT]);
            xfwmWindowHide (&c->corners[CORNER_BOTTOM_RIGHT]);
        }
        else
        {
            if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
            {
                xfwmWindowHide (&c->sides[SIDE_LEFT]);
                xfwmWindowHide (&c->sides[SIDE_RIGHT]);
            }
            else
            {
                xfwmPixmapCreate (screen_info, &frame_pix.pm_sides[SIDE_LEFT],
                    frameLeft (c), left_height);
                xfwmPixmapFill (&screen_info->sides[SIDE_LEFT][state],
                    &frame_pix.pm_sides[SIDE_LEFT],
                    0, 0, frameLeft (c), left_height);
                xfwmWindowSetBG (&c->sides[SIDE_LEFT],
                    &frame_pix.pm_sides[SIDE_LEFT]);
                xfwmWindowShow (&c->sides[SIDE_LEFT], 0, frameTop (c),
                    frameLeft (c), left_height, (requires_clearing | height_changed));

                xfwmPixmapCreate (screen_info, &frame_pix.pm_sides[SIDE_RIGHT],
                    frameRight (c), right_height);
                xfwmPixmapFill (&screen_info->sides[SIDE_RIGHT][state],
                    &frame_pix.pm_sides[SIDE_RIGHT],
                    0, 0, frameRight (c), right_height);
                xfwmWindowSetBG (&c->sides[SIDE_RIGHT],
                    &frame_pix.pm_sides[SIDE_RIGHT]);
                xfwmWindowShow (&c->sides[SIDE_RIGHT],
                    frameWidth (c) - frameRight (c), frameTop (c), frameRight (c),
                    right_height, (requires_clearing | height_changed));
            }

            xfwmPixmapCreate (screen_info, &frame_pix.pm_sides[SIDE_BOTTOM],
                bottom_width, frameBottom (c));
            xfwmPixmapFill (&screen_info->sides[SIDE_BOTTOM][state],
                &frame_pix.pm_sides[SIDE_BOTTOM],
                0, 0, bottom_width, frameBottom (c));
            xfwmWindowSetBG (&c->sides[SIDE_BOTTOM],
                &frame_pix.pm_sides[SIDE_BOTTOM]);
            xfwmWindowShow (&c->sides[SIDE_BOTTOM],
                screen_info->corners[CORNER_BOTTOM_LEFT][state].width,
                frameHeight (c) - frameBottom (c), bottom_width, frameBottom (c),
                (requires_clearing | width_changed));

            xfwmWindowShow (&c->corners[CORNER_TOP_LEFT], 0, 0,
                frameTopLeftWidth (c, state),
                screen_info->corners[CORNER_TOP_LEFT][state].height,
                requires_clearing);

            xfwmWindowShow (&c->corners[CORNER_TOP_RIGHT],
                frameWidth (c) - frameTopRightWidth (c, state),
                0, frameTopRightWidth (c, state),
                screen_info->corners[CORNER_TOP_RIGHT][state].height,
                requires_clearing);

            xfwmWindowShow (&c->corners[CORNER_BOTTOM_LEFT], 0,
                frameHeight (c) -
                screen_info->corners[CORNER_BOTTOM_LEFT][state].height,
                screen_info->corners[CORNER_BOTTOM_LEFT][state].width,
                screen_info->corners[CORNER_BOTTOM_LEFT][state].height,
                requires_clearing);

            xfwmWindowShow (&c->corners[CORNER_BOTTOM_RIGHT],
                frameWidth (c) -
                screen_info->corners[CORNER_BOTTOM_RIGHT][state].width,
                frameHeight (c) -
                screen_info->corners[CORNER_BOTTOM_RIGHT][state].height,
                screen_info->corners[CORNER_BOTTOM_RIGHT][state].width,
                screen_info->corners[CORNER_BOTTOM_RIGHT][state].height,
                requires_clearing);
        }
        frameSetShape (c, state, &frame_pix, button_x);
        xfwmPixmapFree (&frame_pix.pm_title);
        xfwmPixmapFree (&frame_pix.pm_sides[SIDE_BOTTOM]);
        xfwmPixmapFree (&frame_pix.pm_sides[SIDE_LEFT]);
        xfwmPixmapFree (&frame_pix.pm_sides[SIDE_RIGHT]);
    }
    else
    {
        if (xfwmWindowVisible (&c->title))
        {
            xfwmWindowHide (&c->title);
        }
        for (i = 0; i < 3; i++)
        {
            if (MYWINDOW_XWINDOW (c->sides[i]) && xfwmWindowVisible (&c->sides[i]))
            {
                xfwmWindowHide (&c->sides[i]);
            }
        }
        for (i = 0; i < 4; i++)
        {
            if (MYWINDOW_XWINDOW (c->corners[i]) && xfwmWindowVisible (&c->corners[i]))
            {
                xfwmWindowHide (&c->corners[i]);
            }
        }
        for (i = 0; i < BUTTON_LAST; i++)
        {
            if (MYWINDOW_XWINDOW (c->buttons[i]) && xfwmWindowVisible (&c->buttons[i]))
            {
                xfwmWindowHide (&c->buttons[i]);
            }
        }
        frameSetShape (c, 0, NULL, 0);
    }
}

static gboolean
update_frame_idle_cb (gpointer data)
{
    Client *c;
    
    TRACE ("entering update_frame_idle_cb");

    c = (Client *) data;
    g_return_val_if_fail (c, FALSE);

    frameDraw (c, TRUE);
    c->frame_timeout_id = 0;

    return FALSE;
}

void
frameClearQueueDraw (Client * c)
{
    g_return_if_fail (c);

    TRACE ("entering frameClearQueueDraw for \"%s\" (0x%lx)", c->name, c->window);

    if (c->frame_timeout_id)
    {
        g_source_remove (c->frame_timeout_id);
        c->frame_timeout_id = 0;
    }
}

void
frameQueueDraw (Client * c)
{
    g_return_if_fail (c);

    TRACE ("entering frameQueueDraw for \"%s\" (0x%lx)", c->name, c->window);

    if (c->frame_timeout_id == 0)
    {
        c->frame_timeout_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, 
                                              update_frame_idle_cb, c, NULL);
    }
}

