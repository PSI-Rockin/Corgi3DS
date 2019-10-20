# Corgi3DS
Corgi3DS is a dog-themed LLE 3DS emulator. It can successfully boot a B9S-hacked firmware which then loads an ARM9 payload from an SD image, such as [GodMode9](https://github.com/d0k3/GodMode9). It is even capable of loading the home menu and some commercial games (Ocarina of Time 3D, Pilotwings Resort, and potentially others).

<img alt="Screenshot 1" src="https://cdn.discordapp.com/attachments/589212595987283974/626212413997580288/Screen_Shot_2019-09-24_at_8.23.16_PM.png" width="401"> <img alt="Screenshot 2" src="https://i.imgur.com/zneCoU6.png">

Check out our [Discord](https://discord.gg/xFSDSeM)!

Outside contributions are greatly appreciated! [Corgi3DS is licensed under the GPLv3.](https://github.com/PSI-Rockin/Corgi3DS/blob/master/LICENSE)

## Compilation
Compilation requires Qt 5 and GMP. GMP is the library currently used for handling RSA crypto operations. Corgi3DS is cross-platform, **but it may be difficult to compile on Windows due to the GMP requirement**.

### QMake
```
git clone --recursive https://github.com/PSI-Rockin/Corgi3DS.git
cd Corgi3DS
qmake
make
```

### CMake (3.1+)
```
git clone --recursive https://github.com/PSI-Rockin/Corgi3DS.git
cd Corgi3DS
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

## Usage
Currently, Corgi3DS can only be used from the command line. The usage is as follows:
```
./Corgi3DS <ARM9 bootROM> <ARM11 bootROM> <NAND image> <SD image> [Encrypted 3DS cartridge image]
```

NAND dumps require `essential.exefs` - get dumps from the latest version of GodMode9.

**N3DS support is currently unimplemented in Corgi3DS. If you try to use a N3DS NAND, it will not work.**

### Keyboard controls

* Arrow keys -> UP/DOWN/LEFT/RIGHT
* X -> A
* Z -> B
* S -> X
* A -> Y
* Q -> L
* W -> R
* Enter/Return -> START
* Spacebar -> SELECT

The touch screen can be used by clicking anywhere on the bottom screen with the mouse.

## Credits
Uses https://github.com/kokke/tiny-AES-c, a public domain AES library.
