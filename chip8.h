#include <stdlib.h>
#include <SDL2/SDL.h>

// chip8.h - Main emulator state and interface
typedef struct {
    uint8_t mem[4096];
    uint8_t regs[16];
    uint16_t index_register;
    uint16_t pc;
    uint16_t stack[16];
    uint8_t stack_pointer;
    uint8_t display[32][64];
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t keys[16];
} chip8_t;

// cpu.c - All instruction execution
void cpu_execute_instruction(chip8_t* chip8, uint16_t instr);
int cpu_decode_and_execute(chip8_t* chip8);

// display.c - Rendering logic
void display_clear(chip8_t* chip8);
void display_draw_sprite(chip8_t* chip8, uint8_t x, uint8_t y, uint8_t height);
void display_render(SDL_Renderer* renderer, chip8_t* chip8);

// input.c - Keyboard handling
void input_update(chip8_t* chip8);

// timer.c - Timer management
void timer_update(chip8_t* chip8);

// instructions.c - Individual instruction implementations
void instr_clear_screen(chip8_t* chip8);
void instr_jump(chip8_t* chip8, uint16_t addr);
void instr_call(chip8_t* chip8, uint16_t addr);
// ... etc for each instruction type