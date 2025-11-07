#include <stdbool.h>
#ifndef SIGNALS_H
#define SIGNALS_H

// new bool declaration:
bool process_signal_events(void);

void install_signal_handlers(void);

#endif
