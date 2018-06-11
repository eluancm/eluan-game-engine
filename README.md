# eluan-game-engine
idtech-esque networked 3d engine

This is the source for my playground to learn and test game making concepts.
See: http://eluancm.net/projects/game-engine/
Assets not include here!
It searchs for the data/ directory in the working directory.

Basic commands for the demo:

WASD: walk
SPACE: jump
E: general "use" command (open doors, etc)
R: reload weapon
C: crouch
9: give all weapons and ammo
mouse: aim
left mouse button: fire
right mouse button: insert voxel
middle mouse button: remove voxel
mouse wheel: select weapon
F5: quicksave
F9: quickload

-----------------------------------------------------
I HAVE YET TO DO A DEMO WITH ASSETS I CAN DISTRIBUTE!
-----------------------------------------------------

Lots of weapons use the same simple untextured model I made.

Maps: toggle the console (with ~ or ') and enter:

map start
(my simple remake of a map famous in an old console game)

map level2
(lots of voxels to test, slow and memory-heavy)

map level3
(heighmap test, has a car to ride!)

map level5
(huge heightmap, has a car and a kind of flying vehicle with the car chassis)

WEAPONS:
Weapon 6 is a remote controlle missile, fun!

Multiplayer notes:
To force addresses and port numbers without using the command line, see data/autoexec.cfg
NAT not worked around at all, configure firewalls and port forwarding if necessary!

---------
COMPILING
---------
Requirements:

SDL (I used 2.0.0)
SDL_net (I used 2.0.0)
Bullet physics (I used bullet3-master_2016_03_31_0c1a455) (WAS USING: Bullet physics (I used 2.82-rev2709) (patched to remove the effects of m_fixContactNormalDirection and alter winding order of heightfields (both patches are in /RedundantBackup/Linux/Programas/bullet_2015/_heightfield_fix/)))
GLEW (I used 1.10.0)
FreeType2 (I used 2.5.2)
FTGL (I used r1266 with my own custom gl3.0 core profile mod)
openal-soft (I used 1.15.1)
freealut-vancegroup (I checked out at 2014_01_21 - was without update for ~2 months)

For windows, Microsoft Visual Studio 2008 + SP1

For linux, not sure about compiler versions, but do this:
-Full engine
1) copy the Engine/cmakefiles/CMakeLists_full.txt to Engine/CMakeLists.txt
2) create a build directory and run cmake or cmake-gui pointing to Engine/
3) make sure that DEDICATED_SERVER is not set in Engine/Engine/engine.h (should do this via cmake...)
-Dedicated server
1) copy the Engine/cmakefiles/CMakeLists_dedicated.txt to Engine/CMakeLists.txt
2) create a build directory and run cmake or cmake-gui pointing to Engine/
3) make sure that DEDICATED_SERVER is set in Engine/Engine/engine.h (should do this via cmake...)

Then use your generated build files and/or ide to compile.
