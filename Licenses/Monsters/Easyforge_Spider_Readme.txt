Giant Monster Spiders: free sample (Squinty)
============================================

A free sample of Giant Monster Spiders: Squinty, one of its 7 spiders, with every file and animation the pack has. The full pack has all 7 (Old Crusty, Rusty, Mossy, Zoomies, Big Mama, Big Daddy and Squinty): get Giant Monster Spiders (https://easyforge.itch.io/giant-monster-spiders).

Seven giant monster spiders for dark-fantasy, horror and RPG games, each a design of its own: armoured, hooked, overgrown, banded, bloated and gilded, plus a pale blind cave hunter. Every one is rigged on the same 107-bone skeleton with 24 animations - a full fight set, climbing up walls and across ceilings, and a headshot death where the head bursts in blood - so a game gets a whole bestiary of distinct enemies that share one controller.

By Easy Forge. Made 2026-10-29.

WHAT'S INSIDE (1)
  - Squinty: 1.2 m body, legs spanning 4.83 m: the pale blind cave hunter, bone-white with milky eyes and long thin legs; comes with its orb web

THE FILES
  One folder per spider. In each:
  - <name>.glb             the spider at full detail, its 24 clips and textures inside (Godot, Unreal, Blender)
  - <name>_LOD1, _LOD2     lighter versions for distance (.glb)
  - <name>.fbx             for Unity and other engines: skinned meshes <name>_LOD0/1/2, 107 bones, every clip
  - <name>_Body.png        one 2048 sheet and ONE material for the whole body, eyes and hair: colour, its alpha
                           cutting the hair cards out (0.4); _Normal.png the normal map (OpenGL, +Y up);
                           _Gloss.png roughness in G and Unity smoothness in A (glossy eyes, matte hair);
                           _Emission.png where it glows (eyes, glowing markings)
  - <name>_info.json       walk and run speeds (metres a second, to match the feet), how far the pounce lands and
                           where each climb ends (climbEnds)
  - <name>.easymotion.json its motion recipe: EasyAnimate opens the FBX and can make new clips for it
  - <name>.easyspider.json materials, LOD distances, clips and the headshot's bones, for engines that set it up from code
  - <name>_Web.glb / .fbx  its web (most of them), with its textures and <name>_web.json (where it waits, the anchors)
  - preview.png            a picture of it

UNITS AND AXES
  Metres, Y up, the spider faces +Z, its feet on y = 0 (stand it on the ground and it stands). Its left is +X.

CLIPS (every spider, 30 fps)
  Loops: Idle, Walk, Run, TurnLeft, TurnRight (the walk and run speeds are in <name>_info.json).
  Once: Attack, Bite, SwipeLeft, SwipeRight, Pounce (lands pounceDistance ahead), Threat (rears up),
  Jump, Hit, HitLeft, HitRight, Death (rolls over), DeathCollapse (drops on its belly) and DeathHeadshot (its
  head bursts in blood: the pieces fly, bounce and settle, a pool spreads, the body drops).
  Climbing: WallWalk and CeilingWalk loop on a wall and upside down on a ceiling; ClimbUpWall (floor to wall),
  ClimbToCeiling (wall to ceiling), ClimbOverLedge and ClimbDownLedge play once. Each climb carries the spider
  round on its Body bone: when it ends, move the root by that clip's climbEnds (p metres, q a rotation, in the
  frame it started in, from <name>_info.json) and go on - ClimbUpWall at a wall, then WallWalk up it.
THE HEADSHOT IN ANY ENGINE
  The head hangs from the Head bone; DeathHeadshot scales it to nothing at the hit and throws the pieces
  (bones Gore_0 ...) out of it, so it plays wherever the clip plays. In Unity, EasySpiderHeadshot does it live:
  the pieces are real physics, landing on your level.

USING THEM
  Unity: drag the FBX in, make its one material from the PNGs (colour with alpha clip 0.4, normal, gloss in
    Metallic/Smoothness, emission) and use the LOD Group Unity makes from _LOD0-2.
  Godot 4 / Unreal / Blender: import the GLB. Its rig, clips, materials and textures come with it.

MADE WITH AI
  Every one of the spiders was designed, refined and polished by a person in our own procedural 3D
  software, and checked again before release. AI tools helped plan the pack and put its files together.
  No third-party assets.

LICENCE (the short version; LICENSE.txt is the full one)
  Yes: use these in any number of your own games and projects, commercial too, and change them.
  No: resell or share the files themselves, or use them to train AI.

Questions or problems: contact us through the store page.
