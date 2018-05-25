#!/bin/sh
config=\
"[general]
terminal=xterm
refresh_timeout=300"

ttv=\
"saltybet
monstercat"

q_ytdl=\
"best[height <=? 720][tbr <=? 2500]/best
best[height <=? 480][tbr <=? 2250]/best
best
bestaudio"

path=""
if [ -n "${XDG_CONFIG_HOME+set}" ]; then
        path="${XDG_CONFIG_HOME}/cttv"
else
        path="${HOME}/.config/cttv"
fi
mkdir -p "$path"
echo "$config" >> "$path/config"
echo "$ttv" >> "$path/cfg_twitch"
echo "$q_ytdl" >> "$path/q_ytdl"
