Editor: GIMP
Author: Ognjen "Suiseiseki" Robovich
Date: 2018

Textures with just number in their name are downloaded from "www.textures.com".
All of those textures were edited (scaled, tiled, filtered, etc.) in GIMP and exported in .jpg format.
> Read license of Textures.com in "Terms Of Use" section.

I will not leave any kind of license here, because I didn't create those textures from scratch. License of Textures.com applies.
Use them, but please be sure to read license of Textures.com if you want to share your work. No need to credit me.

 - All textures are 'material', so be carefull when tiling them, and use them moderately.
 - You can use 'texset.c' to make 'texset.cfg', and use it in Tesseract with:
	/exec media/texture/suiseiseki/texset.cfg
 - You can also 'tex.c' to make 'tex.tex', and import textures individually with:
	/texload suiseiseki/tex

Usage of 'texset.c' and 'tex.c' is pretty straight forward, I use Kali Linux, but same applies for similar OSs with minor changes:

 - Open Terminal on Ctrl+Alt+T, and execute next few commands:
	$ cd .tesseract/media/texture/unnamed                                        // Go to 'unnamed' folder (your name ofcourse)
	$ ls | cat -n | while read n f; do mv "$f" "$n.jpg"; done                    // Rename all images inside folder to numbers (.jpg/.png)
	$ gcc tex.c                                                                  // Compile C program 'tex.c'
	$ ./a.out && mv "a.tex" "texname.tex"                                        // Run 'tex.c', add values, and rename file to 'texname.tex'

I advise you to change 'texset.c' and 'tex.c' for your own needs. I wrote them quickly to save time, and these are latest versions.
If you notice some texture error, please contact me on Discord or mail, so I can fix it and update node on Tesseract Forum...

 - Discord: Daemes#2613
 - Mail: 7year@protonmail.com

Cheers, Suiseiseki ;)
