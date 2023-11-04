#pragma once

#define STATUS_NOTE_OFF 0x08
#define STATUS_NOTE_ON 0x09
#define STATUS_CC 0x0B

#define TRACK_1 0x00
#define TRACK_2 0x01
#define TRACK_3 0x02
#define TRACK_4 0x03
#define TRACK_5 0x04
#define TRACK_6 0x05
#define TRACK_7 0x06
#define TRACK_8 0x07
#define TRACK_MASTER 0x08

#define LED_RECORD 0x30
#define LED_SOLO 0x31
#define LED_ACTIVATOR 0x32
#define LED_TRACK_SELECT 0x33
#define LED_CLIP_STOP 0x34
#define LED_CLIP_LAUNCH_1 0x35
#define LED_CLIP_LAUNCH_2 0x36
#define LED_CLIP_LAUNCH_3 0x37
#define LED_CLIP_LAUNCH_4 0x38
#define LED_CLIP_LAUNCH_5 0x39

#define LED_CLIP_TRACK 0x3A
#define LED_DEVICE_ON_OFF 0x3B
#define LED_LEFT 0x3C
#define LED_RIGHT 0x3D
#define LED_DETAIL_VIEW 0x3E
#define LED_REQ_QUANT 0x3F
#define LED_MID_OVERDUB 0x40
#define LED_METRONOME 0x41

#define LED_MASTER 0x50

#define LED_STOP_ALL_CLIPS 0x51

#define LED_SCENE_LAUNCH_1 0x52
#define LED_SCENE_LAUNCH_2 0x53
#define LED_SCENE_LAUNCH_3 0x54
#define LED_SCENE_LAUNCH_4 0x55
#define LED_SCENE_LAUNCH_5 0x56

#define LED_PAN 0x57
#define LED_SEND_A 0x58
#define LED_SEND_B 0x59
#define LED_SEND_C 0x5A

#define BTN_PLAY 0x5B
#define BTN_STOP 0x5C
#define BTN_RECORD 0x5D
#define BTN_UP 0x5E
#define BTN_DOWN 0x5F
#define BTN_RIGHT 0x60
#define BTN_LEFT 0x61
#define BTN_SHIFT 0x62
#define BTN_TAP_TEMPO 0x63
#define BTN_NUDGE_PLUS 0x64
#define BTN_NUDGE_MINUS 0x65

#define C_TRACK_LEVEL 0x07
#define C_MASTER_LEVEL 0x0E
#define C_CROSSFADER 0x0F

#define C_DEVICE_KNOB_1 0x10
#define C_DEVICE_KNOB_2 0x11
#define C_DEVICE_KNOB_3 0x12
#define C_DEVICE_KNOB_4 0x13
#define C_DEVICE_KNOB_5 0x14
#define C_DEVICE_KNOB_6 0x15
#define C_DEVICE_KNOB_7 0x16
#define C_DEVICE_KNOB_8 0x17

#define C_KNOB_NUM 8
#define CHAN_NUM 8

#define C_DEVICE_KNOB_RING_TYPE_1 0x18
#define C_DEVICE_KNOB_RING_TYPE_2 0x19
#define C_DEVICE_KNOB_RING_TYPE_3 0x1A
#define C_DEVICE_KNOB_RING_TYPE_4 0x1B
#define C_DEVICE_KNOB_RING_TYPE_5 0x1C
#define C_DEVICE_KNOB_RING_TYPE_6 0x1D
#define C_DEVICE_KNOB_RING_TYPE_7 0x1E
#define C_DEVICE_KNOB_RING_TYPE_8 0x1F

#define C_CUE_LEVEL 0x2F

#define C_TRACK_KNOB_1 0x30
#define C_TRACK_KNOB_2 0x31
#define C_TRACK_KNOB_3 0x32
#define C_TRACK_KNOB_4 0x33
#define C_TRACK_KNOB_5 0x34
#define C_TRACK_KNOB_6 0x35
#define C_TRACK_KNOB_7 0x36
#define C_TRACK_KNOB_8 0x37

#define C_TRACK_KNOB_RING_TYPE_1 0x38
#define C_TRACK_KNOB_RING_TYPE_2 0x39
#define C_TRACK_KNOB_RING_TYPE_3 0x3A
#define C_TRACK_KNOB_RING_TYPE_4 0x3B
#define C_TRACK_KNOB_RING_TYPE_5 0x3C
#define C_TRACK_KNOB_RING_TYPE_6 0x3D
#define C_TRACK_KNOB_RING_TYPE_7 0x3E
#define C_TRACK_KNOB_RING_TYPE_8 0x3F

#define LED_OFF 0x00
#define LED_ON 0x01
#define LED_BLINK 0x02
#define LED_GREEN 0x01
#define LED_GREEN_BLINK 0x02
#define LED_RED 0x03
#define LED_RED_BLINK 0x04
#define LED_YELLOW 0x05
#define LED_YELLOW_BLINK 0x06

#define RING_TYPE_SINGLE 0x01
#define RING_TYPE_VOLUME 0x02
#define RING_TYPE_PAN 0x03
