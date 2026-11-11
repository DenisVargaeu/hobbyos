#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

int keyboard_pop_char(char *character);
void keyboard_irq_handler(void);

#endif