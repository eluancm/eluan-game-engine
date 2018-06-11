# eluan-game-engine
idtech-esque networked 3d engine

This is the source for my playground to learn and test game making concepts.<br/>
See: http://eluancm.net/projects/game-engine/<br/>
It searchs for the data/ directory in the working directory.<br/>
Most assets are NOT included here! But there is shader code, game code, etc. Look into the Engine/Debug/data/ directory! (LUA gamecode is there in the progs/ subdirectory)<br/>

Basic commands for the demo:

WASD: walk<br/>
SPACE: jump<br/>
E: general "use" command (open doors, etc)<br/>
R: reload weapon<br/>
C: crouch<br/>
9: give all weapons and ammo<br/>
mouse: aim<br/>
left mouse button: fire<br/>
right mouse button: insert voxel<br/>
middle mouse button: remove voxel<br/>
mouse wheel: select weapon<br/>
F5: quicksave<br/>
F9: quickload<br/>

-----------------------------------------------------
I HAVE YET TO DO A DEMO WITH ASSETS I CAN DISTRIBUTE!
-----------------------------------------------------

Lots of weapons use the same simple untextured model I made.

Maps: toggle the console (with ~ or ') and enter:

map start<br/>
(my simple remake of a map famous in an old console game)

map level2<br/>
(lots of voxels to test, slow and memory-heavy)

map level3<br/>
(heighmap test, has a car to ride!)

map level5<br/>
(huge heightmap, has a car and a kind of flying vehicle with the car chassis)

WEAPONS:<br/>
Weapon 6 is a remote controlle missile, fun!

Multiplayer notes:<br/>
To force addresses and port numbers without using the command line, see data/autoexec.cfg<br/>
NAT not worked around at all, configure firewalls and port forwarding if necessary!<br/>

---------
COMPILING
---------
Requirements:

SDL (I used 2.0.0)<br/>
SDL_net (I used 2.0.0)<br/>
Bullet physics (I used bullet3-master_2016_03_31_0c1a455) (WAS USING: Bullet physics (I used 2.82-rev2709) (patched to remove the effects of m_fixContactNormalDirection and alter winding order of heightfields (both patches are in /RedundantBackup/Linux/Programas/bullet_2015/_heightfield_fix/)))<br/>
GLEW (I used 1.10.0)<br/>
FreeType2 (I used 2.5.2)<br/>
FTGL (I used r1266 with my own custom gl3.0 core profile mod)<br/>
openal-soft (I used 1.15.1)<br/>
freealut-vancegroup (I checked out at 2014_01_21 - was without update for ~2 months)<br/>

For windows, Microsoft Visual Studio 2008 + SP1

For linux, not sure about compiler versions, but do this:<br/>
-Full engine<br/>
1) copy the Engine/cmakefiles/CMakeLists_full.txt to Engine/CMakeLists.txt<br/>
2) create a build directory and run cmake or cmake-gui pointing to Engine/<br/>
3) make sure that DEDICATED_SERVER is not set in Engine/Engine/engine.h (should do this via cmake...)<br/>
-Dedicated server<br/>
1) copy the Engine/cmakefiles/CMakeLists_dedicated.txt to Engine/CMakeLists.txt<br/>
2) create a build directory and run cmake or cmake-gui pointing to Engine/<br/>
3) make sure that DEDICATED_SERVER is set in Engine/Engine/engine.h (should do this via cmake...)<br/>

Then use your generated build files and/or ide to compile.
