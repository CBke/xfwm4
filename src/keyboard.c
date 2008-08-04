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
        xfwm4    - (c) 2002-2008 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libxfce4util/libxfce4util.h>
#include "keyboard.h"

unsigned int AltMask;
unsigned int MetaMask;
unsigned int NumLockMask;
unsigned int ScrollLockMask;
unsigned int SuperMask;
unsigned int HyperMask;

static gboolean
getKeycode (Display *dpy, const char *str, KeyCode *keycode)
{
    unsigned int value;
    KeySym keysym;

    keysym = XStringToKeysym (str);
    if (keysym == NoSymbol)
    {
        if (sscanf (str, "0x%X", (unsigned int *) &value) != 1)
        {
            *keycode = 0;
            return FALSE;
        }
        *keycode = (KeyCode) value;
    }
    else
    {
        *keycode = XKeysymToKeycode (dpy, keysym);
    }
    return TRUE;
}

int
getModifierMap (const char *str)
{
    gchar *tmp;
    int map;

    tmp = g_ascii_strdown ((gchar *) str, strlen (str));
    map = 0;

    if (strstr (tmp, "shift"))
    {
        map |= ShiftMask;
    }
    if (strstr (tmp, "control"))
    {
        map |= ControlMask;
    }
    if (strstr (tmp, "alt"))
    {
        map |= AltMask;
    }
    if (strstr (tmp, "meta"))
    {
        map |= MetaMask;
    }
    if (strstr (tmp, "super"))
    {
        map |= SuperMask;
    }
    if (strstr (tmp, "hyper"))
    {
        map |= HyperMask;
    }
    if (strstr (tmp, "mod1"))
    {
        map |= Mod1Mask;
    }
    if (strstr (tmp, "mod2"))
    {
        map |= Mod2Mask;
    }
    if (strstr (tmp, "mod3"))
    {
        map |= Mod3Mask;
    }
    if (strstr (tmp, "mod4"))
    {
        map |= Mod4Mask;
    }
    if (strstr (tmp, "mod5"))
    {
        map |= Mod5Mask;
    }
    g_free (tmp);

    return map;
}

void
parseKeyString (Display * dpy, MyKey * key, const char *str)
{
    char *k;

    g_return_if_fail (key != NULL);

    TRACE ("entering parseKeyString");
    TRACE ("key string=%s", str);

    key->keycode = 0;
    key->modifier = 0;

    g_return_if_fail (str != NULL);

    if (!g_ascii_strcasecmp (str, "none"))
    {
        return;
    }

    k = strrchr (str, '+');
    if (k)
    {
        /* There is a modifier */
        getKeycode (dpy, ++k, &key->keycode);
        key->modifier = getModifierMap (str);
    }
    else
    {
        getKeycode (dpy, str, &key->keycode);
        key->modifier = 0;
    }
}

gboolean
grabKey (Display * dpy, MyKey * key, Window w)
{
    gboolean status;

    TRACE ("entering grabKey");

    status = GrabSuccess;
    if (key->keycode)
    {
        if (key->modifier == 0)
        {
            status |=
                XGrabKey (dpy, key->keycode, AnyModifier, w,
                                        TRUE, GrabModeAsync, GrabModeSync);
        }
        else
        {
            /* Here we grab all combinations of well known modifiers */
            status |=
                XGrabKey (dpy, key->keycode,
                                        key->modifier, w,
                                        TRUE, GrabModeAsync, GrabModeSync);
            status |=
                XGrabKey (dpy, key->keycode,
                                        key->modifier | ScrollLockMask, w,
                                        TRUE, GrabModeAsync, GrabModeSync);
            status |=
                XGrabKey (dpy, key->keycode,
                                        key->modifier | NumLockMask, w,
                                        TRUE, GrabModeAsync, GrabModeSync);
            status |=
                XGrabKey (dpy, key->keycode,
                                        key->modifier | LockMask, w,
                                        TRUE, GrabModeAsync, GrabModeSync);
            status |=
                XGrabKey (dpy, key->keycode,
                                        key->modifier | ScrollLockMask | NumLockMask, w,
                                        TRUE, GrabModeAsync, GrabModeSync);
            status |=
                XGrabKey (dpy, key->keycode,
                                        key->modifier | ScrollLockMask | LockMask, w,
                                        TRUE, GrabModeAsync, GrabModeSync);
            status |=
                XGrabKey (dpy, key->keycode,
                                        key->modifier | LockMask | NumLockMask, w,
                                        TRUE, GrabModeAsync, GrabModeSync);
            status |=
                XGrabKey (dpy, key->keycode,
                                        key->modifier | ScrollLockMask | LockMask | NumLockMask, w,
                                        TRUE, GrabModeAsync, GrabModeSync);
        }
    }

    return (status == GrabSuccess);
}

void
ungrabKeys (Display * dpy, Window w)
{
    TRACE ("entering ungrabKeys");

    XUngrabKey (dpy, AnyKey, AnyModifier, w);
}

gboolean
grabButton (Display * dpy, int button, int modifier, Window w)
{
    gboolean status;

    TRACE ("entering grabButton");

    status = GrabSuccess;
    if (modifier == AnyModifier)
    {
        status |=
            XGrabButton (dpy, button, AnyModifier, w, FALSE,
                                ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync,
                                None, None);
    }
    else
    {
        /* Here we grab all combinations of well known modifiers */
        status |=
            XGrabButton (dpy, button, modifier,
                                w, FALSE,
                                ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync,
                                None, None);
        status |=
            XGrabButton (dpy, button, modifier | ScrollLockMask,
                                w, FALSE,
                                ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync,
                                None, None);
        status |=
            XGrabButton (dpy, button, modifier | NumLockMask,
                                w, FALSE,
                                ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync,
                                None, None);
        status |=
            XGrabButton (dpy, button, modifier | LockMask, w, FALSE,
                                ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync,
                                None, None);
        status |=
            XGrabButton (dpy, button, modifier | ScrollLockMask | NumLockMask,
                                w, FALSE,
                                ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync,
                                None, None);
        status |=
            XGrabButton (dpy, button, modifier | ScrollLockMask | LockMask,
                                w, FALSE,
                                ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync,
                                None, None);
        status |=
            XGrabButton (dpy, button, modifier | LockMask | NumLockMask,
                                w, FALSE,
                                ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync,
                                None, None);
        status |=
            XGrabButton (dpy, button,
                                modifier | ScrollLockMask | LockMask | NumLockMask,
                                w, FALSE,
                                ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync,
                                None, None);
    }

    return (status == GrabSuccess);
}

void
ungrabButton (Display * dpy, int button, int modifier, Window w)
{
    TRACE ("entering ungrabKeys");

    TRACE ("entering grabButton");

    if (modifier == AnyModifier)
    {
        XUngrabButton (dpy, button, AnyModifier, w);
    }
    else
    {
        /* Here we ungrab all combinations of well known modifiers */
        XUngrabButton (dpy, button, modifier, w);
        XUngrabButton (dpy, button, modifier | ScrollLockMask, w);
        XUngrabButton (dpy, button, modifier | NumLockMask, w);
        XUngrabButton (dpy, button, modifier | LockMask, w);
        XUngrabButton (dpy, button, modifier | ScrollLockMask | NumLockMask, w);
        XUngrabButton (dpy, button, modifier | ScrollLockMask | LockMask, w);
        XUngrabButton (dpy, button, modifier | LockMask | NumLockMask, w);
        XUngrabButton (dpy, button, modifier | ScrollLockMask | LockMask | NumLockMask, w);
    }
}

void
initModifiers (Display * dpy)
{
    XModifierKeymap *modmap;
    KeySym *keymap;
    unsigned int keycode;
    int min_keycode;
    int max_keycode;
    int keysyms_per_keycode;
    int i;

    AltMask = 0;
    MetaMask = 0;
    NumLockMask = 0;
    ScrollLockMask = 0;
    SuperMask = 0;
    HyperMask = 0;
    keysyms_per_keycode = 0;
    min_keycode = 0;
    max_keycode = 0;

    XDisplayKeycodes (dpy, &min_keycode, &max_keycode);
    modmap = XGetModifierMapping (dpy);
    keymap = XGetKeyboardMapping (dpy, min_keycode, max_keycode - min_keycode + 1, &keysyms_per_keycode);

    if (modmap && keymap)
    {
        for (i = 3 * modmap->max_keypermod; i < 8 * modmap->max_keypermod; i++)
        {
            keycode = modmap->modifiermap[i];
            if ((keycode >= min_keycode) && (keycode <= max_keycode))
            {
                int j;
                KeySym *syms = keymap + (keycode - min_keycode) * keysyms_per_keycode;

                for (j = 0; j < keysyms_per_keycode; j++)
                {
                    if (!NumLockMask && (syms[j] == XK_Num_Lock))
                    {
                        NumLockMask = (1 << (i / modmap->max_keypermod));
                    }
                    else if (!ScrollLockMask && (syms[j] == XK_Scroll_Lock))
                    {
                        ScrollLockMask = (1 << (i / modmap->max_keypermod));
                    }
                    else if (!AltMask && ((syms[j] == XK_Alt_L) || (syms[j] == XK_Alt_R)))
                    {
                        AltMask = (1 << (i / modmap->max_keypermod));
                    }
                    else if (!SuperMask && ((syms[j] == XK_Super_L) || (syms[j] == XK_Super_R)))
                    {
                        SuperMask = (1 << (i / modmap->max_keypermod));
                    }
                    else if (!HyperMask && ((syms[j] == XK_Hyper_L) || (syms[j] == XK_Hyper_R)))
                    {
                        HyperMask = (1 << (i / modmap->max_keypermod));
                    }
                    else if (!MetaMask && ((syms[j] == XK_Meta_L) || (syms[j] == XK_Meta_R)))
                    {
                        MetaMask = (1 << (i / modmap->max_keypermod));
                    }
                }
            }
        }
    }

    /* Cleanup memory */
    if (modmap)
    {
        XFreeModifiermap (modmap);
    }

    if (keymap)
    {
        XFree (keymap);
    }

    /* In case we didn't find AltMask, use Mod1Mask */
    if (AltMask == 0)
    {
        AltMask = Mod1Mask;
    }
}
