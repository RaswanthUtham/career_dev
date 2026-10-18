-#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>

int main() {
    uint16_t capacity = 4;
    uint16_t *samples = (uint16_t*) malloc (4 * sizeof(uint16_t));
    samples[0] = 23;
    samples[1] = 33;
    samples[2] = 44;
    samples[3] = 55;
    uint16_t selected_index = 1;
    uint16_t *selected = &samples[selected_index];
    uint16_t *extended = realloc(samples, 8 * sizeof(uint16_t));
    if(extended) {
        samples = extended;
        selected = &samples[selected_index];
        capacity = 8;
    }
    free(samples);
}
