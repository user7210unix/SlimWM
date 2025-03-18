#!/bin/bash
WORKSPACE=$1
CURRENT=$(cat ~/.current_workspace 2>/dev/null || echo "1")

# Tag the active window with the target workspace
ACTIVE=$(xdotool getactivewindow)
xprop -id "$ACTIVE" -f _WIND_WORKSPACE 8i -set _WIND_WORKSPACE "$WORKSPACE"

# If moving to a different workspace, hide the window
if [ "$WORKSPACE" != "$CURRENT" ]; then
    xdotool windowunmap "$ACTIVE"
fi
