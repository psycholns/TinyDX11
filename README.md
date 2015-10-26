# TinyDX11
DirectX 11 compute shader example in c++ for 1k/4k intros by Psycho/Loonies

It is a dx11 adaption of my old opengl 1kb, which I have been using as "API size benchmark" for a long time: http://www.pouet.net/prod.php?which=32735
Currently compresses to 821 bytes including the 270 bytes of compressed shader source.

This is not a "insert shader.hlsl and (clinkster)music.asm here" intro framework like IQ's set at http://www.iquilezles.org/www/material/isystem1k4k/isystem1k4k.htm but rather meant as a starting point for your own system / source of inspiration / Toolbox when in need.


#Building
Builds with Visual C++ 2013

Based/needs on DirectX June 2010 SDK  (todo: a slightly larger version based on shadercompiler_47)

Needs Crinkler 2.0+ from http://www.pouet.net/prod.php?which=18158 - rename crinkler.exe to link.exe and place in solution directory


#Acknowledgements
Mentor/TBC for some of the size hacks
