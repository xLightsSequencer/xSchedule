PortMidi - https://github.com/PortMidi/portmidi

portmidi.h and porttime.h are the PortMidi 217 headers xSchedule has always used. The Windows sources
(portmidi.c, pmutil.c, pmwin.c, pmwinmm.c, ptwinmm.c and their headers) are from the same release
(upstream commit 7a5f49c, January 2010) and are compiled into the Windows build. Linux builds link the
system libportmidi instead (-lportmidi). License: see license.txt.
