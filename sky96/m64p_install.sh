#!/bin/sh
#/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
# *   Mupen64plus - m64p_install.sh                                         *
# *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
# *   Copyright (C) 2009 Richard Goedeken                                   *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU General Public License as published by  *
# *   the Free Software Foundation; either version 2 of the License, or     *
# *   (at your option) any later version.                                   *
# *                                                                         *
# *   This program is distributed in the hope that it will be useful,       *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU General Public License for more details.                          *
# *                                                                         *
# *   You should have received a copy of the GNU General Public License     *
# *   along with this program; if not, write to the                         *
# *   Free Software Foundation, Inc.,                                       *
# *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
# * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

# terminate the script if any commands return a non-zero error code
set -e

if [ -z "$MAKE" ]; then
	MAKE=make
fi

if [ -z "$M64P_COMPONENTS" ]; then
	M64P_COMPONENTS="core rom ui-console audio-sdl input-sdl rsp-hle video-rice video-glide64mk2"
fi

TOP_DIR="$(cd "$(dirname "$0")/.." && pwd)"
# Install binaries into a dedicated directory to avoid name clashes with the
# source folder.  All other artefacts are installed to the project root by
# default so they can be picked up by the build scripts without adjusting
# library paths.
MAKE_INSTALL="PLUGINDIR= SHAREDIR= BINDIR=bin MANDIR= LIBDIR= APPSDIR= ICONSDIR=icons INCDIR=api LDCONFIG=true"

for component in ${M64P_COMPONENTS}; do
        if [ "${component}" = "core" ]; then
                component_type="library"
        elif  [ "${component}" = "rom" ]; then
                continue
        elif  [ "${component}" = "ui-console" ]; then
                component_type="front-end"
        else
                component_type="plugin"
        fi

        echo "************************************ Installing ${component} ${component_type}"
        "$MAKE" -C source/mupen64plus-${component}/projects/unix install "$@" ${MAKE_INSTALL} DESTDIR="${TOP_DIR}/"
done
