# Next-wave work orders (2026-08-19, staged by integrator Kimi)

Context: 55% campaign. Near-miss regalloc polish is exhausted (parked classes
P1/P2/P8/P9 dominate). Wins come from: (a) full/partial reconstructions of
stubs and skeletons, (b) genuine correctness bugs. Every target below is
un-owned at time of writing. Tools: tools/gdl/{fnasm,fndiff,matchtool}.py.
Check research/PARKED.txt before touching anything. No asm ever.

## btricol.c (un-owned)
- LineLineDist (464 insns, stub with fake body at btricol.c:245): segment-line
  closest-point math. Target idiom map (from fnasm): perp = dirB x dirA via
  fmsubs chains (f7=db.z*da.x-db.x*da.z, f6=db.y*da.z-db.z*da.y,
  f8=db.x*da.y-db.y*da.x); |perp|^2 via fmadds chain; fcmpu vs lbl_80345D50
  (0.0f) gates the parallel case at +0x4b4; t = lbl_80345DA8/perp2 computed as
  f64 then frsp; lenB/lenA are f64 params. Sibling LineLineDist3D2D (same file)
  is MATCHED and shows the TU's exact idioms (volatile/register scaffolding,
  SlowNormalVector, lbl_80345D50 = 0.0f). Start by mirroring its declaration
  style.
- BTriLineCol (450 insns, 8.7%): swept tri/line test built on the edge passes.

## newcam.c (un-owned)
- fn_8006DF34 (1512B, 0%), fn_8006E654 (1476B, 0%), DebugCamControlInputs
  (1092B, 0%): never-written camera helpers. Camera struct in include/game/.

## options.c (un-owned)
- DoOptions (1150 insns, 57%): block-order reconstruction — jump-table case
  order (L49) + u64 flag idiom (L16) + branch polarity. Dump the target jump
  table from the auto_* data .s and reorder source cases by ascending target
  block address.
- show_optmenu (797/763 insns, 62.7%): same family.
- do_controlsmenu (97/96, 84%), next_rune_hint/next_legend_hint/next_boss_hint
  (~97% each, table-index load + slw coloring cluster shared by all three).

## psfx.c (un-owned)
- LoadPlyrData (4552B, 83.4%): 1182/1138 insns; big but mostly present.
- fn_800898DC / fn_80089350 (371/355 insns, ~0.3%): stubs.

## atree.c (un-owned)
- fn_8001267C (575/565, 80.7%) + fn_80011DCC (556/467, 64%): systematic
  stb/lbz store-scheduling + cmplwi/cmpwi signedness drift; check byte-field
  signedness in the atree/anode structs (Xbox PDB types verified for this TU).

## action.c (assigned to w3_action) — DoEnemyAction L49 case-order lever
## already briefed in its worker prompt.

## dbgtext.c: fn_800C03E0 (433/21, 3%) — mostly unwritten.
## mb_model.c: SetupModel (686/23, 3%) — mostly unwritten.
## mb_font.c: MBRenderText (347/19, 4.6%) — mostly unwritten.
## mb_particle.c: MBDrawPsys (810/61, 5.7%), MBNewPsysDescrip (793/23, 3%),
##   setupNewPMode_800CDCE4 (669/138, 15.7%), setWorldParms (843/865, 76.5%),
##   allocPsys/freePsysMem/DrawPsysSub — assorted states.
## sysservice.c: sysResetService real 112 residual = string-pool layout (P7,
##   whole-TU) + gMsgCallback arg scheduling + case-2 hold-loop coloring;
##   padUpdate real 129. Finish only after the TU's other functions settle.
