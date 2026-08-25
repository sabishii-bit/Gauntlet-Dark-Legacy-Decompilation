bits 32

section .text align=16
global _p6fix_entry
extern _p6fix_scan

_p6fix_entry:
    pushfd
    pushad
    mov eax, [0x00587c74]
    push eax
    call _p6fix_scan
    add esp, 4
    popad
    popfd
    ; This hook replaces CALL 0x49d0f0 at 0x435afa. Tail-call the original
    ; predecessor builder so it returns directly to the original call site.
    push strict dword 0x0049d0f0
    ret
