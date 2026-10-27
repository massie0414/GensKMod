; Include once per native memory-handler assembly unit.
extern _VRAM32XTransfer_InFrame
extern _VRAM32XTransfer_Pending
%macro COUNT_32X_VRAM_WRITE 1
    pushfd
    cmp dword [_VRAM32XTransfer_InFrame], 0
    je %%done
    add dword [_VRAM32XTransfer_Pending], %1
%%done:
    popfd
%endmacro
