#pragma once

extern bool TrapSignal;

#define DEV_NULL_PATH "/dev/null"

void signals_init();
void signals_reset();
void signal_int_default();
bool from_jmp();
void trap_on();
void trap_off();
void setup_child(int child, int i, int f);
