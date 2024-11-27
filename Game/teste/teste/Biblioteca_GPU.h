#ifndef BIBLIOTECA_GPU_H
#define BIBLIOTECA_GPU_H
#include <unistd.h>

extern void createMappingMemory(void);
extern void sendInstruction(uint32_t dataA, uint32_t dataB);
extern void set_background_block(uint32_t column, uint32_t line, uint32_t r, uint32_t g, uint32_t b);
extern void isFull(void);
extern void setPolygon(int address, int opcode, int color, int form, int mult, int ref_point_x, int ref_point_y);
extern void set_sprite(int registrador, int x, int y, int offset, int activation_bit);
extern void set_background_color(int R, int G, int B);
extern uint32_t open_button(void);

#endif