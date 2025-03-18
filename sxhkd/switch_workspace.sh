#!/bin/bash
WORKSPACE=$1

# Store the current workspace in a file
echo "$WORKSPACE" > ~/.current_workspace

# Hide all windows not tagged with the current workspace
for win in $(wmctrl -l | awk '{print $1}'); do
    TAG=$(xprop -id "$win" _WIND_WORKSPACE | grep -o '[0-8]' || echo "0")
    if [ "$TAG" = "$WORKSPACE" ] || [ "$TAG" = "0" ]; then
        xdotool windowmap "$win"
    else
        xdotool windowunmap "$win"
    fi
done
