#include <assert.h>
#include "../src/Gens/kmod/vdp_reg.c"
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
struct Reg_VDP_Type VDP_Reg;
void CloseWindow_KMod(UCHAR mode) { OpenedWindow_KMod[mode-1]=FALSE; vdpreg_show(FALSE); }
int main(void) {
    INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_LISTVIEW_CLASSES};
    char text[32];
    int i;
    InitCommonControlsEx(&ic);
    VDP_Reg.Set2=0x44;
    vdpreg_create(GetModuleHandle(NULL),NULL);
    assert(hRegister);
    OpenedWindow_KMod[DMODE_VDP_REG-1]=TRUE;
    vdpreg_show(TRUE);
    for(i=0;i<3;i++) {
        vdpreg_reset();
        assert(IsWindowVisible(hRegister));
        assert(OpenedWindow_KMod[DMODE_VDP_REG-1]);
        assert(ListView_GetItemCount(hVDPReg_List)==24);
        assert(Header_GetItemCount(ListView_GetHeader(hVDPReg_List))==3);
        ListView_GetItemText(hVDPReg_List,1,1,text,sizeof(text));
        assert(!strcmp(text,"0x44"));
    }
    CloseWindow_KMod(DMODE_VDP_REG);
    vdpreg_reset();
    assert(!IsWindowVisible(hRegister));
    puts("PASS: VDP register reset preserves visibility, 24 rows, 3 columns and values");
    return 0;
}