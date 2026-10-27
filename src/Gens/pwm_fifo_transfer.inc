; Count an actual sample enqueue (1) or playback dequeue (0).
; Preserve all registers and flags used by the original FIFO/timer logic.
extern _PWMFIFOTransfer_InFrame
extern _PWMFIFOTransfer_Pending
%macro COUNT_PWM_FIFO_SAMPLE 1
    pushfd
    cmp dword [_PWMFIFOTransfer_InFrame], 0
    je %%done
    add dword [_PWMFIFOTransfer_Pending + %1 * 4], 2
%%done:
    popfd
%endmacro
