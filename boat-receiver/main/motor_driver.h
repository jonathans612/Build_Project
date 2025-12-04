typedef enum {
    UP    = 'U',
    DOWN  = 'D',
    LEFT  = 'L',
    RIGHT = 'R'
} direction_t;

void esc_init(void);
void esc_set_us(int channel, int us);
void driver(direction_t input);