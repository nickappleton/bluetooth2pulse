#include "ws2812.pio.h"

#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"

#ifdef BT2P_ENABLE_DEBUG
#include <stdio.h>
#include "pico/stdlib.h"
#define usbdebug_init() (stdio_init_all())
#define usbdebug_printf(...) (printf(__VA_ARGS__))
#else
#define usbdebug_init() ((void)0)
#define usbdebug_printf(...) ((void)0)
#endif

/* Hardware GPIO routings */
#define BT2P_PWM_HF_GPIO     (3)
#define BT2P_PWM_HF_SLICE    (1) // 1B = pwm_gpio_to_slice_num(3);
#define BT2P_PWM_HF_CHAN     (PWM_CHAN_B)
#define BT2P_PWM_LF_GPIO     (4)
#define BT2P_PWM_LF_SLICE    (2) // 2A =  pwm_gpio_to_slice_num(4);
#define BT2P_PWM_LF_CHAN     (PWM_CHAN_A)
#define BT2P_AG1171_FR_GPIO  (29)
#define BT2P_AG1171_RM_GPIO  (28)
#define BT2P_AG1171_SHK_GPIO (27)




struct tonegen_state {
#define TONE_OFF      (1)
#define TONE_DIALTONE (2)
#define TONE_HANGUP   (3)
    int      state;
    int      timepos; /* increment every 100ms */

};

static void tonegen_init(struct tonegen_state *p_tonegen) {
    // Get some sensible defaults for the slice configuration. By default, the
    // counter is allowed to wrap over its maximum range (0 to 2**16-1)
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int_frac4(&config, 16, 2); /* divide clock by 258/16 approx= 7751938 */
    /* Configure the PWMs we are using with the clock divider we want. */
    pwm_init(BT2P_PWM_HF_SLICE, &config, true);
    pwm_init(BT2P_PWM_LF_SLICE, &config, true);
    /* Setup GPIO to function as PWM */
    gpio_set_function(BT2P_PWM_HF_GPIO, GPIO_FUNC_PWM);
    gpio_set_function(BT2P_PWM_LF_GPIO, GPIO_FUNC_PWM);
    /* Setup periods */
    pwm_set_wrap(BT2P_PWM_HF_SLICE, 18984-1); /* 7751938/18984 approx= 408.34 Hz */
    pwm_set_wrap(BT2P_PWM_LF_SLICE, 19792-1); /* 7751938/18984 approx= 391.67 Hz */
    /* Setup duty cycles to 50% */
    pwm_set_chan_level(BT2P_PWM_HF_SLICE, BT2P_PWM_HF_CHAN, 18984/2);
    pwm_set_chan_level(BT2P_PWM_LF_SLICE, BT2P_PWM_LF_CHAN, 19792/2);
}

static void tonegen_update(struct tonegen_state *p_state) {
    if (p_state->state == TONE_HANGUP) {
        if (p_state->timepos == 5) {
            pwm_set_enabled(BT2P_PWM_HF_SLICE, false);
        } else if (p_state->timepos == 0) {
            pwm_set_enabled(BT2P_PWM_HF_SLICE, true);
        }
        if (++p_state->timepos >= 10) {
            p_state->timepos = 0;
        }
    }
}

static void tonegen_set_state(struct tonegen_state *p_state, int new_state) {
    if (p_state->state != new_state) {
        if (new_state == TONE_DIALTONE) {
            pwm_set_enabled(BT2P_PWM_HF_SLICE, true);
            pwm_set_enabled(BT2P_PWM_LF_SLICE, true);
        } else if (new_state == TONE_HANGUP) {
            pwm_set_enabled(BT2P_PWM_HF_SLICE, true);
            pwm_set_enabled(BT2P_PWM_LF_SLICE, false);
        } else {
            pwm_set_enabled(BT2P_PWM_HF_SLICE, false);
            pwm_set_enabled(BT2P_PWM_LF_SLICE, false);
        }
        p_state->timepos = 0;
        p_state->state   = new_state;
    }
}


void on_uart_rx() {
    while (uart_is_readable(uart1)) {
        uint8_t ch = uart_getc(uart1);
        // Can we send it back?
        //putchar(ch);
        // if (uart_is_writable(uart1)) {
        //     // Change it slightly first!
        //     //ch++;
        // }
        //chars_rxed++;
    }
}

void init_bluetooth(void) {
    uart_init(uart1, 2400);

    gpio_set_function(5, GPIO_FUNC_UART);
    gpio_set_function(20, GPIO_FUNC_UART);

    (void)uart_set_baudrate(uart1, 115200);
    uart_set_hw_flow(uart1, false, false);
    uart_set_format(uart1, 8, 1, UART_PARITY_NONE);

    /* maybe needs to be false? */
    uart_set_fifo_enabled(uart1, true);

    irq_set_exclusive_handler(UART1_IRQ, on_uart_rx);
    irq_set_enabled(UART1_IRQ, true);
    uart_set_irq_enables(uart1, true, false);
}

volatile int shk_state_counter = 0;
volatile int shk_current_state = 0;

void bluetooth2phone_gpio_callback(uint gpio, uint32_t events) {
    if (gpio == BT2P_AG1171_SHK_GPIO) {
        shk_state_counter = 0;
        /* events never seems to change and is the mask the interrupt was
         * configured with? Surely I am doing something wrong... but in
         * the mean time, just read the value. */
        shk_current_state = gpio_get(BT2P_AG1171_SHK_GPIO);
    }
}

/* call every 5ms */
int process_ag1171(int b_ring_if_on_hook) {
    static int      b_is_ringing = 0;
    static unsigned time_step    = 0;
    static int      b_shk_state  = 0;

    /* 25ms of debouncing seems enough... */
    if (shk_state_counter++ >= 5) {
        shk_state_counter = 5;
        b_shk_state       = shk_current_state;
    }

    if (b_is_ringing) {
        /* t=0   rm is high  fr is high
         * t=4   rm is high  fr goes low
         * t=199 rm is high  fr is low
         * t=200 rm is high  fr goes high
         * t=204 rm goes low fr is high */
        int rm_is_high = (time_step < 204);
        int fr_is_low = ((time_step & 4) == 0) && rm_is_high;
        
        gpio_put(BT2P_AG1171_FR_GPIO, !fr_is_low);
        gpio_put(BT2P_AG1171_RM_GPIO, rm_is_high);

        if (b_shk_state || !b_ring_if_on_hook) {
            /* if fr is high and either rm is low or fr has been high for more than 10ms, we can stop. */
            if  (!fr_is_low && (!rm_is_high || (time_step & 0x7 == 0x7))) {
                b_is_ringing = 0;
            }
        }

        if (++time_step >= 800)
            time_step = 0;
    } else {
        b_is_ringing = b_ring_if_on_hook;
        time_step    = 0;
    }

    return b_shk_state;
}

void ag1171_init(void) {
    /* SETUP F/R */
    gpio_set_dir(BT2P_AG1171_FR_GPIO, GPIO_OUT);
    gpio_put(BT2P_AG1171_FR_GPIO, 1);
    gpio_set_function(BT2P_AG1171_FR_GPIO, GPIO_FUNC_SIO);

    /* SETUP RM */
    gpio_set_dir(BT2P_AG1171_RM_GPIO, GPIO_OUT);
    gpio_put(BT2P_AG1171_RM_GPIO, 0);
    gpio_set_function(BT2P_AG1171_RM_GPIO, GPIO_FUNC_SIO);

    /* SETUP SHK */
    gpio_set_dir(BT2P_AG1171_SHK_GPIO, GPIO_IN);
    gpio_put(BT2P_AG1171_SHK_GPIO, 0);
    gpio_set_function(BT2P_AG1171_SHK_GPIO, GPIO_FUNC_SIO);
    gpio_set_irq_enabled_with_callback(BT2P_AG1171_SHK_GPIO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &bluetooth2phone_gpio_callback);
}


void main() {
    unsigned tick = 0;
    struct tonegen_state tonegen;

    usbdebug_init();

    //init_bluetooth();
    ag1171_init();

    ws2812_program_init
        (pio0
        ,0 /* use state machine 0 */
        ,pio_add_program(pio0, &ws2812_program)
        ,PICO_DEFAULT_WS2812_PIN
        ,800*1000
        ,false
        );

    /* Turn on the LED */
    gpio_init(PICO_DEFAULT_WS2812_POWER_PIN);
    gpio_set_dir(PICO_DEFAULT_WS2812_POWER_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_WS2812_POWER_PIN, 1);

    tonegen_init(&tonegen);

    while (true) {
        sleep_ms(5);

        if(shk_current_state) {
            tonegen_set_state(&tonegen, TONE_HANGUP);
            process_ag1171(0);
            
        } else {
            tonegen_set_state(&tonegen, TONE_OFF);
            process_ag1171(0);
        }

        if (tick % 20 == 0) {
            /* update tonegen every 100ms */
            tonegen_update(&tonegen);
        }

        if (shk_current_state) {
            if (tick == 0) {
                pio_sm_put_blocking(pio0, 0, 0xFFFFFF00); /* Green + Red + Blue */
            }
        } else {
            if (tick == 0) {
                pio_sm_put_blocking(pio0, 0, 0x00FFFF00); /* Red + Blue */
            }
            if (tick == 100) {
                pio_sm_put_blocking(pio0, 0, 0xFFFF0000); /* Green + Red */
            }
        }

      

        if (++tick >= 200) {
            tick = 0;
        }
    }
}
