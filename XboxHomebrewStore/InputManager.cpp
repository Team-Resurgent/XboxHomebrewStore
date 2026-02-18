#include "InputManager.h"
#include "Drawing.h"
#include "Math.h"
#include "String.h"

extern "C" 
{
    extern XPP_DEVICE_TYPE XDEVICE_TYPE_IR_REMOTE_TABLE;
    #define XDEVICE_TYPE_IR_REMOTE (&XDEVICE_TYPE_IR_REMOTE_TABLE)
}

#define XINPUT_IR_REMOTE_DISPLAY 213
#define XINPUT_IR_REMOTE_REVERSE 226
#define XINPUT_IR_REMOTE_PLAY 234
#define XINPUT_IR_REMOTE_FORWARD 227
#define XINPUT_IR_REMOTE_SKIP_MINUS 221
#define XINPUT_IR_REMOTE_STOP 224
#define XINPUT_IR_REMOTE_PAUSE 230
#define XINPUT_IR_REMOTE_SKIP_PLUS 223
#define XINPUT_IR_REMOTE_TITLE 229
#define XINPUT_IR_REMOTE_INFO 195
#define XINPUT_IR_REMOTE_UP 166
#define XINPUT_IR_REMOTE_DOWN 167
#define XINPUT_IR_REMOTE_LEFT 169
#define XINPUT_IR_REMOTE_RIGHT 168
#define XINPUT_IR_REMOTE_SELECT 11
#define XINPUT_IR_REMOTE_MENU 247
#define XINPUT_IR_REMOTE_BACK 216
#define XINPUT_IR_REMOTE_1 206
#define XINPUT_IR_REMOTE_2 205
#define XINPUT_IR_REMOTE_3 204
#define XINPUT_IR_REMOTE_4 203
#define XINPUT_IR_REMOTE_5 202
#define XINPUT_IR_REMOTE_6 201
#define XINPUT_IR_REMOTE_7 200
#define XINPUT_IR_REMOTE_8 199
#define XINPUT_IR_REMOTE_9 198
#define XINPUT_IR_REMOTE_0 207
#define XINPUT_IR_REMOTE_MCE_POWER 196
#define XINPUT_IR_REMOTE_MCE_MY_TV 49
#define XINPUT_IR_REMOTE_MCE_MY_MUSIC 9
#define XINPUT_IR_REMOTE_MCE_MY_PICTURES 6
#define XINPUT_IR_REMOTE_MCE_MY_VIDEOS 7
#define XINPUT_IR_REMOTE_MCE_RECORD 232
#define XINPUT_IR_REMOTE_MCE_START 37
#define XINPUT_IR_REMOTE_MCE_VOLUME_PLUS 208
#define XINPUT_IR_REMOTE_MCE_VOLUME_MINUS 209
#define XINPUT_IR_REMOTE_MCE_CHANNEL_PLUS 210
#define XINPUT_IR_REMOTE_MCE_CHANNEL_MINUS 211
#define XINPUT_IR_REMOTE_MCE_MUTE 192
#define XINPUT_IR_REMOTE_MCE_RECORDED_TV 101
#define XINPUT_IR_REMOTE_MCE_LIVE_TV 24
#define XINPUT_IR_REMOTE_MCE_STAR 40
#define XINPUT_IR_REMOTE_MCE_HASH 41
#define XINPUT_IR_REMOTE_MCE_CLEAR 249

typedef struct XInputStateEx
{
    DWORD PacketNumber;
    BYTE Buttons;
    BYTE Region;
    BYTE Counter;
    BYTE FirstEvent;
} XInputStateEx;

#define UPDATE_ANALOG_HYSTERESIS(value, held) ((held) ? ((value) >= 24) : ((value) >= 48))

namespace
{
	bool mInitialized = false;

    MousePosition mMousePosition;

    DWORD mControllerTick;
	HANDLE mControllerHandles[XGetPortCount()];
    float mControllerFiilterX[XGetPortCount()];
    float mControllerFiilterY[XGetPortCount()];
    DWORD mControllerLastPacketNumber[XGetPortCount()];
    ControllerState mControllerStatesCurrent[XGetPortCount()];
    ControllerState mControllerStatesPrevious[XGetPortCount()];

    HANDLE mRemoteHandles[XGetPortCount()];
    DWORD mRemoteLastPacketNumber[XGetPortCount()];
    RemoteState mRemoteStatesCurrent[XGetPortCount()];
    RemoteState mRemoteStatesPrevious[XGetPortCount()];

	HANDLE mMouseHandles[XGetPortCount()];
    DWORD mMouseLastPacketNumber[XGetPortCount()];
    MouseState mMouseStatesCurrent[XGetPortCount()];
    MouseState mMouseStatesPrevious[XGetPortCount()];

    HANDLE mKeyboardHandles[XGetPortCount()];
    KeyboardState mKeyboardState;

    CHAR mMemoryUnityHandles[XGetPortCount() * 2];
}

void InputManager::Init()
{
    XInitDevices(0, 0);

    memset(&mMousePosition, 0, sizeof(mMousePosition));

    mControllerTick = 0;
    memset(mControllerHandles, 0, sizeof(mControllerHandles));
    memset(&mControllerFiilterX, 0, sizeof(mControllerFiilterX));
    memset(&mControllerFiilterY, 0, sizeof(mControllerFiilterY));
    memset(mControllerLastPacketNumber, 0, sizeof(mControllerLastPacketNumber));
    memset(mControllerStatesCurrent, 0, sizeof(mControllerStatesCurrent));
    memset(mControllerStatesPrevious, 0, sizeof(mControllerStatesPrevious));

    memset(mRemoteHandles, 0, sizeof(mRemoteHandles));
    memset(mRemoteLastPacketNumber, 0, sizeof(mRemoteLastPacketNumber));
    memset(mRemoteStatesCurrent, 0, sizeof(mRemoteStatesCurrent));
    memset(mRemoteStatesPrevious, 0, sizeof(mRemoteStatesPrevious));

    memset(mMouseHandles, 0, sizeof(mMouseHandles));
    memset(mMouseLastPacketNumber, 0, sizeof(mMouseLastPacketNumber));
    memset(mMouseStatesCurrent, 0, sizeof(mMouseStatesCurrent));
    memset(mMouseStatesPrevious, 0, sizeof(mMouseStatesPrevious));

    memset(mKeyboardHandles, 0, sizeof(mKeyboardHandles));
    memset(&mKeyboardState, 0, sizeof(mKeyboardState));

    XINPUT_DEBUG_KEYQUEUE_PARAMETERS keyboardSettings;
    keyboardSettings.dwFlags = XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYUP;
    keyboardSettings.dwQueueSize = 25;
    keyboardSettings.dwRepeatDelay = 500;
    keyboardSettings.dwRepeatInterval = 50;
    XInputDebugInitKeyboardQueue(&keyboardSettings);

    memset(&mMemoryUnityHandles, 0, sizeof(mMemoryUnityHandles));
}

void InputManager::ProcessController()
{
    DWORD insertions = 0;
	DWORD removals = 0;
    if (XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals) == TRUE)
	{
		for (int i = 0; i < XGetPortCount(); i++)
		{
			if ((insertions & 1) == 1)
			{
				mControllerHandles[i] = XInputOpen(XDEVICE_TYPE_GAMEPAD, i, XDEVICE_NO_SLOT, NULL);
			}
			if ((removals & 1) == 1)
			{
				XInputClose(mControllerHandles[i]);
				mControllerHandles[i] = NULL;
			}
			insertions = insertions >> 1;
			removals = removals >> 1;
		}
	}

    DWORD now = GetTickCount();
    if (mControllerTick == 0)
    {
        mControllerTick = now;
    }
    float dt = (now - mControllerTick) / 1000.0f;
    if (dt <= 0.0f)
    {
        dt = 1.0f / 60.0f;
    }
    mControllerTick = now;

    for (int i = 0; i < XGetPortCount(); i++)
	{
        XINPUT_STATE controllerInputState;
        if (mControllerHandles[i] == NULL || XInputGetState(mControllerHandles[i], &controllerInputState) != 0) 
		{
			continue;
		}

        if (mControllerLastPacketNumber[i] != controllerInputState.dwPacketNumber)
        {
            memcpy(&mControllerStatesPrevious[i], &mControllerStatesCurrent[i], sizeof(ControllerState));
            mControllerStatesCurrent[i].Buttons[ControllerA] = UPDATE_ANALOG_HYSTERESIS(controllerInputState.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_A], mControllerStatesPrevious[i].Buttons[ControllerB]);
            mControllerStatesCurrent[i].Buttons[ControllerB] = UPDATE_ANALOG_HYSTERESIS(controllerInputState.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_B], mControllerStatesPrevious[i].Buttons[ControllerB]);
            mControllerStatesCurrent[i].Buttons[ControllerX] = UPDATE_ANALOG_HYSTERESIS(controllerInputState.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_X], mControllerStatesPrevious[i].Buttons[ControllerX]);
            mControllerStatesCurrent[i].Buttons[ControllerY] = UPDATE_ANALOG_HYSTERESIS(controllerInputState.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_Y], mControllerStatesPrevious[i].Buttons[ControllerY]);
            mControllerStatesCurrent[i].Buttons[ControllerBlack] = UPDATE_ANALOG_HYSTERESIS(controllerInputState.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_BLACK], mControllerStatesPrevious[i].Buttons[ControllerBlack]);
            mControllerStatesCurrent[i].Buttons[ControllerWhite] = UPDATE_ANALOG_HYSTERESIS(controllerInputState.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_WHITE], mControllerStatesPrevious[i].Buttons[ControllerWhite]);
            mControllerStatesCurrent[i].Buttons[ControllerLTrigger] = UPDATE_ANALOG_HYSTERESIS(controllerInputState.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER], mControllerStatesPrevious[i].Buttons[ControllerLTrigger]);
            mControllerStatesCurrent[i].Buttons[ControllerRTrigger] = UPDATE_ANALOG_HYSTERESIS(controllerInputState.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER], mControllerStatesPrevious[i].Buttons[ControllerRTrigger]);
            mControllerStatesCurrent[i].Buttons[ControllerDpadUp] = (controllerInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
            mControllerStatesCurrent[i].Buttons[ControllerDpadDown] = (controllerInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
            mControllerStatesCurrent[i].Buttons[ControllerDpadLeft] = (controllerInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
            mControllerStatesCurrent[i].Buttons[ControllerDpadRight] = (controllerInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
            mControllerStatesCurrent[i].Buttons[ControllerStart] = (controllerInputState.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
            mControllerStatesCurrent[i].Buttons[ControllerBack] = (controllerInputState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
            mControllerStatesCurrent[i].Buttons[ControllerLThumb] = (controllerInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
            mControllerStatesCurrent[i].Buttons[ControllerRThumb] = (controllerInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
        }

        const float raw_lx =  controllerInputState.Gamepad.sThumbLX / 32768.0f;
        const float raw_ly = -controllerInputState.Gamepad.sThumbLY / 32768.0f;

        const float deadzone = 0.25f;
        float lx = (fabsf(raw_lx) < deadzone) ? 0.0f : (raw_lx - Math::CopySign(deadzone, raw_lx)) / (1.0f - deadzone);
        float ly = (fabsf(raw_ly) < deadzone) ? 0.0f : (raw_ly - Math::CopySign(deadzone, raw_ly)) / (1.0f - deadzone);

        const float filter_alpha = 0.35f;
        mControllerFiilterX[i] = mControllerFiilterX[i] + filter_alpha * (lx - mControllerFiilterX[i]);
        mControllerFiilterY[i] = mControllerFiilterY[i] + filter_alpha * (ly - mControllerFiilterY[i]);

        const bool button_a_held = mControllerStatesCurrent[i].Buttons[ControllerA];
        const bool left_trigger_held = mControllerStatesCurrent[i].Buttons[ControllerLTrigger];
        const float base_speed = 600.0f; 
        const float boost = button_a_held ? 1.25f : 1.0f;
        const float precision = left_trigger_held ? 0.35f : 1.0f;

        const float dx = mControllerFiilterX[i] * base_speed * boost * precision * dt;
        const float dy = mControllerFiilterY[i] * base_speed * boost * precision * dt;

        mMousePosition.X = Math::ClampFloat(mMousePosition.X + dx, 0.0f, (float)Drawing::GetBufferWidth());
        mMousePosition.Y = Math::ClampFloat(mMousePosition.Y + dy, 0.0f, (float)Drawing::GetBufferHeight());

        mControllerStatesCurrent[i].ThumbLeftDx = dx;
        mControllerStatesCurrent[i].ThumbLeftDy = dy;

        mControllerLastPacketNumber[i] = controllerInputState.dwPacketNumber;
    }
}

void InputManager::ProcessRemote()
{
    DWORD insertions = 0;
	DWORD removals = 0;
    if (XGetDeviceChanges(XDEVICE_TYPE_IR_REMOTE, &insertions, &removals) == TRUE)
	{
		for (int i = 0; i < XGetPortCount(); i++)
		{
			if ((insertions & 1) == 1)
			{
				mRemoteHandles[i] = XInputOpen(XDEVICE_TYPE_IR_REMOTE, i, XDEVICE_NO_SLOT, NULL);
			}
			if ((removals & 1) == 1)
			{
				XInputClose(mRemoteHandles[i]);
				mRemoteHandles[i] = NULL;
			}
			insertions = insertions >> 1;
			removals = removals >> 1;
		}
	}

    for (int i = 0; i < XGetPortCount(); i++)
	{
        XInputStateEx remoteInputState;
        if (mRemoteHandles[i] == NULL || XInputGetState(mRemoteHandles[i], (XINPUT_STATE*)&remoteInputState) != 0 || remoteInputState.FirstEvent == 0x00) 
		{
			continue;
		}

        if (mRemoteLastPacketNumber[i] != remoteInputState.PacketNumber)
        {
            memcpy(&mRemoteStatesPrevious[i], &mRemoteStatesCurrent[i], sizeof(RemoteState));
            mRemoteStatesCurrent[i].Buttons[RemoteDisplay] = remoteInputState.Buttons == XINPUT_IR_REMOTE_DISPLAY;
            mRemoteStatesCurrent[i].Buttons[RemoteReverse] = remoteInputState.Buttons == XINPUT_IR_REMOTE_REVERSE;
            mRemoteStatesCurrent[i].Buttons[RemotePlay] = remoteInputState.Buttons == XINPUT_IR_REMOTE_PLAY;
            mRemoteStatesCurrent[i].Buttons[RemoteForward] = remoteInputState.Buttons == XINPUT_IR_REMOTE_FORWARD;
            mRemoteStatesCurrent[i].Buttons[RemoteSkipMinus] = remoteInputState.Buttons == XINPUT_IR_REMOTE_SKIP_MINUS;
            mRemoteStatesCurrent[i].Buttons[RemoteStop] = remoteInputState.Buttons == XINPUT_IR_REMOTE_STOP;
            mRemoteStatesCurrent[i].Buttons[RemotePause] = remoteInputState.Buttons == XINPUT_IR_REMOTE_PAUSE;
            mRemoteStatesCurrent[i].Buttons[RemoteSkipPlus] = remoteInputState.Buttons == XINPUT_IR_REMOTE_SKIP_PLUS;
            mRemoteStatesCurrent[i].Buttons[RemoteTitle] = remoteInputState.Buttons == XINPUT_IR_REMOTE_TITLE;
            mRemoteStatesCurrent[i].Buttons[RemoteInfo] = remoteInputState.Buttons == XINPUT_IR_REMOTE_INFO;
            mRemoteStatesCurrent[i].Buttons[RemoteUp] = remoteInputState.Buttons == XINPUT_IR_REMOTE_UP;
            mRemoteStatesCurrent[i].Buttons[RemoteDown] = remoteInputState.Buttons == XINPUT_IR_REMOTE_DOWN;
            mRemoteStatesCurrent[i].Buttons[RemoteLeft] = remoteInputState.Buttons == XINPUT_IR_REMOTE_LEFT;
            mRemoteStatesCurrent[i].Buttons[RemoteRight] = remoteInputState.Buttons == XINPUT_IR_REMOTE_RIGHT;
            mRemoteStatesCurrent[i].Buttons[RemoteSelect] = remoteInputState.Buttons == XINPUT_IR_REMOTE_SELECT;
            mRemoteStatesCurrent[i].Buttons[RemoteMenu] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MENU;
            mRemoteStatesCurrent[i].Buttons[RemoteBack] = remoteInputState.Buttons == XINPUT_IR_REMOTE_BACK;
            mRemoteStatesCurrent[i].Buttons[Remote1] = remoteInputState.Buttons == XINPUT_IR_REMOTE_1;
            mRemoteStatesCurrent[i].Buttons[Remote2] = remoteInputState.Buttons == XINPUT_IR_REMOTE_2;
            mRemoteStatesCurrent[i].Buttons[Remote3] = remoteInputState.Buttons == XINPUT_IR_REMOTE_3;
            mRemoteStatesCurrent[i].Buttons[Remote4] = remoteInputState.Buttons == XINPUT_IR_REMOTE_4;
            mRemoteStatesCurrent[i].Buttons[Remote5] = remoteInputState.Buttons == XINPUT_IR_REMOTE_5;
            mRemoteStatesCurrent[i].Buttons[Remote6] = remoteInputState.Buttons == XINPUT_IR_REMOTE_6;
            mRemoteStatesCurrent[i].Buttons[Remote7] = remoteInputState.Buttons == XINPUT_IR_REMOTE_7;
            mRemoteStatesCurrent[i].Buttons[Remote8] = remoteInputState.Buttons == XINPUT_IR_REMOTE_8;
            mRemoteStatesCurrent[i].Buttons[Remote9] = remoteInputState.Buttons == XINPUT_IR_REMOTE_9;
            mRemoteStatesCurrent[i].Buttons[Remote0] = remoteInputState.Buttons == XINPUT_IR_REMOTE_0;
            mRemoteStatesCurrent[i].Buttons[RemoteMcePower] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_POWER;
            mRemoteStatesCurrent[i].Buttons[RemoteMceMyTv] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_MY_TV;
            mRemoteStatesCurrent[i].Buttons[RemoteMceMyMusic] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_MY_MUSIC;
            mRemoteStatesCurrent[i].Buttons[RemoteMceMyPictures] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_MY_PICTURES;
            mRemoteStatesCurrent[i].Buttons[RemoteMceMyVideos] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_MY_VIDEOS;
            mRemoteStatesCurrent[i].Buttons[RemoteMceRecord] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_RECORD;
            mRemoteStatesCurrent[i].Buttons[RemoteMceStart] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_START;
            mRemoteStatesCurrent[i].Buttons[RemoteMceVolumePlus] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_VOLUME_PLUS;
            mRemoteStatesCurrent[i].Buttons[RemoteMceVolumeMinus] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_VOLUME_MINUS;
            mRemoteStatesCurrent[i].Buttons[RemoteMceChannelPlus] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_CHANNEL_PLUS;
            mRemoteStatesCurrent[i].Buttons[RemoteMceChannelMinus] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_CHANNEL_MINUS;
            mRemoteStatesCurrent[i].Buttons[RemoteMceMute] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_MUTE;
            mRemoteStatesCurrent[i].Buttons[RemoteMceRecordedTv] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_RECORDED_TV;
            mRemoteStatesCurrent[i].Buttons[RemoteMceLiveTv] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_LIVE_TV;
            mRemoteStatesCurrent[i].Buttons[RemoteMceStar] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_STAR;
            mRemoteStatesCurrent[i].Buttons[RemoteMceHash] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_HASH;
            mRemoteStatesCurrent[i].Buttons[RemoteMceClear] = remoteInputState.Buttons == XINPUT_IR_REMOTE_MCE_CLEAR;
            mRemoteLastPacketNumber[i] = remoteInputState.PacketNumber;
        }
    }
}

void InputManager::ProcessMouse()
{
    DWORD insertions = 0;
	DWORD removals = 0;
    if (XGetDeviceChanges(XDEVICE_TYPE_DEBUG_MOUSE, &insertions, &removals) == TRUE)
	{
		for (int i = 0; i < XGetPortCount(); i++)
		{
			if ((insertions & 1) == 1)
			{
				mMouseHandles[i] = XInputOpen(XDEVICE_TYPE_DEBUG_MOUSE, i, XDEVICE_NO_SLOT, NULL);
			}
			if ((removals & 1) == 1)
			{
				XInputClose(mMouseHandles[i]);
				mMouseHandles[i] = NULL;
			}
			insertions = insertions >> 1;
			removals = removals >> 1;
		}
	}

	for (int i = 0; i < XGetPortCount(); i++)
	{
        XINPUT_STATE mouseInputState;
		if (mMouseHandles[i] == NULL || XInputGetState(mMouseHandles[i], &mouseInputState) != 0) 
		{
			continue;
		}

        if (mMouseLastPacketNumber[i] != mouseInputState.dwPacketNumber)
        {
            memcpy(&mMouseStatesPrevious[i], &mouseInputState, sizeof(MouseState));
            mMouseStatesCurrent[i].Dx = mouseInputState.DebugMouse.cMickeysX;
            mMouseStatesCurrent[i].Dy = mouseInputState.DebugMouse.cMickeysY;
            mMouseStatesCurrent[i].Dz = mouseInputState.DebugMouse.cWheel;
            mMouseStatesCurrent[i].Buttons[MouseLeft] = (mouseInputState.DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_LEFT_BUTTON) != 0;
            mMouseStatesCurrent[i].Buttons[MouseRight] = (mouseInputState.DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_RIGHT_BUTTON) != 0;
            mMouseStatesCurrent[i].Buttons[MouseMiddle] = (mouseInputState.DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_MIDDLE_BUTTON) != 0;
            mMouseStatesCurrent[i].Buttons[MouseExtra1] = (mouseInputState.DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_XBUTTON1) != 0;
            mMouseStatesCurrent[i].Buttons[MouseExtra2] = (mouseInputState.DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_XBUTTON2) != 0;
            mMousePosition.X = Math::ClampFloat(mMousePosition.X + mMouseStatesCurrent[i].Dx, 0, (float)Drawing::GetBufferWidth());
            mMousePosition.Y = Math::ClampFloat(mMousePosition.Y + mMouseStatesCurrent[i].Dy, 0, (float)Drawing::GetBufferHeight());
            mMouseLastPacketNumber[i] = mouseInputState.dwPacketNumber;
        }
    }
}

void InputManager::ProcessKeyboard()
{
    DWORD insertions = 0;
	DWORD removals = 0;
    if (XGetDeviceChanges(XDEVICE_TYPE_DEBUG_KEYBOARD, &insertions, &removals) == TRUE)
	{
		for (int i = 0; i < XGetPortCount(); i++)
		{
			if ((insertions & 1) == 1)
			{
                XINPUT_POLLING_PARAMETERS pollValues;
                pollValues.fAutoPoll = TRUE;
                pollValues.fInterruptOut = TRUE;
                pollValues.bInputInterval = 32;
                pollValues.bOutputInterval = 32;
                pollValues.ReservedMBZ1 = 0;
                pollValues.ReservedMBZ2 = 0;
				mKeyboardHandles[i] = XInputOpen(XDEVICE_TYPE_DEBUG_KEYBOARD, i, XDEVICE_NO_SLOT, &pollValues);
			}
			if ((removals & 1) == 1)
			{
				XInputClose(mKeyboardHandles[i]);
				mKeyboardHandles[i] = NULL;
			}
			insertions = insertions >> 1;
			removals = removals >> 1;
		}
	}

    mKeyboardState.KeyDown = false;
#ifndef XINPUT_DEBUG_KEYSTROKE_FLAG_REPEAT
#define XINPUT_DEBUG_KEYSTROKE_FLAG_REPEAT 0x02
#endif
    for (int i = 0; i < XGetPortCount(); i++)
    {
        if (mKeyboardHandles[i] == NULL)
            continue;
        XINPUT_DEBUG_KEYSTROKE currentKeyStroke;
        memset(&currentKeyStroke, 0, sizeof(currentKeyStroke));
        if (XInputDebugGetKeystroke(&currentKeyStroke) != 0)
            continue;
        const bool keyUp = (currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_KEYUP) != 0;
        const bool repeat = (currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_REPEAT) != 0;
        /* Allow keys with only VirtualKey (e.g. Page Up/Down have Ascii 0) */
        const bool validKey = (currentKeyStroke.VirtualKey != 0);
        if (keyUp || repeat || !validKey)
        {
            continue;
        }
        mKeyboardState.KeyDown = true;
        mKeyboardState.Ascii = currentKeyStroke.Ascii;
        mKeyboardState.VirtualKey = currentKeyStroke.VirtualKey;
        mKeyboardState.Buttons[KeyboardCtrl] = (currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_CTRL) != 0;
        mKeyboardState.Buttons[KeyboardShift] = (currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_SHIFT) != 0;
        mKeyboardState.Buttons[KeyboardAlt] = (currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_ALT) != 0;
        mKeyboardState.Buttons[KeyboardCapsLock] = (currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_CAPSLOCK) != 0;
        mKeyboardState.Buttons[KeyboardNumLock] = (currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_NUMLOCK) != 0;
        mKeyboardState.Buttons[KeyboardScrollLock] = (currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_SCROLLLOCK) != 0;
        break;
    }
}

void InputManager::ProcessMemoryUnit()
{
    DWORD insertions = 0;
	DWORD removals = 0;
    if (XGetDeviceChanges(XDEVICE_TYPE_MEMORY_UNIT, &insertions, &removals) == TRUE) {
        for (uint32_t iPort = 0; iPort < XGetPortCount(); iPort++) {
            for (uint32_t iSlot = 0; iSlot < 2; iSlot++) {
                uint32_t mask = iPort + (iSlot ? 16 : 0);
                mask = 1 << mask;

                uint32_t index = (iPort * 2) + iSlot;
                if ((mask & removals) != 0 && mMemoryUnityHandles[index] != 0) {
                    const char* mountPoint = String::Format("\\??\\%c:", 'H' + index).c_str();
                    STRING sMountPoint = {(USHORT)strlen(mountPoint), (USHORT)strlen(mountPoint) + 1, (char*)mountPoint};

                    DEVICE_OBJECT* device = MU_GetExistingDeviceObject(iPort, iSlot);
                    IoDismountVolume(device);

                    IoDeleteSymbolicLink(&sMountPoint);

                    MU_CloseDeviceObject(iPort, iSlot);
                    mMemoryUnityHandles[index] = 0;
                }

                if ((mask & insertions) != 0 && mMemoryUnityHandles[index] == 0) {
                    char szDeviceName[64];
                    STRING DeviceName;
                    DeviceName.Length = 0;
                    DeviceName.MaximumLength = sizeof(szDeviceName) / sizeof(CHAR) - 2;
                    DeviceName.Buffer = szDeviceName;
                    if (MU_CreateDeviceObject(iPort, iSlot, &DeviceName) >= 0) {
                        const char* mountPoint = String::Format("\\??\\%c:", 'H' + index).c_str();
                        STRING sMountPoint = {(USHORT)strlen(mountPoint), (USHORT)strlen(mountPoint) + 1, (char*)mountPoint};
                        IoCreateSymbolicLink(&sMountPoint, &DeviceName);
                        mMemoryUnityHandles[index] = (char)('H' + index);
                    }
                }
            }
        }
    }
}

bool InputManager::ControllerPressed(ControllerButton button, int port)
{
	for (int i = 0; i < XGetPortCount(); i++)
	{
		if (port >= 0 && port != i || mControllerHandles[i] == NULL)
		{
			continue;
		}
		if (mControllerStatesCurrent[i].Buttons[button] == true && mControllerStatesPrevious[i].Buttons[button] == false)
		{
			return true;
		}
	}
	return false;
}

bool InputManager::RemotePressed(RemoteButton button, int port)
{
    for (int i = 0; i < XGetPortCount(); i++)
	{
		if (port >= 0 && port != i || mRemoteHandles[i] == NULL)
		{
			continue;
		}
		if (mRemoteStatesCurrent[i].Buttons[button] == true && mRemoteStatesPrevious[i].Buttons[button] == false)
		{
			return true;
		}
	}
	return false;
}

bool InputManager::MousePressed(MouseButton button, int port)
{
	for (int i = 0; i < XGetPortCount(); i++)
	{
		if (port >= 0 && port != i || mMouseHandles[i] == NULL)
		{
			continue;
		}
		if (mMouseStatesCurrent[i].Buttons[button] == true && mMouseStatesPrevious[i].Buttons[button] == false)
		{
			return true;
		}
	}
	return false;
}

bool InputManager::TryGetControllerState(int port, ControllerState* controllerState)
{
    if (controllerState != NULL)
    {
	    for (int i = 0; i < XGetPortCount(); i++)
	    {
		    if (port >= 0 && port != i || mControllerHandles[i] == NULL)
		    {
			    continue;
		    }
            *controllerState = mControllerStatesCurrent[i];
            return true;
	    }
    }
	return false;
}

bool InputManager::TryGetRemoteState(int port, RemoteState* remoteState)
{
    if (remoteState != NULL)
    {
	    for (int i = 0; i < XGetPortCount(); i++)
	    {
		    if (port >= 0 && port != i || mRemoteHandles[i] == NULL)
		    {
			    continue;
		    }
            *remoteState = mRemoteStatesCurrent[i];
            return true;
	    }
    }
	return false;
}

bool InputManager::TryGetMouseState(int port, MouseState* mouseState)
{
    if (mouseState != NULL)
    {
	    for (int i = 0; i < XGetPortCount(); i++)
	    {
		    if (port >= 0 && port != i || mMouseHandles[i] == NULL)
		    {
			    continue;
		    }
            *mouseState = mMouseStatesCurrent[i];
            return true;
	    }
    }
	return false;
}

bool InputManager::TryGetKeyboardState(int port, KeyboardState* keyboardState)
{
    if (keyboardState != NULL)
    {
	    for (int i = 0; i < XGetPortCount(); i++)
	    {
		    if (port >= 0 && port != i || mKeyboardHandles[i] == NULL)
		    {
			    continue;
		    }
            *keyboardState = mKeyboardState;
            return true;
	    }
    }
	return false;
}

bool InputManager::HasController(int port)
{
	for (int i = 0; i < XGetPortCount(); i++)
	{
		if (port >= 0 && port != i || mControllerHandles[i] == NULL)
		{
			continue;
		}
        return true;
	}
	return false;
}

bool InputManager::HasRemote(int port)
{
	for (int i = 0; i < XGetPortCount(); i++)
	{
		if (port >= 0 && port != i || mRemoteHandles[i] == NULL)
		{
			continue;
		}
        return true;
	}
	return false;
}

bool InputManager::HasMouse(int port)
{
	for (int i = 0; i < XGetPortCount(); i++)
	{
		if (port >= 0 && port != i || mMouseHandles[i] == NULL)
		{
			continue;
		}
        return true;
	}
	return false;
}

bool InputManager::IsMemoryUnitMounted(char letter) {
    for (int i = 0; i < XGetPortCount() * 2; i++) {
        if (mMemoryUnityHandles[i] == letter) {
            return true;
        }
    }
    return false;
}

void InputManager::PumpInput()
{
    ProcessController();
    ProcessRemote(); 
    ProcessMouse();
    ProcessKeyboard();
    ProcessMemoryUnit();
}

MousePosition InputManager::GetMousePosition()
{
    return mMousePosition;
}
