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
| `fn_80065D98` | `ItemVisible` | ITEMS.OBJ PDB and player-count gate |
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
| `lbl_80112D68` | `sBadItemFloorPosFmt` | DOL string |
| `lbl_80112D98` | `sMaxItemsError` | `"> MAX ITEMS"` error block |
| `lbl_80112E24` | `sTransporterNoDestFmt` | DOL string |
| `lbl_80112FC8` | `sTriggerCameraConflictFmt` | DOL string |
| `lbl_80112FF4` | `sDeathIconName` | `"DEATH_ICON"` |
| `lbl_8011C8A8` | `sArrowObjectNames` | `add_arrow` name table |
| `lbl_80124D14` | `crystal_order` | ITEMS.OBJ PDB |
| `lbl_802577F0` | `sItemRuntime` | item WOBJ arrays and shared load scratch |
| `lbl_80258400` | `sPlayerStartPositions` | 14 position vectors |
| `lbl_802584A8` | `sLookoutParams` | lookout/waypoint records |
| `lbl_80258D18` | `sSumnerCameras` | `SumnerCamActivate` table |
| `lbl_80258DC0` | `sRuneCameras` | `RuneCamActivate` table |
| `lbl_80258E04` | `sTriggerCameras` | trigger-camera records |
| `lbl_8025B604` | `sMilestones` | milestone records |
| `lbl_803448E0` | `sUnusedResetState` | reset-only GC state |
| `lbl_803448E4` | `sCrystalCamera` | `CrystalCamActivate` pointer |
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
| `lbl_80344984` | `AmbientSpecialValue` | LIGHTS.OBJ PDB |
| `lbl_80344988` | `AmbientSpecialCurValue` | LIGHTS.OBJ PDB |
| `lbl_8034498C` | `sLightingScratchX` | initialized lighting scratch |
| `lbl_80344990` | `sLightingScratchY` | initialized lighting scratch |
| `lbl_80344994` | `sLightingScratchZ` | initialized lighting scratch |
| `lbl_80344998` | `sLevelAmbient` | current level ambient value |
| `lbl_8034499C` | `sLevelAmbientScale` | ambient multiplier |
| `lbl_80344B18` | `gPlayerStartYaw` | selected start orientation |
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

