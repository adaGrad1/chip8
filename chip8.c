#include "chip8.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Required for memset
#include <sys/time.h>

#define SCALE 10 // 640x320 window

const uint16_t WIDTH = 64;
const uint16_t HEIGHT = 32;

const uint16_t START_ADDR = 0x0200;
const uint32_t BUFFER_SIZE = 1 << 16;
const uint16_t FONTSET_SIZE = 5 * 16;
const uint8_t fontset[FONTSET_SIZE] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void update_keypresses(uint8_t keys[16]) {
  SDL_Event e;

  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) {
      exit(0);
    }

    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
      uint8_t pressed = (e.type == SDL_KEYDOWN);

      // Map keyboard to CHIP-8 keypad
      // CHIP-8 keypad:    Keyboard mapping:
      // 1 2 3 C           1 2 3 4
      // 4 5 6 D           Q W E R
      // 7 8 9 E           A S D F
      // A 0 B F           Z X C V
      switch (e.key.keysym.sym) {
      case SDLK_1:
        keys[0x1] = pressed;
        break;
      case SDLK_2:
        keys[0x2] = pressed;
        break;
      case SDLK_3:
        keys[0x3] = pressed;
        break;
      case SDLK_4:
        keys[0xC] = pressed;
        break;
      case SDLK_q:
        keys[0x4] = pressed;
        break;
      case SDLK_w:
        keys[0x5] = pressed;
        break;
      case SDLK_e:
        keys[0x6] = pressed;
        break;
      case SDLK_r:
        keys[0xD] = pressed;
        break;
      case SDLK_a:
        keys[0x7] = pressed;
        break;
      case SDLK_s:
        keys[0x8] = pressed;
        break;
      case SDLK_d:
        keys[0x9] = pressed;
        break;
      case SDLK_f:
        keys[0xE] = pressed;
        break;
      case SDLK_z:
        keys[0xA] = pressed;
        break;
      case SDLK_x:
        keys[0x0] = pressed;
        break;
      case SDLK_c:
        keys[0xB] = pressed;
        break;
      case SDLK_v:
        keys[0xF] = pressed;
        break;
      }
    }
  }
}

int prevTick = 0;
long get_time(long frac_sec) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec * frac_sec + (ts.tv_nsec * frac_sec) / (1000000000);
}
uint8_t tick() {
  long t = get_time(60);
  if (prevTick == 0) {
    prevTick = t;
  }
  uint8_t incr = t - prevTick;
  prevTick = t;
  return incr;
}

void draw_screen(SDL_Renderer *renderer, uint8_t display[32][64]) {
  // Clear screen (black)
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  // Draw CHIP-8 display
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White pixels

  int pixel_width = SCALE;
  int pixel_height = SCALE;

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (display[y][x]) {
        SDL_Rect pixel = {x * pixel_width, y * pixel_height, pixel_width,
                          pixel_height};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
  }
  SDL_RenderPresent(renderer);
}

void sleep_us(uint32_t sleep_time) {
  struct timespec t = {sleep_time / 1000000, (sleep_time % 1000000) * 1000};
  nanosleep(&t, NULL);
}

// cpu.c - All instruction execution
void cpu_execute_instruction(chip8_t *c8, uint16_t instr) {
  if ((instr & 0xF000) == 0x0000) { // misc
    switch (instr) {
    case 0x00E0: // clear screen
      memset(c8->display, 0, HEIGHT * WIDTH);
      break;
    case 0x00EE:
      c8->stack_pointer -= 1;
      c8->pc = c8->stack[c8->stack_pointer];
      break;
    }
  } else if ((instr & 0xF000) == 0x1000) { // JMP
    uint16_t addr = instr & 0x0FFF;
    c8->pc = addr - 0x02;
  } else if ((instr & 0xF000) == 0x2000) { // JSR
    uint16_t addr = instr & 0x0FFF;
    c8->stack[c8->stack_pointer++] = c8->pc;
    c8->pc = addr - 2;
  } else if ((instr & 0xF000) == 0x3000) { // SKEQ (reg, constant)
    uint8_t reg_idx = (instr & 0x0F00) >> 8;
    uint8_t constant = instr & 0x00FF;
    if (c8->regs[reg_idx] == constant) {
      c8->pc += 2;
    }
  } else if ((instr & 0xF000) == 0x4000) { // SKNE (reg, constant)
    uint8_t reg_idx = (instr & 0x0F00) >> 8;
    uint8_t constant = instr & 0x00FF;
    if (c8->regs[reg_idx] != constant) {
      c8->pc += 2;
    }
  } else if ((instr & 0xF000) == 0x5000) { // SKEQ (regs)
    uint8_t reg_idx = (instr & 0x0F00) >> 8;
    uint8_t reg_idx_2 = (instr & 0x00F0) >> 4;
    if (c8->regs[reg_idx] == c8->regs[reg_idx_2]) {
      c8->pc += 2;
    }
  } else if ((instr & 0xF000) == 0x6000) { // mov constant into reg
    uint8_t reg_idx = (instr & 0x0F00) >> 8;
    uint8_t constant = instr & 0x00FF;
    c8->regs[reg_idx] = constant;
  } else if ((instr & 0xF000) == 0x7000) { // add constant into reg
    uint8_t reg_idx = (instr & 0x0F00) >> 8;
    uint8_t constant = instr & 0x00FF;
    c8->regs[reg_idx] += constant;
  } else if ((instr & 0xF000) == 0x8000) { // various 2-reg instrs
    uint8_t r = (instr & 0x0F00) >> 8;
    uint8_t y = (instr & 0x00F0) >> 4;

    if ((instr & 0x000F) == 0x0000) { // set
      c8->regs[r] = c8->regs[y];
    } else if ((instr & 0x000F) == 0x0001) { // or
      c8->regs[r] |= c8->regs[y];
    } else if ((instr & 0x000F) == 0x0002) { // and
      c8->regs[r] &= c8->regs[y];
    } else if ((instr & 0x000F) == 0x0003) { // xor
      c8->regs[r] ^= c8->regs[y];
    } else if ((instr & 0x000F) == 0x0004) { // add with carry
      uint8_t carry = (((uint16_t)c8->regs[r]) + ((uint16_t)c8->regs[y])) >> 8;
      c8->regs[r] += c8->regs[y];
      c8->regs[15] = carry;
    } else if ((instr & 0x000F) == 0x0005) { // subtract with borrow
      uint8_t borrow = !(c8->regs[y] > c8->regs[r]);
      c8->regs[r] -= c8->regs[y];
      c8->regs[15] = borrow;
    } else if ((instr & 0x000F) == 0x0006) { // shr
      uint8_t bit0 = c8->regs[r] & 0x01;
      c8->regs[r] >>= 1;
      c8->regs[15] = bit0;
    } else if ((instr & 0x000F) == 0x0007) { // right subtract
      uint8_t borrow = c8->regs[y] >= c8->regs[r];
      c8->regs[r] = c8->regs[y] - c8->regs[r];
      c8->regs[15] = borrow;
    } else if ((instr & 0x000F) == 0x000E) { // shl
      uint8_t bit7 = (c8->regs[r] & 0x80) >> 7;
      c8->regs[r] <<= 1;
      c8->regs[15] = bit7;
    }
  } else if ((instr & 0xF000) == 0x9000) { // skne (reg, reg)
    uint8_t reg_idx = (instr & 0x0F00) >> 8;
    uint8_t reg_idx_2 = (instr & 0x00F0) >> 4;
    if (c8->regs[reg_idx] != c8->regs[reg_idx_2]) {
      c8->pc += 2;
    }
  } else if ((instr & 0xF000) == 0xA000) { // load constant into index register
    c8->index_register = instr & 0x0FFF;
  } else if ((instr & 0xF000) == 0xB000) { // jump with reg offset
    uint16_t fixed_jmp = instr & 0x0FFF;
    c8->pc = fixed_jmp + c8->regs[0];

    c8->pc -= 2; // account for pc += 2 at end of loop

  } else if ((instr & 0xF000) == 0xC000) { // random number <= arg
    uint8_t r = (instr & 0x0F00) >> 8;
    uint8_t max_val = instr & 0x00FF;
    c8->regs[r] = (rand() & max_val);
  } else if ((instr & 0xF000) == 0xD000) { // draw sprite
    uint8_t x = c8->regs[(instr & 0x0F00) >> 8];
    uint8_t y = c8->regs[(instr & 0x00F0) >> 4];
    uint8_t s = instr & 0x000F;
    c8->regs[15] = 0;
    for (int cur_y = y; cur_y < y + s; cur_y++) {
      uint8_t byte = (c8->mem[c8->index_register + cur_y - y]);
      for (int cur_x = x; cur_x < x + 8; cur_x++) {
        uint8_t bit = !!(byte & (128 >> (cur_x - x)));
        if (!c8->regs[15] && c8->display[cur_y % HEIGHT][cur_x % WIDTH] &&
            bit) {
          c8->regs[15] = 1;
        }
        c8->display[cur_y % HEIGHT][cur_x % WIDTH] ^= bit;
      }
    }
  } else if ((instr & 0xF000) == 0xE000) { // keyboard inputs
    uint8_t reg_idx = (instr & 0x0F00) >> 8;
    uint8_t key_idx = c8->regs[reg_idx];
    uint8_t skip = c8->keys[key_idx];
    if ((instr & 0x00FF) == 0xA1)
      skip = !skip;
    c8->pc += 2 * skip;
  } else if ((instr & 0xF000) == 0xF000) { // various random things
    uint8_t n_regs;
    uint8_t reg;
    switch (instr & 0x00FF) {
    case (0x07): // get delay timer into vr
      c8->regs[(instr & 0x0F00) >> 8] = c8->delay_timer;
      break;
    case (0x0a): // wait for for keypress,put key in register vr
      break;
    case (0x15): // set the delay timer to vr
      c8->delay_timer = c8->regs[(instr & 0x0F00) >> 8];
      break;
    case (0x18): // set the sound timer to vr
      c8->sound_timer = c8->regs[(instr & 0x0F00) >> 8];
      break;
    case (0x1e): // add register vr to the index register
      reg = c8->regs[(instr & 0x0F00) >> 8];
      c8->index_register += reg;
      break;
    case (0x29): // point I to the sprite for hexadecimal character in vr
      c8->index_register = c8->regs[(instr & 0x0F00) >> 8] *
                           5; // 5 is because each char is 5 rows tall.
      break;
    case (0x33): // store the bcd representation of register vr at location
                 // I,I+1,I+2
      reg = c8->regs[(instr & 0x0F00) >> 8];
      c8->mem[c8->index_register] = reg / 100;
      c8->mem[c8->index_register + 1] = (reg / 10) % 10;
      c8->mem[c8->index_register + 2] = reg % 10;
      break;
    case (0x55): // store registers v0-vr at location I onwards
      n_regs = (instr & 0x0F00) >> 8;
      for (int i = 0; i <= n_regs; i++) {
        c8->mem[c8->index_register++] = c8->regs[i];
      }
      break;
    case (0x65): // load registers v0-vr from location I onwards
      n_regs = (instr & 0x0F00) >> 8;
      for (int i = 0; i <= n_regs; i++) {
        c8->regs[i] = c8->mem[c8->index_register++];
      }
      break;
    }
  }
}
int cpu_decode_and_execute(chip8_t *c8) {
  uint16_t instr = (((uint16_t)c8->mem[c8->pc]) << 8) + c8->mem[c8->pc + 1];
  if (instr == 0) {
    return 1;
  }
  cpu_execute_instruction(c8, instr);
  c8->pc += 2;
  return 0;
}

// display.c - Rendering logic
void display_clear(chip8_t *chip8);
void display_draw_sprite(chip8_t *chip8, uint8_t x, uint8_t y, uint8_t height);
void display_render(SDL_Renderer *renderer, chip8_t *chip8);

// input.c - Keyboard handling
void input_update(chip8_t *chip8);

// timer.c - Timer management
void timer_update(chip8_t *chip8);

int main(int argc, char *argv[]) {
  printf("%d\n", argc);
  char *rom_path;
  if (argc < 2) {
    printf("Please supply a ROM path as an argument!\n");
    exit(1);
  } else {
    rom_path = argv[1];
  }
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window *window = SDL_CreateWindow(
      "CHIP-8 Display", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      WIDTH * SCALE, HEIGHT * SCALE, SDL_WINDOW_SHOWN);
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  chip8_t *c8 = calloc(1, sizeof(chip8_t));

  FILE *fp = fopen(rom_path, "rb");
  if (!fp) {
    printf("File not found!\n");
    exit(1);
  }
  int bytesRead =
      fread((c8->mem) + START_ADDR, 1, BUFFER_SIZE - START_ADDR, fp);
  memcpy(c8->mem, &fontset, sizeof(fontset));
  c8->pc = START_ADDR;
  while (!cpu_decode_and_execute(c8)) {
    uint8_t t = tick();
    sleep_us(1200);
    if (t >= c8->delay_timer) {
      c8->delay_timer = 0;
    } else {
      c8->delay_timer -= t;
    }
    if (t >= c8->sound_timer) {
      c8->sound_timer = 0;
    } else {
      c8->sound_timer -= t;
    }
    if (t > 0) {
      draw_screen(renderer, c8->display);
      // draw_screen_cli(screen);
      update_keypresses(c8->keys);
    };
  }
  fclose(fp);
}