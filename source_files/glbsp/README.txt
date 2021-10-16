
README for glBSP
================

by Andrew Apted.  JULY 2007


Introduction
------------

glBSP is a nodes builder specially designed to be used with OpenGL-based
DOOM game engines. It adheres to the "GL-Friendly Nodes" specification,
which means it adds some new special nodes to a WAD file that makes it
very easy for an OpenGL DOOM engine to compute the polygons needed for
drawing the levels.

There are many DOOM ports that understand the GL Nodes which glBSP
creates, including: EDGE, Doomsday (JDOOM), PrBoom, Vavoom, ZDoomGL,
Legacy 2.0, and Doom3D.  See the links below.


Status
------

The current version of glBSP is 2.24.  It has been tested and known to
work on numerous large wads, including DOOM I, DOOM II, TeamTNT's
Eternal III, Fanatic's QDOOM, and many others.


Copyright
---------

glBSP is Copyright (C) 2000-2007 Andrew Apted.  It was originally
based on "BSP 2.3" (C) Colin Reed and Lee Killough, which was created
from the basic theory stated in DEU5 (OBJECTS.C) by Raphael Quinet.

The GUI version (glBSPX) is based in part on the work of the FLTK
project, see http://www.fltk.org.

All trademarks are the propriety of their owners.


License
-------

Thanks to Lee Killough and André Majorel (former maintainer of BSP,
and devil-may-care French flying ace, respectively), glBSP is under
the GNU General Public License (GPL).  See the file 'COPYING.txt' in
the source package (or go to http://www.gnu.org) for the full text,
but to summarise:

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.


Links
-----

The homepage for glBSP can be found here:
   http://glbsp.sourceforge.net/

The "GL Friendly Nodes" specifications is here:
   http://glbsp.sourceforge.net/glnodes.html

The EDGE homepage can be found here:
   http://edge.sourceforge.net/

The DoomsDay (JDOOM) homepage is here:
   http://www.doomsdayhq.com/

Look here for PrBOOM:
   http://prboom.sourceforge.net/

Legacy info can be found here:
   http://legacy.newdoom.com/

The Vavoom site is here:
   http://www.vavoom-engine.com/

ZDoomGL is known to hang out around here:
   http://zdoomgl.mancubus.net/

The Doom3D site can be found here:
   http://doomworld.com/doom3d/


Acknowledgements
----------------

Andy Baker, for making binaries, writing code and other help.

Marc A. Pullen, for testing and helping with the documentation.

Lee Killough and André Majorel, for giving their permission to put
glBSP under the GNU GPL.

Janis Legzdinsh for fixing many problems with Hexen wads.

Darren Salt has sent in numerous patches.

Jaakko Keränen, who gave some useful feedback on the "GL Friendly
Nodes" specification.

The authors of FLTK (Fast Light Tool Kit), for a nice LGPL C++ GUI
toolkit that even I can get working on both Linux and Win32.

Marc Rousseau (author of ZenNode 1.0), Robert Fenske Jr (author
of Warm 1.6), L.M. Witek (author of Reject 1.1), and others, for
releasing the source code to their WAD utilities, and giving me
lots of ideas to "borrow" :), like blockmap packing.

Lee Killough and Colin Reed (and others), who wrote the original
BSP 2.3 which glBSP was based on.

Matt Fell, for the Doom Specs v1.666.

Raphael Quinet, for DEU and the original idea.

id Software, for not only creating such an irresistable game, but
releasing the source code for so much of their stuff.

... and everyone else who deserves it ! 



---------------------------------------------------------------------------

