#!/bin/bash
export DISPLAY=:0
export XAUTHORITY=/run/user/1000/gdm/Xauthority
cd "$(dirname "$0")"
./alibaba-cloud-spend
