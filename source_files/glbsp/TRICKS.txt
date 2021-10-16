
glBSP and Map-Tricks/Errors
===========================

1. Self-referencing Lines

This trick involves linedefs with the same sector reference
on both sidedefs.  In the original DOOM renderer, segs from
these lines are virtually ignored (though any mid-masked
texture is still drawn).  Hence by using a sector which is
different from the surrounding sector, people could create
the following special physics effects:

  1. invisible walkways (self-ref sector floor is higher)
  2. deep water (self-ref sector floor is lower)
  3. force fields (self-ref sector blocks movement)
  4. railings you can jump over.
 
glBSP detects these lines and does two things: prevents
spurious warnings about mismatched sectors, and adjusts
the final subsector so that the FIRST seg is not one on
a self-reference line.  It does the latter because most
engines use the very first seg to decide what sector to
associate the subsector with.

Engines should be aware of the trick and perform their
own analysis to emulate it.  NOTE this trick will cause
some subsectors to contain segs from multiple sectors.


2. Overlapping Lines

This trick is mostly used to make higher mid-masking textures.

glBSP can detect perfectly overlapping linedefs (ones using
the same start and end vertices, possibly swapped), and will
IGNORE them (producing no segs from the second and subsequent
lines).

Engines should also detect overlapping lines and emulate
the trick (by rendering the additional mid-masked textures).


3. One-Sided Windows

This trick uses a one-sided linedef where a two-sided linedef
should be, so the back of the one-sided line faces into a
normal sector.  In the original DOOM renderer, there was no
back-facing seg in the BSP, hence you could see through the
line when viewed from the back.  When viewed from the front,
the one-sided linedef acted normally (blocked your view).

glBSP can detect many usages of this trick (although not 100%
since that would require much more expensive analysis).  It
has to be explicitly enabled with the -windowfx option (or
for libglbsp via a field in nodebuildinfo_t).

When found, glBSP will add a REAL seg along the back of the
one-sided linedef.  This is the only time that segs are
placed on a linedef with no corresponding sidedef.

Engines should detect that and emulate the trick accordingly.
Depending on how your renderer does occlusion culling, it may
not be necessary to do anything special.


4. Missing Lowers/Uppers

This trick is mentioned since it is very common, though it
doesn't affect or interfere with node building in any way.

When the DOOM renderer encounters a missing lower texture,
and the floor on the back side is visible (z < camera.z)
then the space where the lower texture should be will be
filled by that floor texture.  The same thing happens with
missing uppers too.

This effect can be used to create a deep water effect, or
for drawing a dark ceiling over a bright lava sector.

When the back floor is out of view (z > camera.z), then
the missing lower is not drawn at all.  Same for missing
uppers.  This is sometimes used to create "invisible"
doors and platforms.


5. Map Errors

PWADs often contain a few mapping errors in them, and even
the IWADs contain some small errors, for example:

  Doom1 E3M2:  Linedef #156 has a wrong sector reference.
  Doom2 MAP02: Sector #23 is not properly closed.
  Doom2 MAP14: Linedefs #964 and #966 are missing back sidedefs.
  Hexen MAP02: Vertex #1370 is not joined to linedef #668.

glBSP is not psychic and cannot automatically fix errors
in maps (and besides, sometimes they are used deliberately
for special effects, as described above).

When a linedef has a wrong sector reference, the usual
result is that the subsector (or subsectors) near that
spot will contain segs belonging to different sectors.
I cannot say what best way to handle it is -- engine
developers should at least be aware of the possibility.

Other map errors are more serious and cause structural
problems, like degenerate subsectors (which contain only
one or two segs).  There is nothing glBSP or the engine
can do in these cases.


---------------------------------------------------------------------------

