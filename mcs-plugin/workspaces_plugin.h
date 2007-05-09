/*
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

        Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
*/

#ifndef INC_PLUGIN_H
#define INC_PLUGIN_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define RCFILE1 "workspaces.xml"
#define RCFILE2 "margins.xml"
#define PLUGIN_NAME "workspaces"
#define CHANNEL1 "workspaces"
#define CHANNEL2 "margins"

#define BORDER 5

void ws_create_channel (McsManager * manager, const char *channel,
                        const char *rcfile);

gboolean ws_save_channel (McsManager * manager, const char *channel,
                       const char *rcfile);

#endif /* INC_PLUGIN_H */
