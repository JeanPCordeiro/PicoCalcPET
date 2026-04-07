#include "trs.h"
#include "trs_keyboard_internal.h"
#include "platform/platform.h"

#define KEY_QUEUE_SIZE 32
#define STRETCH_AMOUNT 4000

#define TK(a, b) (((a) << 4) + (b))
#define TK_ADDR(tk) (((tk) >> 4) & 0xf)
#define TK_DATA(tk) ((tk) & 0xf)
#define TK_DOWN(tk) (((tk) & 0x10000) == 0)

#define TK_AtSign TK(0, 0)
#define TK_A TK(0, 1)
#define TK_B TK(0, 2)
#define TK_C TK(0, 3)
#define TK_D TK(0, 4)
#define TK_E TK(0, 5)
#define TK_F TK(0, 6)
#define TK_G TK(0, 7)
#define TK_H TK(1, 0)
#define TK_I TK(1, 1)
#define TK_J TK(1, 2)
#define TK_K TK(1, 3)
#define TK_L TK(1, 4)
#define TK_M TK(1, 5)
#define TK_N TK(1, 6)
#define TK_O TK(1, 7)
#define TK_P TK(2, 0)
#define TK_Q TK(2, 1)
#define TK_R TK(2, 2)
#define TK_S TK(2, 3)
#define TK_T TK(2, 4)
#define TK_U TK(2, 5)
#define TK_V TK(2, 6)
#define TK_W TK(2, 7)
#define TK_X TK(3, 0)
#define TK_Y TK(3, 1)
#define TK_Z TK(3, 2)
#define TK_LeftBracket TK(3, 3)
#define TK_Backslash TK(3, 4)
#define TK_RightBracket TK(3, 5)
#define TK_Caret TK(3, 6)
#define TK_Underscore TK(3, 7)
#define TK_0 TK(4, 0)
#define TK_1 TK(4, 1)
#define TK_2 TK(4, 2)
#define TK_3 TK(4, 3)
#define TK_4 TK(4, 4)
#define TK_5 TK(4, 5)
#define TK_6 TK(4, 6)
#define TK_7 TK(4, 7)
#define TK_8 TK(5, 0)
#define TK_9 TK(5, 1)
#define TK_Colon TK(5, 2)
#define TK_Semicolon TK(5, 3)
#define TK_Comma TK(5, 4)
#define TK_Minus TK(5, 5)
#define TK_Period TK(5, 6)
#define TK_Slash TK(5, 7)
#define TK_Enter TK(6, 0)
#define TK_Clear TK(6, 1)
#define TK_Break TK(6, 2)
#define TK_Up TK(6, 3)
#define TK_Down TK(6, 4)
#define TK_Left TK(6, 5)
#define TK_Right TK(6, 6)
#define TK_Space TK(6, 7)
#define TK_LeftShift TK(7, 0)
#define TK_RightShift TK(7, 1)

#define TK_NULL TK(9, 0)
#define TK_Neutral TK(9, 1)
#define TK_ForceShift TK(9, 2)
#define TK_ForceNoShift TK(9, 3)
#define TK_ForceShiftPersistent TK(9, 4)
#define TK_AllKeysUp TK(9, 5)

typedef struct {
    int bit_action;
    int shift_action;
} key_table_t;

static const int alpha_bit_actions[26] = {
    TK_A, TK_B, TK_C, TK_D, TK_E, TK_F, TK_G,
    TK_H, TK_I, TK_J, TK_K, TK_L, TK_M, TK_N,
    TK_O, TK_P, TK_Q, TK_R, TK_S, TK_T, TK_U,
    TK_V, TK_W, TK_X, TK_Y, TK_Z
};

static int key_queue[KEY_QUEUE_SIZE];
static int key_queue_head;
static int key_queue_entries;
static int keystate[9];
static int force_shift = TK_Neutral;
static int keys_down;
static int key_heartbeat;
static tstate_t key_stretch_timeout;
static int tap_bit_action = TK_NULL;
static int tap_shift_action = TK_Neutral;
static int tap_heartbeat_budget;

int stretch_amount = STRETCH_AMOUNT;
int trs_kb_bracket_state;
int trs_joystick;

enum {
    SYNTHETIC_TAP_HEARTBEATS = 6
};

static void queue_key(int state)
{
    key_queue[(key_queue_head + key_queue_entries) % KEY_QUEUE_SIZE] = state;
    if (key_queue_entries < KEY_QUEUE_SIZE) {
        key_queue_entries++;
    }
}

static int dequeue_key(void)
{
    int rval = -1;

    if (key_queue_entries > 0) {
        rval = key_queue[key_queue_head];
        key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
        key_queue_entries--;
    }

    return rval;
}

static void change_keystate(int action)
{
    switch (action) {
    case TK_AllKeysUp:
        for (int i = 0; i < 8; ++i) {
            keystate[i] = 0;
        }
        force_shift = TK_Neutral;
        keys_down = 0;
        z80_state.keypress = 0;
        break;
    case TK_Neutral:
    case TK_ForceShift:
    case TK_ForceNoShift:
    case TK_ForceShiftPersistent:
        force_shift = action;
        break;
    default: {
        int key_down = TK_DOWN(action);
        int was_down = (keystate[TK_ADDR(action)] & (1 << TK_DATA(action))) != 0;

        if (key_down != was_down) {
            if (key_down) {
                keystate[TK_ADDR(action)] |= (1 << TK_DATA(action));
                keys_down++;
            } else {
                keystate[TK_ADDR(action)] &= ~(1 << TK_DATA(action));
                keys_down--;
            }
            z80_state.keypress = keys_down ? 1 : 0;
        }
    } break;
    }
}

static key_table_t lookup_keysym(int keysym)
{
    switch (keysym) {
    case PLATFORM_KEY_UP:
        return (key_table_t){TK_Up, TK_Neutral};
    case PLATFORM_KEY_DOWN:
        return (key_table_t){TK_Down, TK_Neutral};
    case PLATFORM_KEY_LEFT:
        return (key_table_t){TK_Left, TK_Neutral};
    case PLATFORM_KEY_RIGHT:
        return (key_table_t){TK_Right, TK_Neutral};
    case PLATFORM_KEY_TAB:
        return (key_table_t){TK_Right, TK_Neutral};
    case PLATFORM_KEY_ESC:
        return (key_table_t){TK_Break, TK_Neutral};
    case PLATFORM_KEY_BREAK:
        return (key_table_t){TK_Break, TK_Neutral};
    case PLATFORM_KEY_CLEAR:
        return (key_table_t){TK_Clear, TK_Neutral};
    case PLATFORM_KEY_BACKSPACE:
        return (key_table_t){TK_Left, TK_Neutral};
    case PLATFORM_KEY_ENTER:
        return (key_table_t){TK_Enter, TK_Neutral};
    case PLATFORM_KEY_HOME:
        return (key_table_t){TK_Left, TK_Neutral};
    case PLATFORM_KEY_END:
        return (key_table_t){TK_Right, TK_Neutral};
    case PLATFORM_KEY_PAGE_UP:
        return (key_table_t){TK_Up, TK_Neutral};
    case PLATFORM_KEY_PAGE_DOWN:
        return (key_table_t){TK_Down, TK_Neutral};
    case PLATFORM_KEY_F1:
        return (key_table_t){TK_Break, TK_Neutral};
    case PLATFORM_KEY_F2:
        return (key_table_t){TK_Clear, TK_Neutral};
    case PLATFORM_KEY_F3:
        return (key_table_t){TK_LeftBracket, TK_ForceNoShift};
    case PLATFORM_KEY_F4:
        return (key_table_t){TK_RightBracket, TK_ForceNoShift};
    default:
        break;
    }

    switch (keysym & 0xffff) {
    case 0x08: return (key_table_t){TK_Left, TK_Neutral};
    case 0x0c: return (key_table_t){TK_Clear, TK_Neutral};
    case 0x0d: return (key_table_t){TK_Enter, TK_Neutral};
    case 0x1b: return (key_table_t){TK_Break, TK_Neutral};
    case 0x20: return (key_table_t){TK_Space, TK_Neutral};
    case '!': return (key_table_t){TK_1, TK_ForceShift};
    case '"': return (key_table_t){TK_2, TK_ForceShift};
    case '#': return (key_table_t){TK_3, TK_ForceShift};
    case '$': return (key_table_t){TK_4, TK_ForceShift};
    case '%': return (key_table_t){TK_5, TK_ForceShift};
    case '&': return (key_table_t){TK_6, TK_ForceShift};
    case '\'': return (key_table_t){TK_7, TK_ForceShift};
    case '(': return (key_table_t){TK_8, TK_ForceShift};
    case ')': return (key_table_t){TK_9, TK_ForceShift};
    case '*': return (key_table_t){TK_Colon, TK_ForceShift};
    case '+': return (key_table_t){TK_Semicolon, TK_ForceShift};
    case ',': return (key_table_t){TK_Comma, TK_ForceNoShift};
    case '-': return (key_table_t){TK_Minus, TK_ForceNoShift};
    case '.': return (key_table_t){TK_Period, TK_ForceNoShift};
    case '/': return (key_table_t){TK_Slash, TK_ForceNoShift};
    case '0': return (key_table_t){TK_0, TK_ForceNoShift};
    case '1': return (key_table_t){TK_1, TK_ForceNoShift};
    case '2': return (key_table_t){TK_2, TK_ForceNoShift};
    case '3': return (key_table_t){TK_3, TK_ForceNoShift};
    case '4': return (key_table_t){TK_4, TK_ForceNoShift};
    case '5': return (key_table_t){TK_5, TK_ForceNoShift};
    case '6': return (key_table_t){TK_6, TK_ForceNoShift};
    case '7': return (key_table_t){TK_7, TK_ForceNoShift};
    case '8': return (key_table_t){TK_8, TK_ForceNoShift};
    case '9': return (key_table_t){TK_9, TK_ForceNoShift};
    case ':': return (key_table_t){TK_Colon, TK_ForceNoShift};
    case ';': return (key_table_t){TK_Semicolon, TK_ForceNoShift};
    case '<': return (key_table_t){TK_Comma, TK_ForceShift};
    case '=': return (key_table_t){TK_Minus, TK_ForceShift};
    case '>': return (key_table_t){TK_Period, TK_ForceShift};
    case '?': return (key_table_t){TK_Slash, TK_ForceShift};
    case '@': return (key_table_t){TK_AtSign, TK_ForceNoShift};
    case '[': return (key_table_t){TK_LeftBracket, TK_ForceNoShift};
    case '\\': return (key_table_t){TK_Backslash, TK_ForceNoShift};
    case ']': return (key_table_t){TK_RightBracket, TK_ForceNoShift};
    case '^': return (key_table_t){TK_Caret, TK_ForceNoShift};
    case '_': return (key_table_t){TK_Underscore, TK_ForceNoShift};
    case '`': return (key_table_t){TK_AtSign, TK_ForceShift};
    case '{': return (key_table_t){TK_LeftBracket, TK_ForceShift};
    case '|': return (key_table_t){TK_Backslash, TK_ForceShift};
    case '}': return (key_table_t){TK_RightBracket, TK_ForceShift};
    case '~': return (key_table_t){TK_Caret, TK_ForceShift};
    default:
        if (keysym >= 'A' && keysym <= 'Z') {
            return (key_table_t){alpha_bit_actions[keysym - 'A'], TK_ForceShift};
        }
        if (keysym >= 'a' && keysym <= 'z') {
            return (key_table_t){alpha_bit_actions[keysym - 'a'], TK_ForceNoShift};
        }
        break;
    }

    return (key_table_t){TK_NULL, TK_Neutral};
}

void trs_kb_reset(void)
{
    key_queue_head = 0;
    key_queue_entries = 0;
    for (int i = 0; i < 9; ++i) {
        keystate[i] = 0;
    }
    force_shift = TK_Neutral;
    keys_down = 0;
    key_heartbeat = 0;
    key_stretch_timeout = z80_state.t_count;
    tap_bit_action = TK_NULL;
    tap_shift_action = TK_Neutral;
    tap_heartbeat_budget = 0;
    z80_state.keypress = 0;
}

void trs_kb_heartbeat(void)
{
    key_heartbeat++;

    if (tap_bit_action != TK_NULL) {
        if (tap_heartbeat_budget > 0) {
            tap_heartbeat_budget--;
        }
        if (tap_heartbeat_budget <= 0) {
            tap_bit_action = TK_NULL;
            tap_shift_action = TK_Neutral;
            z80_state.keypress = 0;
        } else {
            z80_state.keypress = turbo_mode ? turbo_mode : 1;
        }
    }
}

void trs_kb_bracket(int shifted)
{
    trs_kb_bracket_state = shifted;
}

int trs_kb_mem_read(int address)
{
    int key = -1;
    int data = 0;

    if (key_heartbeat > 2) {
        do {
            key = dequeue_key();
            if (key >= 0) {
                change_keystate(key);
            }
        } while (key >= 0);
    }

    if (key_stretch_timeout - z80_state.t_count > TSTATE_T_MID) {
        key = dequeue_key();
        key_stretch_timeout = z80_state.t_count + stretch_amount;
    }

    if (key >= 0) {
        change_keystate(key);
    }

    key_heartbeat = 0;

    for (int bit = 0; bit < 7; ++bit) {
        if (address & (1 << bit)) {
            data |= keystate[bit];
        }
    }

    if (address & 0x80) {
        int tmp = keystate[7];

        if (force_shift == TK_ForceNoShift) {
            tmp &= ~3;
        } else if (force_shift != TK_Neutral && (tmp & 3) == 0) {
            tmp |= 1;
        }

        if (tap_shift_action == TK_ForceNoShift) {
            tmp &= ~3;
        } else if (tap_shift_action != TK_Neutral && (tmp & 3) == 0) {
            tmp |= 1;
        }
        data |= tmp;
    }

    if (tap_bit_action != TK_NULL) {
        int tap_addr = TK_ADDR(tap_bit_action);
        int tap_data = TK_DATA(tap_bit_action);

        if (tap_addr < 7) {
            if (address & (1 << tap_addr)) {
                data |= (1 << tap_data);
            }
        } else if (tap_addr == 7 && (address & 0x80)) {
            data |= (1 << tap_data);
        }
    }

    return data;
}

void trs_xlate_keysym(int keysym)
{
    int key_down;
    key_table_t kt;
    static int shift_action = TK_Neutral;

    if (keysym == 0x10000) {
        queue_key(TK_AllKeysUp);
        shift_action = TK_Neutral;
        return;
    }

    key_down = (keysym & 0x10000) == 0;
    kt = lookup_keysym(keysym);
    if (kt.bit_action == TK_NULL) {
        return;
    }

    if (key_down) {
        if (shift_action != TK_ForceShiftPersistent && shift_action != kt.shift_action) {
            shift_action = kt.shift_action;
            queue_key(shift_action);
        }
        queue_key(kt.bit_action);
    } else {
        queue_key(kt.bit_action | 0x10000);
        if (shift_action != TK_Neutral && shift_action == kt.shift_action) {
            shift_action = TK_Neutral;
            queue_key(shift_action);
        }
    }
}

void clear_key_queue(void)
{
    key_queue_head = 0;
    key_queue_entries = 0;
}

void trs_key_event(int keysym)
{
    key_table_t kt = lookup_keysym(keysym);

    if (kt.bit_action == TK_NULL) {
        return;
    }

    tap_bit_action = kt.bit_action;
    tap_shift_action = kt.shift_action;
    tap_heartbeat_budget = SYNTHETIC_TAP_HEARTBEATS;
    z80_state.keypress = turbo_mode ? turbo_mode : 1;
}

int trs_joystick_in(void)
{
    return 0xff;
}
