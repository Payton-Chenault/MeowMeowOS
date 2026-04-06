#include "beep.h"

#include "../../intf/speaker/speaker.h"
#include "../../kernel_services/kernel_services.h"

void beep(uint32_t freq, uint32_t duration_ms) {
    play_sound(freq);
    ksleep(duration_ms);
    no_sound();
}