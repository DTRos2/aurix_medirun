# Audio command type
uint8 TYPE_TTS=0
uint8 TYPE_EFFECT=1
uint8 TYPE_TTS_AND_EFFECT=2
uint8 TYPE_STOP=3

uint8 type

# TTS text
string text

# Effect sound id
# examples:
# start, waypoint, goal, warning, error
string sound_id

# 0.0 ~ 1.0
float32 volume

# repeat count
uint8 repeat
