typedef enum {
    UP    = 'U',
    DOWN  = 'D',
    LEFT  = 'L',
    RIGHT = 'R'
} direction_t;

void esc_init(void);
void esc_set_us(ledc_channel_t ch, int us);
void driver(direction_t input);