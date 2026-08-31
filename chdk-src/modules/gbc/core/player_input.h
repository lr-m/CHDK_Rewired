#ifndef PLAYER_INPUT_H
#define PLAYER_INPUT_H

/* CHDK port: see gbc_port.h */

struct player_input {
    bool button_left;
    bool button_right;
    bool button_up;
    bool button_down;
    bool button_a;
    bool button_b;
    bool button_start;
    bool button_select;

    bool special_quit;
    bool special_savestate;
    bool special_dbgbreak;
};

#endif
