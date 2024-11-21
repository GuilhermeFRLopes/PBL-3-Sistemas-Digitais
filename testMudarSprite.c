#include <stdio.h>

int main() {
    createMappingMemory();

    for (int i = 0; i < 20; i++){
        
        for (int j = 0; j < 20; j++){
            set_pixelSprite(0 + i + j*20, 0b0001);
        }
    }

    set_sprite(1, 220, 100, 0 , 0);
}
