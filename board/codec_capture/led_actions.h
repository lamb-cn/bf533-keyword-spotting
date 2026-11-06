#ifndef BOARD_LED_ACTIONS_H
#define BOARD_LED_ACTIONS_H

enum LedAction {
 LED_NONE, LED_FOUR, LED_FIVE, LED_SIX, LED_SEVEN, LED_EIGHT, LED_NINE,
 LED_GO, LED_STOP, LED_LEFT, LED_RIGHT, LED_OFF
};

typedef struct {
 unsigned selected, mask, running, start_block;
} LedState;

/* A 20 ms audio block is the time base. Advance every six blocks (120 ms). */
static unsigned led_display(const LedState *state, unsigned block){
 if(!state->running)return state->mask;
 return 1u<<((state->selected+(block-state->start_block)/6u)%6u);
}

static void led_command(LedState *state, enum LedAction action, unsigned block){
 switch(action){
 case LED_NONE:return;
 case LED_GO:state->start_block=block;state->running=1;return;
 case LED_STOP:state->running=0;return;
 case LED_OFF:state->running=0;state->mask=0;return;
 case LED_LEFT:state->selected=(state->selected+5u)%6u;break;
 case LED_RIGHT:state->selected=(state->selected+1u)%6u;break;
 default:state->selected=(unsigned)(action-LED_FOUR);break;
 }
 state->running=0;
 state->mask^=1u<<state->selected;
}
#endif
