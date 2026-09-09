bits 32

section .text align=16
global _p6fix_entry
global _preserve_dead_schedule_entry
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

align 16
_preserve_dead_schedule_entry:
    ; This hook replaces the MarkLastUses-only CALL to RemoveInstruction at
    ; 0x530aff.  The generic remover must continue invalidating schedules for
    ; every other caller.  If this block was already scheduled, deleting an
    ; instruction proven dead by MarkLastUses cannot change the dependencies
    ; or relative order of the remaining list, so retain that valid schedule.
    push ebx
    push esi
    push edi
    mov esi, [esp + 16]
    xor ebx, ebx
    xor edi, edi
    test esi, esi
    jz .call_original
    mov ebx, [esi + 8]
    test ebx, ebx
    jz .call_original
    ; The confirmed invalidation is a compiler-created dead `LI vreg, 0` in
    ; an eight-instruction block.  Keep this semantic guard narrow; other dead
    ; forms and block shapes retain the stock reschedule path.
    cmp word [esi + 0x14], 137
    jne .call_original
    cmp word [esi + 0x1a], 2
    jne .call_original
    cmp byte [esi + 0x1c], 0
    jne .call_original
    cmp byte [esi + 0x28], 4
    jne .call_original
    cmp dword [esi + 0x2a], 0
    jne .call_original
    cmp word [ebx + 0x2c], 8
    jne .call_original
    movzx edi, word [ebx + 0x2e]
    and edi, 8
.call_original:
    push esi
    mov eax, strict dword 0x0049d010
    call eax
    add esp, 4
    test edi, edi
    jz .done
    or word [ebx + 0x2e], 8
.done:
    pop edi
    pop esi
    pop ebx
    ret
