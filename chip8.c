#include <SDL2/SDL.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> // Required for memset

#define SCALE 10  // 640x320 window
#define ROM_PATH "chip8-roms/roms/pong.rom"
#define VERBOSE 0

const uint16_t WIDTH = 64;
const uint16_t HEIGHT = 32;

const uint16_t START_ADDR = 0x0200;
const uint32_t BUFFER_SIZE = 1 << 16;
const uint16_t FONTSET_SIZE = 5 * 16;
const uint8_t fontset[FONTSET_SIZE] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0,		// 0
	0x20, 0x60, 0x20, 0x20, 0x70,		// 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0,		// 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0,		// 3
	0x90, 0x90, 0xF0, 0x10, 0x10,		// 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0,		// 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0,		// 6
	0xF0, 0x10, 0x20, 0x40, 0x40,		// 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0,		// 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0,		// 9
	0xF0, 0x90, 0xF0, 0x90, 0x90,		// A
	0xE0, 0x90, 0xE0, 0x90, 0xE0,		// B
	0xF0, 0x80, 0x80, 0x80, 0xF0,		// C
	0xE0, 0x90, 0x90, 0x90, 0xE0,		// D
	0xF0, 0x80, 0xF0, 0x80, 0xF0,		// E
	0xF0, 0x80, 0xF0, 0x80, 0x80		// F
};

void update_keypresses(uint8_t keys[16]){
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
            printf("keypress: %d\n", e.type);
            switch (e.key.keysym.sym) {
                case SDLK_1: keys[0x1] = pressed; break;
                case SDLK_2: keys[0x2] = pressed; break;
                case SDLK_3: keys[0x3] = pressed; break;
                case SDLK_4: keys[0xC] = pressed; break;
                case SDLK_q: keys[0x4] = pressed; break;
                case SDLK_w: keys[0x5] = pressed; break;
                case SDLK_e: keys[0x6] = pressed; break;
                case SDLK_r: keys[0xD] = pressed; break;
                case SDLK_a: keys[0x7] = pressed; break;
                case SDLK_s: keys[0x8] = pressed; break;
                case SDLK_d: keys[0x9] = pressed; break;
                case SDLK_f: keys[0xE] = pressed; break;
                case SDLK_z: keys[0xA] = pressed; break;
                case SDLK_x: keys[0x0] = pressed; break;
                case SDLK_c: keys[0xB] = pressed; break;
                case SDLK_v: keys[0xF] = pressed; break;
            }
        }
    }
}

int prevTick = 0;
long get_time(long frac_sec){
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * frac_sec + (ts.tv_nsec * frac_sec)/ (1000000000);
}
uint8_t tick(){
    long t = get_time(60);
    if(prevTick == 0){
        prevTick = t;
    }
    uint8_t incr = t - prevTick;
    prevTick = t;
    return incr;
}

void draw_screen(SDL_Renderer* renderer, uint8_t display[32][64]){
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
                    SDL_Rect pixel = {
                        x * pixel_width,
                        y * pixel_height,
                        pixel_width,
                        pixel_height
                    };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }

}

int main(){
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("CHIP-8 Display",
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        WIDTH * SCALE, HEIGHT * SCALE,
                                        SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    uint8_t buffer[BUFFER_SIZE];
    uint8_t regs[16];
    uint16_t idx_register = 0;

    FILE *fp = fopen(ROM_PATH, "rb");
    int bytesRead = fread(buffer+START_ADDR, 1, BUFFER_SIZE-START_ADDR, fp);
    uint16_t pc_stack[16];
    uint8_t pc_stack_ptr = 0;
    uint8_t screen[HEIGHT][WIDTH] = {0};
    uint8_t delay = 0;
    uint8_t sound = 0;
    uint8_t keys[16];
    memcpy(&buffer, &fontset, sizeof(fontset));
    for(int pc = START_ADDR; buffer[pc] || buffer[pc+1]; pc+=2){
        uint8_t t = tick();
        if(t >= delay){
            delay = 0;
        } else{
            delay -= t;
        }
        if(t >= sound){
            sound = 0;
        } else{
            sound -= t;
        }
        if(t > 0){
            draw_screen(renderer, screen);
            update_keypresses(&keys);
        }
        uint16_t instr = (((uint16_t)buffer[pc]) << 8) + buffer[pc+1];
        if(VERBOSE){
            printf("PC: 0x%x; ", pc);
            printf("REGS:\n");
            for(int i = 0; i < 16; i++){
                printf("%x,",regs[i]);
            }
            printf("\nidx reg: %x\n", idx_register);
            printf("instr: %x \n", instr);
        }
        if      ((instr & 0xF000) == 0x0000){ // misc
            switch(instr){
                case 0x00E0: // clear screen
                    memset(screen, 0, HEIGHT*WIDTH);
                    break;
                case 0x00EE:
                    pc_stack_ptr -= 1;
                    pc = pc_stack[pc_stack_ptr];
                    break;
            }
        }
        else if ((instr & 0xF000) == 0x1000){ // JMP
            uint16_t addr = instr & 0x0FFF;
            pc = addr - 0x02;
        }
        else if ((instr & 0xF000) == 0x2000){ // JSR
            uint16_t addr = instr & 0x0FFF;
            pc_stack[pc_stack_ptr++] = pc;
            pc = addr-2;
        }
        else if ((instr & 0xF000) == 0x3000){ // SKEQ (reg, constant)
            uint8_t reg_idx = (instr & 0x0F00) >> 8;
            uint8_t constant = instr & 0x00FF;
            if(regs[reg_idx] == constant){
                pc += 2;
            }
        }
        else if ((instr & 0xF000) == 0x4000){ // SKNE (reg, constant)
            uint8_t reg_idx = (instr & 0x0F00) >> 8;
            uint8_t constant = instr & 0x00FF;
            if(regs[reg_idx] != constant){
                pc += 2;
            }
        }
        else if ((instr & 0xF000) == 0x5000){ // SKEQ (regs)
            uint8_t reg_idx = (instr & 0x0F00) >> 8;
            uint8_t reg_idx_2 = (instr & 0x00F0) >> 4;
            if(regs[reg_idx] == regs[reg_idx_2]){
                pc += 2;
            }
        }
        else if ((instr & 0xF000) == 0x6000){ // mov constant into reg
            uint8_t reg_idx = (instr & 0x0F00) >> 8;
            uint8_t constant = instr & 0x00FF;
            regs[reg_idx] = constant;
        }
        else if ((instr & 0xF000) == 0x7000){ // add constant into reg
            uint8_t reg_idx = (instr & 0x0F00) >> 8;
            uint8_t constant = instr & 0x00FF;
            regs[reg_idx] += constant;
        }
        else if ((instr & 0xF000) == 0x8000){ // various 2-reg instrs
            uint8_t r = (instr & 0x0F00) >> 8;
            uint8_t y = (instr & 0x00F0) >> 4;

            if      ((instr & 0x000F) == 0x0000){ // set
                regs[r] = regs[y];
            }
            else if ((instr & 0x000F) == 0x0001){ // or
                regs[r] |= regs[y];
            }
            else if ((instr & 0x000F) == 0x0002){ // and
                regs[r] &= regs[y];
            }
            else if ((instr & 0x000F) == 0x0003){ // xor
                regs[r] ^= regs[y];
            }
            else if ((instr & 0x000F) == 0x0004){ // add with carry
                uint8_t carry = (((uint16_t)regs[r]) + ((uint16_t)regs[y])) >> 8;
                regs[r] += regs[y];
                regs[15] = carry;
            }
            else if ((instr & 0x000F) == 0x0005){ // subtract with borrow
                uint8_t borrow = regs[y] > regs[r];
                regs[r] -= regs[y];
                regs[15] = borrow;
            }
            else if ((instr & 0x000F) == 0x0006){ // shr
                uint8_t bit0 = regs[r] & 0x01;
                regs[r] >>= 1;
                regs[15] = bit0;
            }
            else if ((instr & 0x000F) == 0x0007){ // right subtract
                uint8_t borrow = regs[r] > regs[y];
                regs[r] = regs[y] - regs[r];
                regs[15] = borrow;
            }
            else if ((instr & 0x000F) == 0x000E){ // shl
                uint8_t bit7 = (regs[r] & 0x80) >> 7;
                regs[r] <<= 1;
                regs[15] = bit7;
            }
        }
        else if ((instr & 0xF000) == 0x9000){ // skne (reg, reg)
            uint8_t reg_idx = (instr & 0x0F00) >> 8;
            uint8_t reg_idx_2 = (instr & 0x00F0) >> 4;
            if(regs[reg_idx] != regs[reg_idx_2]){
                pc += 2;
            }
        }
        else if ((instr & 0xF000) == 0xA000){ // load constant into index register
            idx_register = instr & 0x0FFF;
        }
        else if ((instr & 0xF000) == 0xB000){ // jump with reg offset
            uint16_t fixed_jmp = instr & 0x0FFF;
            pc = fixed_jmp + regs[0];
            pc -= 2; // account for pc += 2 at end of loop

        }
        else if ((instr & 0xF000) == 0xC000){ // random number <= arg
            uint8_t r = instr & 0x0F00 >> 8;
            regs[r] = rand() & 0xFF;
        }
        else if ((instr & 0xF000) == 0xD000){ // draw sprite
            uint8_t x = regs[(instr & 0x0F00) >> 8];
            uint8_t y = regs[(instr & 0x00F0) >> 4];
            uint8_t s = instr & 0x000F;
            for(int cur_y = y; cur_y < y + s; cur_y++){
                uint8_t byte = (buffer[idx_register+cur_y-y]);
                for(int cur_x = x; cur_x < x + 8; cur_x++){
                    uint8_t bit = !!(byte & (128 >> (cur_x-x)));
                    screen[cur_y%HEIGHT][cur_x%WIDTH] ^= bit;
                }
            }
        }
        else if ((instr & 0xF000) == 0xE000){ // keyboard inputs
        }
        else if ((instr & 0xF000) == 0xF000){ // various random things
            uint8_t n_regs;
            uint8_t reg;
            switch(instr & 0x00FF){
                case(0x07): // get delay timer into vr
                    regs[(instr & 0x0F00) >> 8] = delay;
                    break;
                case(0x0a): // wait for for keypress,put key in register vr	
                    printf("NOT IMPLEMENTED!!\n");
                    break;
                case(0x15): // set the delay timer to vr
                    delay = regs[(instr & 0x0F00) >> 8];
                    break;
                case(0x18): // set the sound timer to vr
                    sound = regs[(instr & 0x0F00) >> 8];
                    break;
                case(0x1e): // add register vr to the index register
                    reg = regs[(instr & 0x0F00) >> 8];
                    idx_register += reg;
                    break;
                case(0x29): // point I to the sprite for hexadecimal character in vr
                    idx_register = regs[(instr & 0x0F00) >> 8] * 5; // 5 is because each char is 5 rows tall.
                    break;
                case(0x33): // store the bcd representation of register vr at location I,I+1,I+2
                    reg = regs[(instr & 0x0F00) >> 8];
                    buffer[idx_register] = reg / 100;
                    buffer[idx_register+1] = (reg / 10) % 10;
                    buffer[idx_register+2] = reg % 10;
                    break;
                case(0x55): // store registers v0-vr at location I onwards
                    n_regs = (instr & 0x0F00) >> 8;
                    for(int i = 0; i <= n_regs; i++){
                        buffer[idx_register++] = regs[i];
                    }
                    break;
                case(0x65): // load registers v0-vr from location I onwards	
                    n_regs = (instr & 0x0F00) >> 8;
                    for(int i = 0; i <= n_regs; i++){
                        regs[i] = buffer[idx_register++];
                    }
                    break;
            }
        }

    }

    printf("%x\n", buffer[0x200]);
    fclose(fp);
}