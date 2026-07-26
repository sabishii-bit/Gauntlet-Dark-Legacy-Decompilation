#include "types.h"

/* Midway "pb" graphics library global init/reset/close layer (pb_global.obj
 * on Xbox, 4 fns; the GCN build keeps 3 -- pbResetGlobal is deadstripped).
 * .text 0x800C33FC-0x800C3674.
 *
 * pbInitGlobal brings up every pb sub-module in order (error, frame, window,
 * text, texture, geom, stats, objects, ...), then verifies via a per-module
 * "initialized" bitmask kept in the shared pb-global block (lbl_802C5230). If
 * any module failed to raise its bit it reports through ErrorPrintf:
 *   "ERROR: pbInitGlobal did not init all modules: %08x\n"
 * pbCloseGlobal tears the same modules down in reverse. pbSetupPBGPtrs (a file
 * -static helper, called by both) wires up the ~14-entry pushbuffer global
 * pointer table (lbl_80344FC8..lbl_80345000).
 *
 * Function names come from shell3D.pdb (pb_global.obj) and were confirmed
 * against the disassembly via the pbInitGlobal error string + the module
 * init/close call chains + the shared pb-global block reference (lbl_802C5230).
 *
 * NonMatching: bodies are structural stubs; dtk substitutes the original DOL
 * text for this range, so byte output is unaffected.
 */

/* --- shared pb-global block: per-module init bitmask + counters --- */
extern u32 gPBGlobal[]; /* lbl_802C5230 */

/* --- pushbuffer global pointer table (this layer wires it up) --- */
extern void* gPBGPtrs[]; /* lbl_80344FC8.. */

/* --- module bring-up / teardown entry points (other pb TUs) --- */
extern void ErrorPrintf(const char* fmt, ...);
extern void pbInitWindow(void);
extern void pbSetDefaultWindow(void);
extern void pbCloseWindow(void);

/* This TU owns a single rodata format string, held in the shared .rodata run
 * at 0x80116580 ("ERROR: pbInitGlobal did not init all modules: %08x\n"); it
 * is not re-claimed here because this is a text-only NonMatching split. */

/* Wire up the ~14-entry pushbuffer global pointer table. File-static helper
 * shared by pbInitGlobal and pbCloseGlobal. */
void pbSetupPBGPtrs(void)
{
    /* stub: populates gPBGPtrs[] from the module blocks */
    gPBGPtrs[0] = (void*)0;
}

/* Tear down every pb sub-module in reverse init order. */
void pbCloseGlobal(void)
{
    pbCloseWindow();
    pbSetupPBGPtrs();
    /* stub: closes frame / text / texture / geom / stats / objects / ... */
}

/* Bring up every pb sub-module, then verify the init bitmask (reported via
 * ErrorPrintf against the format string held in this TU's rodata). */
void pbInitGlobal(void)
{
    pbInitWindow();
    pbSetDefaultWindow();
    if (gPBGlobal[0] != 0) {
        ErrorPrintf((const char*)0, gPBGlobal[0]);
    }
    pbSetupPBGPtrs();
}
