# Corgi3DS
An LLE dog-themed 3DS emulator. Can successfully boot a B9S-hacked firmware which then loads an ARM9 payload from an SD image, such as GodMode9. Otherwise not too useful for anything else yet.

![GodMode9](https://i.imgur.com/8z7oVUU.png)

Requires Qt 5 and GMP. Currently only tested on macOS.

Usage from command line: [ARM9 boot ROM] [ARM11 boot ROM] [OTP file] [NAND image] [NAND CID] [SD image]

Keyboard control:

* Arrow keys -> UP/DOWN/LEFT/RIGHT
* X -> A
* Z -> B
* S -> X
* A -> Y
* Q -> L
* W -> R

Uses https://github.com/kokke/tiny-AES-c, a public domain AES library.
