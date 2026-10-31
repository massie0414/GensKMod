#ifndef G_INPUT_H
#define G_INPUT_H

#define DIRECTINPUT_VERSION 0x0500  // for joystick support

#include <dinput.h>
//#include <mmsystem.h>

struct K_Def {
	unsigned int Start, Mode;
	unsigned int A, B, C;
	unsigned int X, Y, Z;
	unsigned int Up, Down, Left, Right;
	};

extern struct K_Def Keys_Def[8];
extern unsigned char Kaillera_Keys[16];

int String_Size(char *Chaine);
int Check_Key_Pressed(unsigned int key);
int Setting_Keys(HWND hset, int Player, int Type);
int Handle_Key_Setting_Message(HWND hset, UINT message, WPARAM wParam, LPARAM lParam);
int Init_Input(HINSTANCE hInst, HWND hWnd);
void End_Input(void);
void Update_Input(void);
void Update_Controllers(void);
void Scan_Player_Net(int Player);
void Update_Controllers_Net(int num_player);


#endif
