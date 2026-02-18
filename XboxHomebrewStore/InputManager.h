#pragma once

#include "External.h"

typedef struct MousePosition
{
    float X;
    float Y;
} MousePosition;

typedef struct ControllerState
{
    float ThumbLeftDx;
    float ThumbLeftDy;
    float ThumbRightDx;
    float ThumbRightDy;
    bool Buttons[16];
} ControllerState;

typedef enum ControllerButton
{
    ControllerA = 0,
    ControllerB = 1,
    ControllerX = 2,
    ControllerY = 3,
    ControllerBlack = 4,
    ControllerWhite = 5,
    ControllerLTrigger = 6,
    ControllerRTrigger = 7,
    ControllerDpadUp = 8,
    ControllerDpadDown = 9,
    ControllerDpadLeft = 10,
    ControllerDpadRight = 11,
    ControllerStart = 12,
    ControllerBack = 13,
    ControllerLThumb = 14,
    ControllerRThumb = 15,
} ControllerButton;

typedef struct RemoteState
{
    bool Buttons[44];
} RemoteState;

typedef enum RemoteButton
{
    RemoteDisplay = 0,
    RemoteReverse = 1,
    RemotePlay = 2,
    RemoteForward = 3,
    RemoteSkipMinus = 4,
    RemoteStop = 5,
    RemotePause = 6,
    RemoteSkipPlus = 7,
    RemoteTitle = 8,
    RemoteInfo = 9,
    RemoteUp = 10,
    RemoteDown = 11,
    RemoteLeft = 12,
    RemoteRight = 13,
    RemoteSelect = 14,
    RemoteMenu = 15,
    RemoteBack = 16,
    Remote1 = 17,
    Remote2 = 18,
    Remote3 = 19,
    Remote4 = 20,
    Remote5 = 21,
    Remote6 = 22,
    Remote7 = 23,
    Remote8 = 24,
    Remote9 = 25,
    Remote0 = 26,
    RemoteMcePower = 27,
    RemoteMceMyTv = 28,
    RemoteMceMyMusic = 29,
    RemoteMceMyPictures = 30,
    RemoteMceMyVideos = 31,
    RemoteMceRecord = 32,
    RemoteMceStart = 33,
    RemoteMceVolumePlus = 34,
    RemoteMceVolumeMinus = 35,
    RemoteMceChannelPlus = 36,
    RemoteMceChannelMinus = 37,
    RemoteMceMute = 38,
    RemoteMceRecordedTv = 39,
    RemoteMceLiveTv = 40,
    RemoteMceStar = 41,
    RemoteMceHash = 42,
    RemoteMceClear = 43
} RemoteButton;

typedef struct MouseState
{
    float Dx;
    float Dy;
    float Dz;
    bool Buttons[5];
} MouseState;

typedef enum MouseButton
{
    MouseLeft = 0,
    MouseRight = 1,
    MouseMiddle = 2,
    MouseExtra1 = 3,
    MouseExtra2 = 4
} MouseButton;

typedef struct KeyboardState
{
    bool KeyDown;
    char Ascii;
    char VirtualKey;
    bool Buttons[6];
} KeyboardState;

typedef enum KeyboardButton
{
    KeyboardCtrl = 0,
    KeyboardShift = 1,
    KeyboardAlt = 2,
    KeyboardCapsLock = 3,
    KeyboardNumLock = 4,
    KeyboardScrollLock = 5
} KeyboardButton;

class InputManager
{
public:
    static void Init();
    static void ProcessController();
    static void ProcessRemote();
    static void ProcessMouse();
    static void ProcessKeyboard();
    static void ProcessMemoryUnit();
    static bool ControllerPressed(ControllerButton button, int port);
    static bool RemotePressed(RemoteButton button, int port);
    static bool MousePressed(MouseButton button, int port);
    static bool TryGetControllerState(int port, ControllerState* controllerState);
    static bool TryGetRemoteState(int port, RemoteState* remoteState);
    static bool TryGetMouseState(int port, MouseState* mouseState);
    static bool TryGetKeyboardState(int port, KeyboardState* keyboardState);
    static bool HasController(int port);
    static bool HasRemote(int port);
    static bool HasMouse(int port);
    static bool IsMemoryUnitMounted(char letter);
    static void PumpInput();
    static MousePosition GetMousePosition();
};
