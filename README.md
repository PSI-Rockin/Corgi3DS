# Corgi3DS
Corgi3DS is a dog-themed LLE 3DS emulator. It can successfully boot a B9S-hacked firmware which then loads an ARM9 payload from an SD image, such as [GodMode9](https://github.com/d0k3/GodMode9). It is even capable of loading the home menu and some commercial games (Ocarina of Time 3D, Pilotwings Resort, and potentially others).

<img alt="Screenshot 1" src="https://cdn.discordapp.com/attachments/589212595987283974/626212413997580288/Screen_Shot_2019-09-24_at_8.23.16_PM.png" width="401"> <img alt="Screenshot 2" src="https://i.imgur.com/zneCoU6.png">

Check out our [Discord](https://discord.gg/xFSDSeM)!

Outside contributions are greatly appreciated! [Corgi3DS is licensed under the GPLv3.](https://github.com/PSI-Rockin/Corgi3DS/blob/master/LICENSE)

## Compilation
Compilation requires Qt 5 and GMP. GMP is the library currently used for handling RSA crypto operations. Corgi3DS is cross-platform, **but it may be difficult to compile on Windows due to the GMP requirement**.

### MSYS2
Make sure you're using **MSYS2 MinGW 64-bit** for this.

```
pacman -S mingw-w64-x86_64-{qt5,gmp,cmake}
git clone --recursive https://github.com/PSI-Rockin/Corgi3DS.git
cd Corgi3DS
mkdir build && cd build
cmake .. -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
make
```

### macOS/Linux

#### QMake
```
git clone --recursive https://github.com/PSI-Rockin/Corgi3DS.git
cd Corgi3DS
qmake
make
```

#### CMake (3.1+)
```
git clone --recursive https://github.com/PSI-Rockin/Corgi3DS.git
cd Corgi3DS
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

## Usage
Required files:
* 3DS boot ROMs (boot9/boot11)
* Encrypted NAND image dumped from GodMode9. NAND dumps require `essential.exefs`.

Optional files:
* SD image
* Encrypted 3DS cart dump (.3ds)

These files can be given through either the GUI or the CLI. Type `-h` or `--help` for a list of available commands in the command line.

Due to the risk of corrupted data, make backups of all NAND and SD images used.

### Keyboard controls

* Arrow keys -> UP/DOWN/LEFT/RIGHT
* X -> A
* Z -> B
* S -> X
* A -> Y
* Q -> L
* W -> R
* H -> HOME Menu button
* P -> Power button
* Enter/Return -> START
* Spacebar -> SELECT

The touch screen can be used by clicking anywhere on the bottom screen with the mouse.

## Credits
Uses https://github.com/kokke/tiny-AES-c, a public domain AES library.
