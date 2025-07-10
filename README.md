Made for funsies during a batch at the [Recurse Center](https://www.recurse.com/)

To build:
clang -g chip8.c -o chip8 -I/opt/homebrew/include -D_THREAD_SAFE -L/opt/homebrew/lib -lSDL2

might have to do something different to find SDL on your maching

To use:
run ./chip8 [path/to/chip8/rom.ch8]
