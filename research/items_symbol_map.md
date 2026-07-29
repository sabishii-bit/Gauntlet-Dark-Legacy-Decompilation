# `items.c` placeholder symbol map

This map records the July 2026 identification pass over
`src/game/world/items.c`. “PDB” means the name is present in the Xbox
`shell3D.pdb` symbol roster. “Behavior” means the GameCube symbol has no known
PDB spelling and the name describes its observed role. Strings were read
directly from the DOL; tables and state were identified from their complete
cross-reference sets.

## Functions

| Old symbol | Name | Basis |
|---|---|---|
| `fn_800115D0` | `AtreeDelete` | PDB order, whole-tree teardown behavior |
| `fn_80011104` | `AnimateATree` | ATREE.OBJ PDB; thin zero-frame animation wrapper |
| `fn_80012F78` | `AtreeInit` | ATREE.OBJ PDB; thin runtime-tree construction wrapper |
| `fn_8004F404` | `check_vacancy` | ENEMY.OBJ PDB; validates a candidate generator position |
| `fn_80050FB0` | `GetEnemyType` | ENEMY.OBJ PDB; resolves type/subtype through loaded overrides |
| `fn_80051F64` | `EnemyTypePrefix` | ENEMY.OBJ PDB; returns the type's formatted-name prefix |
| `fn_80051FDC` | `EnemyDescType` | ENEMY.OBJ PDB; description-to-enemy-type lookup |
| `fn_8005412C` | `SetPlayerVars` | GAMEMAIN.OBJ PDB; rebuilds active-player category globals |
| `fn_80057B30` | `FindWave` | gauntworld.obj PDB; parses the compact letter/digit wave tag |
| `fn_80057AB4` | `LevelItemDesc` | PDB and returned level record |
| `fn_80057AC0` | `WorldItemDesc` | PDB and returned world record |
| `fn_8005A260` | `LoadModel` | GLUE.OBJ PDB and model-loader calls |
| `fn_8005A3B8` | `UpdateObjWorldMat` | GLUE.OBJ PDB and matrix/node update |
| `fn_800635B4` | `generate_now` | ITEMS.OBJ PDB and generator gate |
| `fn_8006366C` | `DistanceToClosestPlayer` | ITEMS.OBJ PDB and distance scan |
| `fn_80063854` | `did_generate` | ITEMS.OBJ PDB and owner scan |
| `fn_80063ABC` | `DeleteItem` | ITEMS.OBJ PDB and complete teardown |
| `fn_80063C58` | `SafeRockSetup` | ITEMS.OBJ PDB and boss setup |
| `fn_80063D0C` | `SafeRockActive` | ITEMS.OBJ PDB and state predicate |
| `fn_80063D40` | `SafeRockActivate` | ITEMS.OBJ PDB and reactivation |
| `fn_80063F10` | `CollectSafeRocks` | GC-only behavior; sole caller activates returned rocks |
| `fn_80064154` | `AddItemSub` | ITEMS.OBJ PDB and item finalization |
| `fn_80064390` | `LinkItemTriggers` | GC behavior; validates trigger IDs and builds successor chains |
| `fn_80065D98` | `ItemVisible` | ITEMS.OBJ PDB and player-count gate |
| `fn_80065E28` | `RegisterItemWobj` | GC behavior; registers the five parallel item-WOBJ runtime arrays |
| `fn_80066080` | `update_player_milestone` | ITEMS.OBJ PDB; player milestone-history update |
| `fn_80067904` | `InitLighting` | LIGHTS.OBJ PDB |
| `fn_8006799C` | `DoLighting` | LIGHTS.OBJ PDB |
| `fn_8006EC18` | `CurTransmitterBlink` | NEWCAM.OBJ PDB and debug-arrow behavior |
| `fn_80093B04` | `StartFXNoLoop` | SFX PDB roster and one-shot spawn |
| `fn_800B8E94` | `MBOX_ReallyFindObject` | already mapped MB_MODEL symbol |
| `fn_800B92B0` | `MBSetObject` | already mapped MB_OBJECTS symbol |
| `fn_800BD050` | `CreateYPRMatrix` | ML_FMATH PDB order and decompilation |
| `fn_800BD3E8` | `FixAngle` | ML_FMATH PDB order and decompilation |
| `fn_800C0CF4` | `pbResetWindowPool` | GC-only behavior |
| `fn_800C0DDC` | `pbSetWindowUV0` | GC-only behavior |
| `fn_800C0DF4` | `pbSetWindowUV1` | GC-only behavior |

## Item-owned data and strings

| Old symbol | Name | Basis |
|---|---|---|
| `lbl_80112AF4` | `sRuneCameraVariants` | camera variant table |
| `lbl_80112D04` | `sMissingLookoutParamFmt` | DOL string |
| `lbl_80112D20` | `sUnableToAddItemFmt` | DOL string |
| `lbl_80112D38` | `sSafeRockBoss41ObjectName` | `"G5BIGDIRT"` |
| `lbl_80112D44` | `sSafeRockBoss44ObjectName` | `"H4NSFFXL_PURPLE"` |
| `lbl_80112D54` | `sNewItemBadIndex` | `"NewItem: bad index"` fatal-error string |
| `lbl_80112D68` | `sBadItemFloorPosFmt` | DOL string |
| `lbl_80112D98` | `sMaxItemsError` | `"> MAX ITEMS"` error block |
| `lbl_80112E24` | `sTransporterNoDestFmt` | DOL string |
| `lbl_80112FC8` | `sTriggerCameraConflictFmt` | DOL string |
| `lbl_80112FF4` | `sDeathIconName` | `"DEATH_ICON"` |
| `lbl_8011C8A8` | `sArrowObjectNames` | `add_arrow` name table |
| `lbl_8011BD40` | `sEnemyDefaultAlgorithm` | ENEMY.OBJ per-type fallback algorithm table |
| `lbl_80124D14` | `crystal_order` | ITEMS.OBJ PDB |
| `lbl_802577F0` | `sItemRuntime` | item WOBJ arrays and shared load scratch |
| `lbl_8025EA10` | `sItemWobjTargets` | 150-entry item-WOBJ target-pointer table |
| `lbl_80258400` | `sPlayerStartPositions` | 14 position vectors |
| `lbl_802584A8` | `sLookoutParams` | lookout/waypoint records |
| `lbl_80258D18` | `sSumnerCameras` | `SumnerCamActivate` table |
| `lbl_80258DC0` | `sRuneCameras` | `RuneCamActivate` table |
| `lbl_80258E04` | `sTriggerCameras` | trigger-camera records |
| `lbl_8025B604` | `sMilestones` | milestone records |
| `lbl_803448E0` | `sUnusedResetState` | reset-only GC state |
| `lbl_803448BC` | `gNumType7Items` | count incremented by `SetItem`'s type-7 constructor |
| `lbl_803448E4` | `sCrystalCamera` | `CrystalCamActivate` pointer |
| `lbl_803448F0` | `sVisibleSumCoinCount` | visible type-1/subtype-1 item count built by `AddItemInstList` |
| `lbl_803448F4` | `sShownMilestones` | current milestone overlay state |
| `lbl_803448F8` | `sShownCameras` | current camera overlay state |
| `lbl_803448FC` | `sLastPlayerStart` | highest valid player-start index |
| `lbl_80344900` | `sNumLookoutParams` | lookout/waypoint count |
| `lbl_80344908` | `sWindowCameras` | `WindowCamActivate` table |
| `lbl_80344918` | `sNumTriggerCameras` | trigger-camera count |
| `lbl_8034491C` | `sNumMilestones` | milestone count |
| `lbl_80344920` | `sSpecialItem13` | item subtype-13 singleton |
| `lbl_80344924` | `sSpecialItem10` | item subtype-10 singleton |
| `lbl_80344928` | `sUnusedItemState` | reset-only GC state |
| `lbl_8034492C` | `sNumItemWobjs` | item WOBJ count |
| `lbl_80344930` | `sDeathItemInfo` | type-2/subtype-47 definition |
| `lbl_80344934` | `sKeyringAtree` | `"KEYRING"` atree match |
| `lbl_80344938` | `sDeathIconAtree` | `"DEATH_ICON"` atree match |
| `lbl_8034493C` | `sChestAtree` | `"CHESTSG"` atree match |
| `lbl_8034494C` | `sNumItems` | item-pool high-water count |
| `lbl_80344954` | `sPreviousSafeRockCount` | safe-rock transition state |
| `lbl_80344958` | `sSafeRockCount` | safe-rock setup/runtime count |
| `lbl_8034495C` | `default_gen_count` | ITEMS.OBJ PDB and generator gate |
| `lbl_8034497C` | `sItemsRootNode` | item scene-tree root |
| `lbl_80346DC0` | `sWindowCameraVariant0` | value 300 |
| `lbl_80346DC4` | `sWindowCameraVariant1` | value 300 |
| `lbl_80346EE0` | `sNoDistance` | `-1.0f` distance sentinel |
| `lbl_80346EE4` | `sItemZero` | shared item `0.0f` |
| `lbl_80346F3C` | `sItemFloorRadius` | `1.0f` floor probe radius |
| `lbl_80346F48` | `sItemFloorYOffset` | `0.1` floor offset |
| `lbl_80346F78` | `sLevelOneSuffix` | `"L1"` |
| `lbl_80346F7C` | `sRootSuffix` | `"ROOT"` |
| `lbl_80346FB0` | `sArrowFloorYOffset` | `0.5` |
| `lbl_80346FC0` | `sZeroDouble` | shared item `0.0` |
| `lbl_80346FF4` | `sKeyringName` | `"KEYRING"` |
| `lbl_80346FFC` | `sArrowFloorRadius` | `0.2f` |
| `lbl_80347020` | `sPi` | π |
| `lbl_80347118` | `ITEM_ACTIVE_DIST` | ITEMS.OBJ PDB; value 1000 |
| `lbl_80347128` | `sItemHealthTextureFmt` | `"%s%d"` |
| `lbl_80347148` | `sMilestoneHeightTolerance` | vertical milestone acceptance threshold |
| `lbl_80347150` | `sMilestoneDistanceTolerance` | horizontal milestone acceptance threshold |
| `lbl_80347160` | `sInvalidPlayerStartY` | `-100000.0` sentinel |
| `lbl_80347168` | `sGoodWizardChestName` | `"CHESTSG"` |
| `lbl_80347170` | `sSeeThroughObjectName` | `"SEETHRU"` |

## Lighting and shared engine state

| Old symbol | Name | Basis |
|---|---|---|
| `lbl_80127D60` | `gIdentityMatrix` | shared 4×4 identity matrix |
| `lbl_8023CAE0` | `gFloorCollisionResult` | world collision result |
| `lbl_80251364` | `gWadAtreeHeaders` | WAD atree-header array |
| `lbl_80257590` | `gGameOptions` | 12-word game option/override block |
| `lbl_802757D4` | `gDefaultPlayerPosition` | new-camera fallback position |
| `lbl_80275AE0` | `gPlayers` | four-player record array |
| `lbl_80344568` | `gGameBusy` | global gameplay-update gate |
| `lbl_8034457C` | `gFrameTicks` | per-frame game tick count |
| `lbl_803445C8` | `gControllerButtons` | 64-bit controller/button flags |
| `lbl_80344764` | `gNumPlayers` | effective player count |
| `lbl_8034477C` | `gGameMode` | global game-state/mode id |
| `lbl_803447BC` | `gScriptedCameraState` | scripted camera sub-state |
| `lbl_80344914` | `CurTransmitter` | NEWCAM.OBJ PDB |
| `lbl_80344980` | `AmbientSpecialTime` | LIGHTS.OBJ PDB |
| `lbl_803449A0` | `gDemoMode` | demo/select restriction flag shared by player, tower, and main |
| `lbl_80344984` | `AmbientSpecialValue` | LIGHTS.OBJ PDB |
| `lbl_80344988` | `AmbientSpecialCurValue` | LIGHTS.OBJ PDB |
| `lbl_8034498C` | `sLightingScratchX` | initialized lighting scratch |
| `lbl_80344990` | `sLightingScratchY` | initialized lighting scratch |
| `lbl_80344994` | `sLightingScratchZ` | initialized lighting scratch |
| `lbl_80344998` | `sLevelAmbient` | current level ambient value |
| `lbl_8034499C` | `sLevelAmbientScale` | ambient multiplier |
| `lbl_80344B18` | `gPlayerStartYaw` | selected start orientation |
| `lbl_80344C60` | `gSumnerReady` | tower/player good-wizard readiness state |
| `lbl_80346F50` | `sCameraVisibilityRadius` | `2.0f` camera-mode visibility sphere radius |
| `lbl_80346FC8` | `sNewtonThree` | `3.0` reciprocal-square-root refinement constant |
| `lbl_80347038` | `sNoNearbyPlayerDistance` | `1000.0f` initial closest-player sentinel |
| `lbl_8034709C` | `sItemSearchDistance` | shared `10.0f` item/world-node search distance |
| `lbl_80347120` | `sCameraDistanceLimit` | `100.0` camera-mode proximity cutoff |

## Player milestone ABI

`update_player_milestone` takes the active `Player*` in `r3`; its former
zero-argument declaration was an ABI error. The routine reads the player index
at `+0x0000` and maintains a five-entry `s32 milestone[5]` history at
`+0x0A34`. It queries `ShowMilestones(-1)`, clears the old milestone-node
flags before shifting the history, and then applies the five-bit
`sShownMilestones` visibility mask to the retained node handles.

The reconstructed body is instruction-count identical to the 0x1EC-byte
target. Residual differences are register coloring, two equivalent
three-instruction node-address forms, and adjacent load scheduling.

## Item-instance and trigger pipeline

`iteminst` is now typed from the Xbox PDB and verified against every access in
`AddItemInstList`: it is a 0x3C-byte compact placement record with the
definition index at `+0x00`, position at `+0x18`, Euler rotation at `+0x24`,
and twelve type-specific parameter bytes at `+0x30`.

`AddItemInstList` expands those records into the 0xF0-byte live `Item` pool.
Its reconstructed body has the target's exact 100-instruction count. It seeds
the item RNG, reserves 500 spare entries, builds each placement matrix, calls
`SetItem`, counts visible sum-coins, finalizes the pool, matches transporters,
and invokes `LinkItemTriggers`.

`LinkItemTriggers` identifies the type-5 fields in `Item.data`: flags at
`+0x04`, trigger ID at `+0x06`, successor ID at `+0x07`, and successor pointer
at `+0x08`. The translation reproduces all functional target operations. The
retail code also contains a five-instruction comparison of a candidate's
successor ID with the current trigger ID whose condition result is immediately
discarded; the portable translation intentionally omits that dead comparison
and is 156 instructions versus the target's 161.

`RegisterItemWobj` reveals the item-WOBJ storage as five parallel 150-entry
float arrays (X, node Y, secondary X, Z, and value) followed by a 150-entry
target-pointer table at runtime offset `0x7220`. Its translation is
instruction-count identical at 139 instructions.

## Generator placement pipeline

The two previously anonymous entry points at the start of the GC module are
the PDB-local `place_logic12` (`0x800631AC`) and `generate_single`
(`0x80063444`). Both translations are instruction-for-instruction exact.

`generate_single` consumes the generator payload in `Item.data`, spawns from
the item's collision position, initializes `Enemy.birth_style`, `algorithm`,
facing, and `birth_pos`, then increments the generated count. `place_logic12`
takes that payload plus the newly allocated enemy index, tries offsets of
`3.5f` at `generator_angle ± pi/2`, records which side succeeded in
`Enemy.flag1`, and advances the payload phase/count.

The associated constants are now mapped as `sTwoPi` (`0x80347028`),
`sNegativePi` (`0x80347030`), `sHalfPi` (`0x80347108`), and
`sLogic12Distance` (`0x80347110`).

## Locator runtime map

`AddLocatorInstList` is now translated from its 0x1C `locator` input records.
It is a zero-argument loader over `gWorldInfo.locators/nlocators`, not the old
placeholder list/count API. Its portable body is 421 instructions versus the
419-instruction target and implements every locator kind.

The translation resolves the full `sItemRuntime` middle region:

| Runtime offset | Type/count | Purpose |
|---|---:|---|
| `0x0BB8` | `char[32]` | item load path scratch |
| `0x0BD8` | `f32[14]` | player-start yaw |
| `0x0C10` | `f32[14][3]` | player-start positions |
| `0x0CB8` | `LookoutParam[20]` | lookout matrices and links |
| `0x1528` | `TriggerCamera*[3][14]` | Sumner camera variants |
| `0x15D0` | `TriggerCamera*[17]` | rune/player transmitters |
| `0x1614` | `TriggerCamera[256]` | trigger-camera records |
| `0x3E14` | `MilestoneParam[128]` | milestone arrow matrices |
| `0x7220` | `void*[150]` | existing item-WOBJ targets |

The newly identified state symbols are `gNumTransmitters` (`0x80344544`),
`sLastTransmitter` (`0x80344904`), `sSpecialTransmitter` (`0x80344910`), and
the float initialization sentinel `sInvalidPlayerStartYFloat`
(`0x80347158`).
| `lbl_80344EB8` | `gSceneRoot` | default MB scene root |
| `lbl_80347180` | `sOne` | `1.0f` |
| `lbl_80347184` | `sNegativeHalf` | `-0.5f` |
| `lbl_80347188` | `sLightingZero` | `0.0f` |
| `lbl_8034718C` | `sNegativeOne` | `-1.0f` |
| `lbl_80347190` | `sAmbientMinimum` | `0.0` |
| `lbl_80347198` | `sAmbientDecay` | `0.6` |
| `lbl_803471A0` | `sAmbientBrightenStep` | `0.05` |
| `lbl_803471A8` | `sAmbientDarkenStep` | `-0.25` |
| `lbl_803471B0` | `sAmbientMaximum` | `1.0` |
