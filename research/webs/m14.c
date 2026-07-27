/* m04: FatalError arm kept, table code removed */
typedef unsigned int u32; typedef signed int s32; typedef unsigned char u8;
typedef struct MBObject { u8 pad0[82]; u8 type; u8 pad1[13]; u32 flags; u8 pad2[8]; s32 index; void* romobj; } MBObject;
extern void* fn_800BB29C(void*, void*, int);
extern void FatalError(const char*, int);
extern u8* gWinGlobals;
extern void* lbl_80344EBC; extern void* lbl_80344EB8;
extern const char str_BadMBSetObject[];
MBObject* MBNewObject(s32 objid, void* name, void* parent, u32 flags) {
    MBObject* obj;
    if (parent == 0) parent = (flags & 0x00002000) ? lbl_80344EBC : lbl_80344EB8;
    if (objid == -1) {
        obj = (MBObject*)fn_800BB29C(parent, name, 1);
    } else {
        obj = (MBObject*)fn_800BB29C(parent, name, 2);
        if (obj != 0) {
            if (objid < 0) {
                FatalError(str_BadMBSetObject, 0x800000);
                obj->index = objid;
                obj->romobj = 0;
            } else {
                u8* mgr = gWinGlobals;
                void** table = *(void***)(mgr + 0x30);
                table = (void**)table[(objid >> 16) * 4 + 1];
                obj->index = objid;
                obj->romobj = (u8*)0 + objid;
                obj->type = 2;
            }
            obj->flags |= flags;
        }
    }
    return obj;
}
