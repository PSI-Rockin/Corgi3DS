pacman -S --noconfirm mingw-w64-gmp
mkdir /corgi3DS/build  && cd /corgi3DS/build
x86_64-w64-mingw32-cmake .. 
make