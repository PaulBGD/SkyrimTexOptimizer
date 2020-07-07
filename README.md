# SkyrimTexOptimizer
_This is in beta, there are no guarantees that anything works well._

Licensed GPL 3.0.

## View On Nexus

Link will go here once I get an official release =)

## Background

This is my first project in C++.
I don't have a lot of knowledge terms of structuring, the standard library, and build tools.
Please feel free to contribute and correct anything that's done improperly.

## How to build

I'm building this with MSVC 2017, but newer version probably work too.
I currently build it with CMake DEBUG mode, as RELEASE mode seems to break it.
That probably should get checked out soon, it's somewhere in the doCPUWork logic.

### Steps:

1. Make a build directory.
2. In the build directory run `cmake ..`
3. My setup uses nmake, which can be ran with `nmake` (I think)
4. STO.exe should now be in the current directory.

## Goals

### Completed
- A one step process for converting all textures to a proper format, compression, and size for the user.

### Todo

- Importing meshes and seeing what textures they use, instead of optimizing all textures.
  As well, allowing the scale of the mesh to correspond with out big of a texture that's outputted.
  There are many texture packs out there that release 4096x4096 versions of flowers, which is wasteful for 99% of users.
- A GUI in which the user can specify input/output directories and rules based off file and directory location.
- A configuration format that can be imported/exported, allowing users to share rules.
- In the future I would like to support fixing broken meshes, with potentially some levels of optimization.
- Also there's multiple memory leaks in the NIF library, many `new` calls without matching `delete` calls.

## Thanks

### Cathedral Assets Optimizer

The file `textures.cpp/hpp` is just a heavily modified version of the one from their repository, it gives much better
results than using `texconv.exe` from the command line and I appreciate them for it. 

### BodySlide and Outfit Studio

As far as I'm aware they are 100% the creators of the NIF library, which is fantastic to use.
Thank you for releasing everything open source.
