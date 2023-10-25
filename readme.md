<p align="center">
  <img width="600" src="https://dl.dropboxusercontent.com/s/jld3nn81hag8w8g/logo_ol640x160.png?dl=0" alt="Valhalla Logo">
</p>

<p align="center">
  A first-person arena shooter project based on <a href="http://tesseract.gg/">Tesseract</a> which focuses on both old school and casual gameplay.
</p>

<p align="center">
  <a href="https://discord.gg/qFMAde5WQP">
    <img src="https://img.shields.io/badge/Discord-blue?style=for-the-badge&logo=discord&logoColor=white" alt="Discord link"/>
  </a>
  <a href="https://www.youtube.com/channel/UCjAPRHO03EqzBtTbcHXEbBw">
    <img src="https://img.shields.io/badge/YouTube-red?style=for-the-badge&logo=youtube&logoColor=white" alt="YouTube link"/>
  </a>
</p>

# About
We are a team of volunteers that have been collaborating since 2019 and are brought together by our shared enthusiasm and creativity.
In an effort to enhance our skills and learn new things, we are working to bring the project's shared goal to life.

Although Valhalla is inspired by [Cube 2: Sauerbraten](http://sauerbraten.org) and numerous other ancient video games, it does not intend to blindly follow the path of other similar projects and attempt to create yet another *clone* of well-known old-school games, but rather it seeks to innovate the genre by proposing a simple but enjoyable shooter, experimenting with fresh concepts and combining old-school and modern elements to create something distinctive, leading to a modern interpretation of the classic arena gameplay.

We strive to offer the majority of players a satisfying gaming experience by making sure Valhalla is always free and open source, collaborating with the community, focusing on the creation of a nice blend of classic and casual gameplay and trying to enhance the built-in map editor based on the Cube Engine 2.

Anyone can contribute features, assets and ideas to the project to help it develop.

# Goals
Some of the goals we have in mind are:
- **Accessibility**: Add tutorials and other important features to help new players get into the game.
- **Cross-Platform Support**: Support Windows, Mac, and Linux.
- **Depth**: Create simple yet convincing lore to give the game a sense of purpose and its universe a story with depth.
- **Game Engine**: Tweak, maintain, and update the engine together with the community.
- **Immersion**: Bring audio, effects, and gunplay closer to modern standards.
- **Iterate on the Arena Shooter Formula**: Experiment with new ideas and enhance the arena shooter experience.
- **Map Editor Improvements**: Make most features of the map editor accessible through a simple and clear user interface.
- **Simplicity**: Build gameplay that is interesting and varied while keeping the game itself simple, polished, and fun.
- **UI/HUD Improvements and Customization**: Make the UI clear and accessible while allowing easy customization.
- **Unique Aesthetics**: Maintain an art direction that helps create something distinct and original.
- **Vibrant Atmosphere**: Avoid hyper-realistic and gloomy design language, build levels that feel open and bright.

# License
Valhalla's source code and related configuration files and scripts are licensed under the [permissive zlib license](./license.md).  
The included [ENet network library](./source/enet) which the project uses is covered by an MIT-style license, which is however compatible with the above license for all practical purposes.

The game assets are excluded from the licenses mentioned above as these may be subject to individual copyright and have specific distribution restrictions.

# Contributions
Without invaluable contributions, this project would not have reached this stage.  
To view a list of people who have contributed to the source code and related configuration files and scripts, consult our [contributors.md](./contributors.md) file.  
If you would like to know how to make significant contributions to the project, please take a moment to read our [contribution guidelines](https://github.com/project-valhalla/.github/blob/main/CONTRIBUTING.md).  

# Build Instructions
Below, we have provided an overview of the recommended or available methods for building the source code.

## Windows
The binaries will be available in `bin/bin64` and `bin/bin32`. They can be launched by using the `valhalla.bat` batch script located in the root directory of the repository.

### Code::Blocks
In order to build the project on [Code::Blocks](https://www.codeblocks.org/downloads/binaries/), you can use [TDM-GCC](http://tdm-gcc.tdragon.net/download).  
Make sure Code::Blocks is using the chosen compiler by checking the directory used at `Settings` -> `Compiler` -> `Toolchain executables` -> `Compiler's installation directory` in Code::Blocks.  
Once the compiler is configured, open the `valhalla.cbp` project located in `source/vcpp` and click on `Build` -> `Build`.

### Visual Studio
In order to build the project on [Visual Studio](https://visualstudio.microsoft.com/en/), you will require one of the latest versions of Visual Studio and its C/C++ extension pack.  
Open the `valhalla.sln` solution or the `valhalla.vcxproj` project located in `source/vcpp` and click on `Build` -> `Build valhalla`.

## Linux
In order to build the source code by using the Makefile included in the `source` directory, you need to ensure you have the **SDL2**, **SDL2-image**, **SDL2-mixer** and **OpenGL** development libraries installed.  
After configuring the required libraries, change the current directory into the root directory of the repository in your terminal and execute the `make install` command:
```
cd ~/main/source
make install
```
This will also install the binaries and make them accessible in the `bin/bin_unix` directory, which can be executed using the `valhalla_unix` shell script located in the root directory of the repository.

## macOS
### Makefile
You can use the Makefile located in the `source` directory to build the source code, similarly to the steps described in the [Linux](#linux) section.  
To launch the application, you can run the shell script `valhalla.sh` found in `bin/valhalla.app/Contents/MacOS`.

### Code::Blocks
Alternatively, you can use Code::Blocks to load and build the project by opening the `source/vcpp/valhalla.cbp` file and following similar steps to those described in the [Windows](#windows) section.
