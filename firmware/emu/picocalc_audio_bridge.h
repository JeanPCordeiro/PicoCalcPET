#ifndef PICOCALC_TRS_AUDIO_BRIDGE_H
#define PICOCALC_TRS_AUDIO_BRIDGE_H

#include "trs.h"

void picocalc_audio_bridge_reset(void);
void picocalc_audio_bridge_set_cassette_motor(int value);
void picocalc_audio_bridge_cassette_out(int value);
int picocalc_audio_bridge_cassette_in(void);
void picocalc_audio_bridge_sound_out(int value);
void picocalc_audio_bridge_orch90_out(int channels, int value);
void picocalc_audio_bridge_orch90_flush(void);
void picocalc_audio_bridge_cassette_update(void);
void picocalc_audio_bridge_cassette_kickoff(void);

#endif
