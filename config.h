#pragma once

#include <iostream>
#include <string>
#include <list>
#include <unordered_map>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xcursor/Xcursor.h>

#define MODIFIER Mod4Mask

#define BUTTON_FOCUS 1

#define NUM_WORKSPACES 4

#define KEY_KILL_WM XK_e

#define KEY_CLOSE XK_q

#define KEY_OPEN_LAUNCHER XK_d
#define CMD_LAUNCHER "rofi -show run"

#define KEY_OPEN_TERMINAL XK_Return
#define CMD_TERMINAL "st"

#define DEFAULT_MASTER_SIZE 1000

#define KEY_GROW_MASTER XK_bracketright
#define KEY_SHRINK_MASTER XK_bracketleft
#define PX_INCREMENT_MASTER 50

