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
 
        xcompmgr - (c) 2003 Keith Packard
        metacity - (c) 2003, 2004 Red Hat, Inc.
        xfwm4    - (c) 2004 Olivier Fourdan
 
 */

/* THIS CODE IS NOT FINISHED YET, IT WON'T COMPILE, PLEASE BE PATIENT */

#ifndef INC_COMPOSITOR_H
#define INC_COMPOSITOR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>

#include "display.h"
#include "screen.h"
#include "client.h"

void compositorHandleEvent (DisplayInfo *, XEvent *);
void compositorAddWindow (Client *, gboolean);
void compositorRemoveWindow (Client *);
void compositorManageScreen (ScreenInfo *);
void compositorUnmanageScreen (ScreenInfo *);
void compositorDamageClient (Client *);

#endif /* INC_COMPOSITOR_H */
