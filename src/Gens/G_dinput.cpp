#include <stdio.h>
#include "gens.h"
#include "resource.h"
#include "G_Input.h"
#include "io.h"
#include "G_main.h"

#define KEYDOWN(key) (Keys[key] & 0x80) 
#define MAX_JOYS 8

LPDIRECTINPUT lpDI;
LPDIRECTINPUTDEVICE lpDIDKeyboard;
LPDIRECTINPUTDEVICE lpDIDMouse;
char Phrase[1024];
int Nb_Joys = 0;
static IDirectInputDevice2 *Joy_ID[MAX_JOYS] = {NULL};
static DIJOYSTATE Joy_State[MAX_JOYS] = {{0}};
static bool Joy_Valid[MAX_JOYS];
static bool Keyboard_Valid;
long MouseX, MouseY;
unsigned char Keys[256];
unsigned char Kaillera_Keys[16];


struct K_Def Keys_Def[8] = {
	{DIK_RETURN, DIK_RSHIFT,
	DIK_A, DIK_S, DIK_D,
	DIK_Z, DIK_X, DIK_C,
	DIK_UP, DIK_DOWN, DIK_LEFT, DIK_RIGHT},
	{DIK_U, DIK_T,
	DIK_K, DIK_L, DIK_M,
	DIK_I, DIK_O, DIK_P,
	DIK_Y, DIK_H, DIK_G, DIK_J}
};


int String_Size(char *Chaine)
{
	int i = 0;

	while (*(Chaine + i++));

	return(i - 1);	
}


static void Clear_Joy_State(int i)
{
	ZeroMemory(&Joy_State[i], sizeof(Joy_State[i]));
	for (int j = 0; j < 4; j++) Joy_State[i].rgdwPOV[j] = 0xFFFFFFFF;
	Joy_Valid[i] = false;
}


void End_Input()
{
	if (lpDIDMouse)
	{
		lpDIDMouse->Release();
		lpDIDMouse = NULL;
	}
	if (lpDIDKeyboard)
	{
		lpDIDKeyboard->Unacquire();
		lpDIDKeyboard->Release();
		lpDIDKeyboard = NULL;
	}
	for (int i = 0; i < MAX_JOYS; i++)
	{
		if (Joy_ID[i])
		{
			Joy_ID[i]->Unacquire();
			Joy_ID[i]->Release();
			Joy_ID[i] = NULL;
		}
		Clear_Joy_State(i);
	}
	ZeroMemory(Keys, sizeof(Keys));
	Keyboard_Valid = false;
	Nb_Joys = 0;
	if (lpDI)
	{
		lpDI->Release();
		lpDI = NULL;
	}
}


BOOL CALLBACK InitJoystick(LPCDIDEVICEINSTANCE lpDIIJoy, LPVOID pvRef)
{
	HRESULT rval;
	LPDIRECTINPUTDEVICE	lpDIJoy;
	DIPROPRANGE diprg;
	int i;
 
	if (Nb_Joys >= MAX_JOYS) return(DIENUM_STOP);
		
	Joy_ID[Nb_Joys] = NULL;

	rval = lpDI->CreateDevice(lpDIIJoy->guidInstance, &lpDIJoy, NULL);
	if (rval != DI_OK)
	{
		MessageBox(HWnd, "IDirectInput::CreateDevice FAILED", "erreur joystick", MB_OK);
		return(DIENUM_CONTINUE);
	}

	rval = lpDIJoy->QueryInterface(IID_IDirectInputDevice2, (void **)&Joy_ID[Nb_Joys]);
	lpDIJoy->Release();
	if (rval != DI_OK)
	{
		MessageBox(HWnd, "IDirectInputDevice2::QueryInterface FAILED", "erreur joystick", MB_OK);
	    Joy_ID[Nb_Joys] = NULL;
	    return(DIENUM_CONTINUE);
	}

	rval = Joy_ID[Nb_Joys]->SetDataFormat(&c_dfDIJoystick);
	if (rval != DI_OK)
	{
		MessageBox(HWnd, "IDirectInputDevice::SetDataFormat FAILED", "erreur joystick", MB_OK);
		Joy_ID[Nb_Joys]->Release();
		Joy_ID[Nb_Joys] = NULL;
		return(DIENUM_CONTINUE);
	}

	rval = Joy_ID[Nb_Joys]->SetCooperativeLevel((HWND)pvRef, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);

	if (rval != DI_OK)
	{ 
		MessageBox(HWnd, "IDirectInputDevice::SetCooperativeLevel FAILED", "erreur joystick", MB_OK);
		Joy_ID[Nb_Joys]->Release();
		Joy_ID[Nb_Joys] = NULL;
		return(DIENUM_CONTINUE);
	}
 
	diprg.diph.dwSize = sizeof(diprg); 
	diprg.diph.dwHeaderSize = sizeof(diprg.diph); 
	diprg.diph.dwObj = DIJOFS_X;
	diprg.diph.dwHow = DIPH_BYOFFSET;
	diprg.lMin = -1000; 
	diprg.lMax = +1000;
 
	rval = Joy_ID[Nb_Joys]->SetProperty(DIPROP_RANGE, &diprg.diph);
	if ((rval != DI_OK) && (rval != DI_PROPNOEFFECT)) 
		MessageBox(HWnd, "IDirectInputDevice::SetProperty() (X-Axis) FAILED", "erreur joystick", MB_OK);

	diprg.diph.dwSize = sizeof(diprg); 
	diprg.diph.dwHeaderSize = sizeof(diprg.diph); 
	diprg.diph.dwObj = DIJOFS_Y;
	diprg.diph.dwHow = DIPH_BYOFFSET;
	diprg.lMin = -1000; 
	diprg.lMax = +1000;
 
	rval = Joy_ID[Nb_Joys]->SetProperty(DIPROP_RANGE, &diprg.diph);
	if ((rval != DI_OK) && (rval != DI_PROPNOEFFECT)) 
		MessageBox(HWnd, "IDirectInputDevice::SetProperty() (Y-Axis) FAILED", "erreur joystick", MB_OK);

	for(i = 0; i < 10; i++)
	{
		rval = Joy_ID[Nb_Joys]->Acquire();
		if (rval == DI_OK) break;
		Sleep(10);
	}

	Nb_Joys++;

	return(DIENUM_CONTINUE);
}


int Init_Input(HINSTANCE hInst, HWND hWnd)
{
	int i;
	HRESULT rval;

	End_Input();
	
	rval = DirectInputCreate(hInst, DIRECTINPUT_VERSION, &lpDI, NULL);
	if (rval != DI_OK)
	{
		MessageBox(hWnd, "DirectInput failed ...You must have DirectX 5", "Error", MB_OK);
		return 0;
	}
	
	Nb_Joys = 0;

	for(i = 0; i < MAX_JOYS; i++) Joy_ID[i] = NULL;

	rval = lpDI->EnumDevices(DIDEVTYPE_JOYSTICK, &InitJoystick, hWnd, DIEDFL_ATTACHEDONLY);
	if (rval != DI_OK) return 0;

//	rval = lpDI->CreateDevice(GUID_SysMouse, &lpDIDMouse, NULL);
	rval = lpDI->CreateDevice(GUID_SysKeyboard, &lpDIDKeyboard, NULL);
	if (rval != DI_OK) return 0;

//	rval = lpDIDMouse->SetCooperativeLevel(hWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	rval = lpDIDKeyboard->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
	if (rval != DI_OK) return 0;

//	rval = lpDIDMouse->SetDataFormat(&c_dfDIMouse);
	rval = lpDIDKeyboard->SetDataFormat(&c_dfDIKeyboard);
	if (rval != DI_OK) return 0;

//	rval = lpDIDMouse->Acquire();
	for(i = 0; i < 10; i++)
	{
		rval = lpDIDKeyboard->Acquire();
		if (rval == DI_OK) break;
		Sleep(10);
	}

	return 1;
}


// One bounded sample per call; failed reads must never leave a held input behind.
void Update_Input()
{
	Keyboard_Valid = false;
	if (lpDIDKeyboard)
	{
		HRESULT result = lpDIDKeyboard->GetDeviceState(sizeof(Keys), Keys);
		if ((result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) &&
			SUCCEEDED(lpDIDKeyboard->Acquire()))
			result = lpDIDKeyboard->GetDeviceState(sizeof(Keys), Keys);
		Keyboard_Valid = SUCCEEDED(result);
	}
	if (!Keyboard_Valid) ZeroMemory(Keys, sizeof(Keys));

	for (int i = 0; i < Nb_Joys; i++)
	{
		HRESULT result = DIERR_NOTACQUIRED;
		if (Joy_ID[i])
		{
			result = Joy_ID[i]->Poll();
			if (SUCCEEDED(result))
				result = Joy_ID[i]->GetDeviceState(sizeof(Joy_State[i]), &Joy_State[i]);
			if ((result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) &&
				SUCCEEDED(Joy_ID[i]->Acquire()))
			{
				result = Joy_ID[i]->Poll();
				if (SUCCEEDED(result))
					result = Joy_ID[i]->GetDeviceState(sizeof(Joy_State[i]), &Joy_State[i]);
			}
		}
		Joy_Valid[i] = SUCCEEDED(result);
		if (!Joy_Valid[i]) Clear_Joy_State(i);
	}
}


// Adjacent directions overlap at diagonals; both must work during gameplay too.
static bool POV_Pressed(DWORD angle, unsigned int direction)
{
	if (LOWORD(angle) == 0xFFFF || angle >= 36000) return false;
	switch (direction)
	{
		case 1: return angle <= 4500 || angle >= 31500;
		case 2: return angle >= 4500 && angle <= 13500;
		case 3: return angle >= 13500 && angle <= 22500;
		case 4: return angle >= 22500 && angle <= 31500;
	}
	return false;
}


int Check_Key_Pressed(unsigned int key)
{
	if (key < 0x100) return Keyboard_Valid && KEYDOWN(key);
	unsigned int joy = (key >> 8) & 0xF;
	if (joy >= MAX_JOYS || !Joy_ID[joy] || !Joy_Valid[joy]) return 0;
	if (key & 0x80)
		return POV_Pressed(Joy_State[joy].rgdwPOV[(key >> 4) & 3], key & 0xF);
	if (key & 0x70)
	{
		unsigned int button = (key & 0xFF) - 0x10;
		return button < 32 && (Joy_State[joy].rgbButtons[button] & 0x80);
	}
	switch (key & 0xF)
	{
		case 1: return Joy_State[joy].lY < -500;
		case 2: return Joy_State[joy].lY > 500;
		case 3: return Joy_State[joy].lX < -500;
		case 4: return Joy_State[joy].lX > 500;
	}
	return 0;
}


// Read the current sample without waiting. Prefer the requested direction when
// a POV diagonal contains two directions (e.g. down-right while defining DOWN).
static unsigned int Pressed_Key(unsigned int preferredPOV)
{
	for (int i = 1; i < 256; i++)
		if (Keyboard_Valid && KEYDOWN(i)) return i;
	for (int i = 0; i < Nb_Joys; i++)
	{
		if (!Joy_Valid[i]) continue;
		unsigned int base = 0x1000 + 0x100 * i;
		if (Joy_State[i].lY < -500) return base + 1;
		if (Joy_State[i].lY > 500) return base + 2;
		if (Joy_State[i].lX < -500) return base + 3;
		if (Joy_State[i].lX > 500) return base + 4;
		for (int j = 0; j < 4; j++)
		{
			DWORD angle = Joy_State[i].rgdwPOV[j];
			if (preferredPOV && POV_Pressed(angle, preferredPOV))
				return base + 0x80 + 0x10 * j + preferredPOV;
			for (unsigned int direction = 1; direction <= 4; direction++)
				if (POV_Pressed(angle, direction))
					return base + 0x80 + 0x10 * j + direction;
		}
		for (int j = 0; j < 32; j++)
			if (Joy_State[i].rgbButtons[j] & 0x80) return base + 0x10 + j;
	}
	return 0;
}


void Update_Controllers()
{

	Update_Input();

	if (Check_Key_Pressed(Keys_Def[0].Up))
	{
		Controller_1_Up = 0;
		Controller_1_Down = 1;
	}
	else
	{
		Controller_1_Up = 1;
		if (Check_Key_Pressed(Keys_Def[0].Down)) Controller_1_Down = 0;
		else Controller_1_Down = 1;
	}
	
	if (Check_Key_Pressed(Keys_Def[0].Left))
	{
		Controller_1_Left = 0;
		Controller_1_Right = 1;
	}
	else
	{
		Controller_1_Left = 1;
		if (Check_Key_Pressed(Keys_Def[0].Right)) Controller_1_Right = 0;
		else Controller_1_Right = 1;
	}

	if (Check_Key_Pressed(Keys_Def[0].Start)) Controller_1_Start = 0;
	else Controller_1_Start = 1;

	if (Check_Key_Pressed(Keys_Def[0].A)) Controller_1_A = 0;
	else Controller_1_A = 1;

	if (Check_Key_Pressed(Keys_Def[0].B)) Controller_1_B = 0;
	else Controller_1_B = 1;

	if (Check_Key_Pressed(Keys_Def[0].C)) Controller_1_C = 0;
	else Controller_1_C = 1;

	if (Controller_1_Type & 1)
	{
		if (Check_Key_Pressed(Keys_Def[0].Mode)) Controller_1_Mode = 0;
		else Controller_1_Mode = 1;

		if (Check_Key_Pressed(Keys_Def[0].X)) Controller_1_X = 0;
		else Controller_1_X = 1;

		if (Check_Key_Pressed(Keys_Def[0].Y)) Controller_1_Y = 0;
		else Controller_1_Y = 1;

		if (Check_Key_Pressed(Keys_Def[0].Z)) Controller_1_Z = 0;
		else Controller_1_Z = 1;
	}

	if (Check_Key_Pressed(Keys_Def[1].Up))
	{
		Controller_2_Up = 0;
		Controller_2_Down = 1;
	}
	else
	{
		Controller_2_Up = 1;
		if (Check_Key_Pressed(Keys_Def[1].Down)) Controller_2_Down = 0;
		else Controller_2_Down = 1;
	}

	
	if (Check_Key_Pressed(Keys_Def[1].Left))
	{
		Controller_2_Left = 0;
		Controller_2_Right = 1;
	}
	else
	{
		Controller_2_Left = 1;
		if (Check_Key_Pressed(Keys_Def[1].Right)) Controller_2_Right = 0;
		else Controller_2_Right = 1;
	}

	if (Check_Key_Pressed(Keys_Def[1].Start)) Controller_2_Start = 0;
	else Controller_2_Start = 1;

	if (Check_Key_Pressed(Keys_Def[1].A)) Controller_2_A = 0;
	else Controller_2_A = 1;

	if (Check_Key_Pressed(Keys_Def[1].B)) Controller_2_B = 0;
	else Controller_2_B = 1;

	if (Check_Key_Pressed(Keys_Def[1].C)) Controller_2_C = 0;
	else Controller_2_C = 1;

	if (Controller_2_Type & 1)
	{
		if (Check_Key_Pressed(Keys_Def[1].Mode)) Controller_2_Mode = 0;
		else Controller_2_Mode = 1;

		if (Check_Key_Pressed(Keys_Def[1].X)) Controller_2_X = 0;
		else Controller_2_X = 1;

		if (Check_Key_Pressed(Keys_Def[1].Y)) Controller_2_Y = 0;
		else Controller_2_Y = 1;

		if (Check_Key_Pressed(Keys_Def[1].Z)) Controller_2_Z = 0;
		else Controller_2_Z = 1;
	}

	if (Controller_1_Type & 0x10)			// TEAMPLAYER PORT 1
	{
		if (Check_Key_Pressed(Keys_Def[2].Up))
		{
			Controller_1B_Up = 0;
			Controller_1B_Down = 1;
		}
		else
		{
			Controller_1B_Up = 1;
			if (Check_Key_Pressed(Keys_Def[2].Down)) Controller_1B_Down = 0;
			else Controller_1B_Down = 1;
		}
	
		if (Check_Key_Pressed(Keys_Def[2].Left))
		{
			Controller_1B_Left = 0;
			Controller_1B_Right = 1;
		}
		else
		{
			Controller_1B_Left = 1;
			if (Check_Key_Pressed(Keys_Def[2].Right)) Controller_1B_Right = 0;
			else Controller_1B_Right = 1;
		}

		if (Check_Key_Pressed(Keys_Def[2].Start)) Controller_1B_Start = 0;
		else Controller_1B_Start = 1;

		if (Check_Key_Pressed(Keys_Def[2].A)) Controller_1B_A = 0;
		else Controller_1B_A = 1;

		if (Check_Key_Pressed(Keys_Def[2].B)) Controller_1B_B = 0;
		else Controller_1B_B = 1;

		if (Check_Key_Pressed(Keys_Def[2].C)) Controller_1B_C = 0;
		else Controller_1B_C = 1;

		if (Controller_1B_Type & 1)
		{
			if (Check_Key_Pressed(Keys_Def[2].Mode)) Controller_1B_Mode = 0;
			else Controller_1B_Mode = 1;

			if (Check_Key_Pressed(Keys_Def[2].X)) Controller_1B_X = 0;
			else Controller_1B_X = 1;

			if (Check_Key_Pressed(Keys_Def[2].Y)) Controller_1B_Y = 0;
			else Controller_1B_Y = 1;

			if (Check_Key_Pressed(Keys_Def[2].Z)) Controller_1B_Z = 0;
			else Controller_1B_Z = 1;
		}

		if (Check_Key_Pressed(Keys_Def[3].Up))
		{
			Controller_1C_Up = 0;
			Controller_1C_Down = 1;
		}
		else
		{
			Controller_1C_Up = 1;
			if (Check_Key_Pressed(Keys_Def[3].Down)) Controller_1C_Down = 0;
			else Controller_1C_Down = 1;
		}
	
		if (Check_Key_Pressed(Keys_Def[3].Left))
		{
			Controller_1C_Left = 0;
			Controller_1C_Right = 1;
		}
		else
		{
			Controller_1C_Left = 1;
			if (Check_Key_Pressed(Keys_Def[3].Right)) Controller_1C_Right = 0;
			else Controller_1C_Right = 1;
		}

		if (Check_Key_Pressed(Keys_Def[3].Start)) Controller_1C_Start = 0;
		else Controller_1C_Start = 1;

		if (Check_Key_Pressed(Keys_Def[3].A)) Controller_1C_A = 0;
		else Controller_1C_A = 1;

		if (Check_Key_Pressed(Keys_Def[3].B)) Controller_1C_B = 0;
		else Controller_1C_B = 1;

		if (Check_Key_Pressed(Keys_Def[3].C)) Controller_1C_C = 0;
		else Controller_1C_C = 1;

		if (Controller_1C_Type & 1)
		{
			if (Check_Key_Pressed(Keys_Def[3].Mode)) Controller_1C_Mode = 0;
			else Controller_1C_Mode = 1;

			if (Check_Key_Pressed(Keys_Def[3].X)) Controller_1C_X = 0;
			else Controller_1C_X = 1;

			if (Check_Key_Pressed(Keys_Def[3].Y)) Controller_1C_Y = 0;
			else Controller_1C_Y = 1;

			if (Check_Key_Pressed(Keys_Def[3].Z)) Controller_1C_Z = 0;
			else Controller_1C_Z = 1;
		}

		if (Check_Key_Pressed(Keys_Def[4].Up))
		{
			Controller_1D_Up = 0;
			Controller_1D_Down = 1;
		}
		else
		{
			Controller_1D_Up = 1;
			if (Check_Key_Pressed(Keys_Def[4].Down)) Controller_1D_Down = 0;
			else Controller_1D_Down = 1;
		}
	
		if (Check_Key_Pressed(Keys_Def[4].Left))
		{
			Controller_1D_Left = 0;
			Controller_1D_Right = 1;
		}
		else
		{
			Controller_1D_Left = 1;
			if (Check_Key_Pressed(Keys_Def[4].Right)) Controller_1D_Right = 0;
			else Controller_1D_Right = 1;
		}

		if (Check_Key_Pressed(Keys_Def[4].Start)) Controller_1D_Start = 0;
		else Controller_1D_Start = 1;

		if (Check_Key_Pressed(Keys_Def[4].A)) Controller_1D_A = 0;
		else Controller_1D_A = 1;

		if (Check_Key_Pressed(Keys_Def[4].B)) Controller_1D_B = 0;
		else Controller_1D_B = 1;

		if (Check_Key_Pressed(Keys_Def[4].C)) Controller_1D_C = 0;
		else Controller_1D_C = 1;

		if (Controller_1D_Type & 1)
		{
			if (Check_Key_Pressed(Keys_Def[4].Mode)) Controller_1D_Mode = 0;
			else Controller_1D_Mode = 1;

			if (Check_Key_Pressed(Keys_Def[4].X)) Controller_1D_X = 0;
			else Controller_1D_X = 1;

			if (Check_Key_Pressed(Keys_Def[4].Y)) Controller_1D_Y = 0;
			else Controller_1D_Y = 1;

			if (Check_Key_Pressed(Keys_Def[4].Z)) Controller_1D_Z = 0;
			else Controller_1D_Z = 1;
		}
	}

	if (Controller_2_Type & 0x10)			// TEAMPLAYER PORT 2
	{
		if (Check_Key_Pressed(Keys_Def[5].Up))
		{
			Controller_2B_Up = 0;
			Controller_2B_Down = 1;
		}
		else
		{
			Controller_2B_Up = 1;
			if (Check_Key_Pressed(Keys_Def[5].Down)) Controller_2B_Down = 0;
			else Controller_2B_Down = 1;
		}
	
		if (Check_Key_Pressed(Keys_Def[5].Left))
		{
			Controller_2B_Left = 0;
			Controller_2B_Right = 1;
		}
		else
		{
			Controller_2B_Left = 1;
			if (Check_Key_Pressed(Keys_Def[5].Right)) Controller_2B_Right = 0;
			else Controller_2B_Right = 1;
		}

		if (Check_Key_Pressed(Keys_Def[5].Start)) Controller_2B_Start = 0;
		else Controller_2B_Start = 1;

		if (Check_Key_Pressed(Keys_Def[5].A)) Controller_2B_A = 0;
		else Controller_2B_A = 1;

		if (Check_Key_Pressed(Keys_Def[5].B)) Controller_2B_B = 0;
		else Controller_2B_B = 1;

		if (Check_Key_Pressed(Keys_Def[5].C)) Controller_2B_C = 0;
		else Controller_2B_C = 1;

		if (Controller_2B_Type & 1)
		{
			if (Check_Key_Pressed(Keys_Def[5].Mode)) Controller_2B_Mode = 0;
			else Controller_2B_Mode = 1;

			if (Check_Key_Pressed(Keys_Def[5].X)) Controller_2B_X = 0;
			else Controller_2B_X = 1;

			if (Check_Key_Pressed(Keys_Def[5].Y)) Controller_2B_Y = 0;
			else Controller_2B_Y = 1;

			if (Check_Key_Pressed(Keys_Def[5].Z)) Controller_2B_Z = 0;
			else Controller_2B_Z = 1;
		}

		if (Check_Key_Pressed(Keys_Def[6].Up))
		{
			Controller_2C_Up = 0;
			Controller_2C_Down = 1;
		}
		else
		{
			Controller_2C_Up = 1;
			if (Check_Key_Pressed(Keys_Def[6].Down)) Controller_2C_Down = 0;
			else Controller_2C_Down = 1;
		}
	
		if (Check_Key_Pressed(Keys_Def[6].Left))
		{
			Controller_2C_Left = 0;
			Controller_2C_Right = 1;
		}
		else
		{
			Controller_2C_Left = 1;
			if (Check_Key_Pressed(Keys_Def[6].Right)) Controller_2C_Right = 0;
			else Controller_2C_Right = 1;
		}

		if (Check_Key_Pressed(Keys_Def[6].Start)) Controller_2C_Start = 0;
		else Controller_2C_Start = 1;

		if (Check_Key_Pressed(Keys_Def[6].A)) Controller_2C_A = 0;
		else Controller_2C_A = 1;

		if (Check_Key_Pressed(Keys_Def[6].B)) Controller_2C_B = 0;
		else Controller_2C_B = 1;

		if (Check_Key_Pressed(Keys_Def[6].C)) Controller_2C_C = 0;
		else Controller_2C_C = 1;

		if (Controller_2C_Type & 1)
		{
			if (Check_Key_Pressed(Keys_Def[6].Mode)) Controller_2C_Mode = 0;
			else Controller_2C_Mode = 1;

			if (Check_Key_Pressed(Keys_Def[6].X)) Controller_2C_X = 0;
			else Controller_2C_X = 1;

			if (Check_Key_Pressed(Keys_Def[6].Y)) Controller_2C_Y = 0;
			else Controller_2C_Y = 1;

			if (Check_Key_Pressed(Keys_Def[6].Z)) Controller_2C_Z = 0;
			else Controller_2C_Z = 1;
		}

		if (Check_Key_Pressed(Keys_Def[7].Up))
		{
			Controller_2D_Up = 0;
			Controller_2D_Down = 1;
		}
		else
		{
			Controller_2D_Up = 1;
			if (Check_Key_Pressed(Keys_Def[7].Down)) Controller_2D_Down = 0;
			else Controller_2D_Down = 1;
		}
	
		if (Check_Key_Pressed(Keys_Def[7].Left))
		{
			Controller_2D_Left = 0;
			Controller_2D_Right = 1;
		}
		else
		{
			Controller_2D_Left = 1;
			if (Check_Key_Pressed(Keys_Def[7].Right)) Controller_2D_Right = 0;
			else Controller_2D_Right = 1;
		}

		if (Check_Key_Pressed(Keys_Def[7].Start)) Controller_2D_Start = 0;
		else Controller_2D_Start = 1;

		if (Check_Key_Pressed(Keys_Def[7].A)) Controller_2D_A = 0;
		else Controller_2D_A = 1;

		if (Check_Key_Pressed(Keys_Def[7].B)) Controller_2D_B = 0;
		else Controller_2D_B = 1;

		if (Check_Key_Pressed(Keys_Def[7].C)) Controller_2D_C = 0;
		else Controller_2D_C = 1;

		if (Controller_2D_Type & 1)
		{
			if (Check_Key_Pressed(Keys_Def[7].Mode)) Controller_2D_Mode = 0;
			else Controller_2D_Mode = 1;

			if (Check_Key_Pressed(Keys_Def[7].X)) Controller_2D_X = 0;
			else Controller_2D_X = 1;

			if (Check_Key_Pressed(Keys_Def[7].Y)) Controller_2D_Y = 0;
			else Controller_2D_Y = 1;

			if (Check_Key_Pressed(Keys_Def[7].Z)) Controller_2D_Z = 0;
			else Controller_2D_Z = 1;
		}
	}
}


// Key assignment runs in the dialog's normal message loop, never in a wait loop.
static const UINT_PTR KEY_SETTING_TIMER = 0x4B53;
static struct KeySettingState {
	HWND dialog, previousFocus;
	WNDPROC textProc;
	int player, step, count;
	bool waitRelease;
	unsigned int releaseKey;
	K_Def pending;
} KeySetting = {};

static unsigned int K_Def::*const KeyFields[] = {
	&K_Def::Up, &K_Def::Down, &K_Def::Left, &K_Def::Right,
	&K_Def::Start, &K_Def::A, &K_Def::B, &K_Def::C,
	&K_Def::Mode, &K_Def::X, &K_Def::Y, &K_Def::Z
};
static const char *const KeyPrompts[] = {
	"INPUT KEY FOR UP", "INPUT KEY FOR DOWN", "INPUT KEY FOR LEFT", "INPUT KEY FOR RIGHT",
	"INPUT KEY FOR START", "INPUT KEY FOR A", "INPUT KEY FOR B", "INPUT KEY FOR C",
	"INPUT KEY FOR MODE", "INPUT KEY FOR X", "INPUT KEY FOR Y", "INPUT KEY FOR Z"
};

static void Key_Setting_Controls(HWND dialog, BOOL enabled)
{
	for (int id = IDC_COMBO_PORT1; id <= IDC_BUTTON_SETKEYSP2D; id++)
		EnableWindow(GetDlgItem(dialog, id), enabled);
	EnableWindow(GetDlgItem(dialog, ID_HELP_HELP), enabled);
	SetDlgItemText(dialog, IDOK, enabled ? "OK" : "Cancel");
}

static void Finish_Key_Setting(bool commit)
{
	HWND dialog = KeySetting.dialog;
	if (!dialog) return;
	KillTimer(dialog, KEY_SETTING_TIMER);
	if (commit) Keys_Def[KeySetting.player] = KeySetting.pending;
	HWND text = GetDlgItem(dialog, IDC_STATIC_TEXT1);
	SetWindowLongPtr(text, GWLP_WNDPROC, (LONG_PTR)KeySetting.textProc);
	KeySetting.dialog = NULL;
	Key_Setting_Controls(dialog, TRUE);
	SetDlgItemText(dialog, IDC_STATIC_TEXT0, "SETTING KEYS");
	SetDlgItemText(dialog, IDC_STATIC_TEXT1, commit ? "CONFIGURATION SUCCESSFUL" : "");
	SetDlgItemText(dialog, IDC_STATIC_TEXT2, "");
	if (GetForegroundWindow() == dialog)
		SetFocus(IsWindow(KeySetting.previousFocus) ? KeySetting.previousFocus : GetDlgItem(dialog, IDOK));
}

// Keep Enter, Space, Tab and arrow keys available for binding instead of letting
// the dialog manager turn them into button clicks or focus navigation.
static LRESULT CALLBACK Key_Setting_Text_Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
	if (message == WM_KEYDOWN || message == WM_KEYUP || message == WM_CHAR)
	{
		if (message == WM_KEYUP && wParam == VK_ESCAPE) Finish_Key_Setting(false);
		return 0;
	}
	return CallWindowProc(KeySetting.textProc, hwnd, message, wParam, lParam);
}

int Setting_Keys(HWND hset, int Player, int TypeP)
{
	if (KeySetting.dialog || Player < 0 || Player >= 8) return 0;
	HWND text = GetDlgItem(hset, IDC_STATIC_TEXT1);
	if (!text || !GetDlgItem(hset, IDC_STATIC_TEXT2)) return 0;
	KeySetting.dialog = hset;
	KeySetting.previousFocus = GetFocus();
	KeySetting.player = Player;
	KeySetting.step = 0;
	KeySetting.count = TypeP ? 12 : 8;
	KeySetting.waitRelease = true;
	KeySetting.releaseKey = 0;
	KeySetting.pending = Keys_Def[Player];
	KeySetting.textProc = (WNDPROC)SetWindowLongPtr(text, GWLP_WNDPROC, (LONG_PTR)Key_Setting_Text_Proc);
	Key_Setting_Controls(hset, FALSE);
	SetFocus(text);
	SetDlgItemText(hset, IDC_STATIC_TEXT1, KeyPrompts[0]);
	SetDlgItemText(hset, IDC_STATIC_TEXT2, "Release all controls. Esc / Cancel to cancel.");
	if (!SetTimer(hset, KEY_SETTING_TIMER, 20, NULL))
	{
		Finish_Key_Setting(false);
		SetDlgItemText(hset, IDC_STATIC_TEXT1, "Could not start key assignment.");
		return 0;
	}
	return 1;
}

static void Tick_Key_Setting()
{
	HWND dialog = KeySetting.dialog;
	if (GetForegroundWindow() != dialog)
	{
		KeySetting.waitRelease = true;
		return;
	}
	Update_Input();
	static const unsigned int povDirections[] = {1, 3, 4, 2};
	unsigned int key = Pressed_Key(KeySetting.step < 4 ? povDirections[KeySetting.step] : 0);
	// Escape is reserved for cancellation on key-up, handled by the focused control.
	if (key == DIK_ESCAPE) return;
	if (KeySetting.waitRelease)
	{
		unsigned int previous = KeySetting.releaseKey;
		bool valid = previous == 0 || (previous < 0x100 ? Keyboard_Valid : Joy_Valid[(previous >> 8) & 0xF]);
		if (!valid)
		{
			SetDlgItemText(dialog, IDC_STATIC_TEXT2, "Input unavailable. Reconnect or cancel.");
			return;
		}
		if (key)
		{
			SetDlgItemText(dialog, IDC_STATIC_TEXT2, "Release all controls. Esc / Cancel to cancel.");
			return;
		}
		KeySetting.waitRelease = false;
		if (KeySetting.step == KeySetting.count)
		{
			Finish_Key_Setting(true);
			return;
		}
	}
	bool available = Keyboard_Valid;
	for (int i = 0; i < Nb_Joys; i++) available = available || Joy_Valid[i];
	unsigned int previous = KeySetting.releaseKey;
	bool lostPrevious = previous >= 0x100 && !Joy_Valid[(previous >> 8) & 0xF];
	SetDlgItemText(dialog, IDC_STATIC_TEXT2, !available || lostPrevious ?
		"Input unavailable. Reconnect or cancel." : "Press a control. Esc / Cancel to cancel.");
	if (!key) return;
	KeySetting.pending.*KeyFields[KeySetting.step] = key;
	KeySetting.step++;
	KeySetting.waitRelease = true;
	KeySetting.releaseKey = key;
	if (KeySetting.step < KeySetting.count)
		SetDlgItemText(dialog, IDC_STATIC_TEXT1, KeyPrompts[KeySetting.step]);
	SetDlgItemText(dialog, IDC_STATIC_TEXT2, "Release all controls. Esc / Cancel to cancel.");
}

int Handle_Key_Setting_Message(HWND hset, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (KeySetting.dialog != hset) return 0;
	switch (message)
	{
		case WM_TIMER:
			if (wParam != KEY_SETTING_TIMER) return 0;
			Tick_Key_Setting();
			return 1;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL || (LOWORD(wParam) == IDOK && lParam))
				Finish_Key_Setting(false);
			return 1;
		case WM_ACTIVATE:
			KeySetting.waitRelease = true;
			if (LOWORD(wParam) != WA_INACTIVE) SetFocus(GetDlgItem(hset, IDC_STATIC_TEXT1));
			break;
		case WM_CLOSE:
		case WM_DESTROY:
			Finish_Key_Setting(false);
			break;
	}
	return 0;
}


void Scan_Player_Net(int Player)
{
	if (!Player) return;

	Update_Input();

	if (Check_Key_Pressed(Keys_Def[0].Up))
	{
		Kaillera_Keys[0] &= ~0x08;
		Kaillera_Keys[0] |= 0x04;
	}
	else
	{
		Kaillera_Keys[0] |= 0x08;
		if (Check_Key_Pressed(Keys_Def[0].Down)) Kaillera_Keys[0] &= ~0x04;
		else Kaillera_Keys[0] |= 0x04;
	}
	
	if (Check_Key_Pressed(Keys_Def[0].Left))
	{
		Kaillera_Keys[0] &= ~0x02;
		Kaillera_Keys[0] |= 0x01;
	}
	else
	{
		Kaillera_Keys[0] |= 0x02;
		if (Check_Key_Pressed(Keys_Def[0].Right)) Kaillera_Keys[0] &= ~0x01;
		else Kaillera_Keys[0] |= 0x01;
	}

	if (Check_Key_Pressed(Keys_Def[0].Start)) Kaillera_Keys[0] &= ~0x80;
	else Kaillera_Keys[0] |= 0x80;

	if (Check_Key_Pressed(Keys_Def[0].A)) Kaillera_Keys[0] &= ~0x40;
	else Kaillera_Keys[0] |= 0x40;

	if (Check_Key_Pressed(Keys_Def[0].B)) Kaillera_Keys[0] &= ~0x20;
	else Kaillera_Keys[0] |= 0x20;

	if (Check_Key_Pressed(Keys_Def[0].C)) Kaillera_Keys[0] &= ~0x10;
	else Kaillera_Keys[0] |= 0x10;

	if (Controller_1_Type & 1)
	{
		if (Check_Key_Pressed(Keys_Def[0].Mode)) Kaillera_Keys[1] &= ~0x08;
		else Kaillera_Keys[1] |= 0x08;

		if (Check_Key_Pressed(Keys_Def[0].X)) Kaillera_Keys[1] &= ~0x04;
		else Kaillera_Keys[1] |= 0x04;

		if (Check_Key_Pressed(Keys_Def[0].Y)) Kaillera_Keys[1] &= ~0x02;
		else Kaillera_Keys[1] |= 0x02;

		if (Check_Key_Pressed(Keys_Def[0].Z)) Kaillera_Keys[1] &= ~0x01;
		else Kaillera_Keys[1] |= 0x01;
	}
}


void Update_Controllers_Net(int num_player)
{
	Controller_1_Up = (Kaillera_Keys[0] & 0x08) >> 3;
	Controller_1_Down = (Kaillera_Keys[0] & 0x04) >> 2;
	Controller_1_Left = (Kaillera_Keys[0] & 0x02) >> 1;
	Controller_1_Right = (Kaillera_Keys[0] & 0x01);
	Controller_1_Start = (Kaillera_Keys[0] & 0x80) >> 7;
	Controller_1_A = (Kaillera_Keys[0] & 0x40) >> 6;
	Controller_1_B = (Kaillera_Keys[0] & 0x20) >> 5;
	Controller_1_C = (Kaillera_Keys[0] & 0x10) >> 4;

	if (Controller_1_Type & 1)
	{
		Controller_1_Mode = (Kaillera_Keys[0 + 1] & 0x08) >> 3;
		Controller_1_X = (Kaillera_Keys[0 + 1] & 0x04) >> 2;
		Controller_1_Y = (Kaillera_Keys[0 + 1] & 0x02) >> 1;
		Controller_1_Z = (Kaillera_Keys[0 + 1] & 0x01);
	}

	if (num_player > 2)			// TEAMPLAYER
	{
		Controller_1B_Up = (Kaillera_Keys[2] & 0x08) >> 3;
		Controller_1B_Down = (Kaillera_Keys[2] & 0x04) >> 2;
		Controller_1B_Left = (Kaillera_Keys[2] & 0x02) >> 1;
		Controller_1B_Right = (Kaillera_Keys[2] & 0x01);
		Controller_1B_Start = (Kaillera_Keys[2] & 0x80) >> 7;
		Controller_1B_A = (Kaillera_Keys[2] & 0x40) >> 6;
		Controller_1B_B = (Kaillera_Keys[2] & 0x20) >> 5;
		Controller_1B_C = (Kaillera_Keys[2] & 0x10) >> 4;

		if (Controller_1B_Type & 1)
		{
			Controller_1B_Mode = (Kaillera_Keys[2 + 1] & 0x08) >> 3;
			Controller_1B_X = (Kaillera_Keys[2 + 1] & 0x04) >> 2;
			Controller_1B_Y = (Kaillera_Keys[2 + 1] & 0x02) >> 1;
			Controller_1B_Z = (Kaillera_Keys[2 + 1] & 0x01);
		}

		Controller_1C_Up = (Kaillera_Keys[4] & 0x08) >> 3;
		Controller_1C_Down = (Kaillera_Keys[4] & 0x04) >> 2;
		Controller_1C_Left = (Kaillera_Keys[4] & 0x02) >> 1;
		Controller_1C_Right = (Kaillera_Keys[4] & 0x01);
		Controller_1C_Start = (Kaillera_Keys[4] & 0x80) >> 7;
		Controller_1C_A = (Kaillera_Keys[4] & 0x40) >> 6;
		Controller_1C_B = (Kaillera_Keys[4] & 0x20) >> 5;
		Controller_1C_C = (Kaillera_Keys[4] & 0x10) >> 4;

		if (Controller_1C_Type & 1)
		{
			Controller_1C_Mode = (Kaillera_Keys[4 + 1] & 0x08) >> 3;
			Controller_1C_X = (Kaillera_Keys[4 + 1] & 0x04) >> 2;
			Controller_1C_Y = (Kaillera_Keys[4 + 1] & 0x02) >> 1;
			Controller_1C_Z = (Kaillera_Keys[4 + 1] & 0x01);
		}

		Controller_1D_Up = (Kaillera_Keys[6] & 0x08) >> 3;
		Controller_1D_Down = (Kaillera_Keys[6] & 0x04) >> 2;
		Controller_1D_Left = (Kaillera_Keys[6] & 0x02) >> 1;
		Controller_1D_Right = (Kaillera_Keys[6] & 0x01);
		Controller_1D_Start = (Kaillera_Keys[6] & 0x80) >> 7;
		Controller_1D_A = (Kaillera_Keys[6] & 0x40) >> 6;
		Controller_1D_B = (Kaillera_Keys[6] & 0x20) >> 5;
		Controller_1D_C = (Kaillera_Keys[6] & 0x10) >> 4;

		if (Controller_1D_Type & 1)
		{
			Controller_1D_Mode = (Kaillera_Keys[6 + 1] & 0x08) >> 3;
			Controller_1D_X = (Kaillera_Keys[6 + 1] & 0x04) >> 2;
			Controller_1D_Y = (Kaillera_Keys[6 + 1] & 0x02) >> 1;
			Controller_1D_Z = (Kaillera_Keys[6 + 1] & 0x01);
		}
	}
	else
	{
		Controller_2_Up = (Kaillera_Keys[2] & 0x08) >> 3;
		Controller_2_Down = (Kaillera_Keys[2] & 0x04) >> 2;
		Controller_2_Left = (Kaillera_Keys[2] & 0x02) >> 1;
		Controller_2_Right = (Kaillera_Keys[2] & 0x01);
		Controller_2_Start = (Kaillera_Keys[2] & 0x80) >> 7;
		Controller_2_A = (Kaillera_Keys[2] & 0x40) >> 6;
		Controller_2_B = (Kaillera_Keys[2] & 0x20) >> 5;
		Controller_2_C = (Kaillera_Keys[2] & 0x10) >> 4;

		if (Controller_2_Type & 1)
		{
			Controller_2_Mode = (Kaillera_Keys[2 + 1] & 0x08) >> 3;
			Controller_2_X = (Kaillera_Keys[2 + 1] & 0x04) >> 2;
			Controller_2_Y = (Kaillera_Keys[2 + 1] & 0x02) >> 1;
			Controller_2_Z = (Kaillera_Keys[2 + 1] & 0x01);
		}
	}
}


