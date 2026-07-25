#ifndef NMWEXCEPTION_H
#define NMWEXCEPTION_H

#include "types.h"
#include "__ppc_eabi_linker.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DTORCALL(dtor, objptr) (((void (*)(void*, int))dtor)(objptr, -1))

typedef s16 vbase_ctor_arg_type;
typedef char local_cond_type;

typedef struct CatchInfo {
    void* location;
    void* typeinfo;
    void* dtor;
    void* sublocation;
    s32 pointercopy;
    void* stacktop;
} CatchInfo;

typedef struct DestructorChain {
    struct DestructorChain* next;
    void* destructor;
    void* object;
} DestructorChain;

void __unregister_fragment(int fragmentID);
int __register_fragment(struct __eti_init_info* info, char* TOC);
void* __register_global_object(void* object, void* destructor, void* regmem);
void __destroy_global_chain(void);

extern void __end__catch(CatchInfo* catchinfo);
extern void __throw(char* throwtype, void* location, void* dtor);
extern char __throw_catch_compare(const char* throwtype, const char* catchtype,
                                  s32* offset_result);
extern void __unexpected(CatchInfo* catchinfo);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace std {

class exception {
public:
    exception() throw() {}
#ifdef NMWEXCEPTION_CPP
    // The runtime TU defines the destructor out-of-line (emitted first, at
    // 0x800E31D4); every other TU sees it inline so mwcc can inline it into
    // derived-class destructors (e.g. ~bad_exception in ExceptionPPC.cpp).
    virtual ~exception() throw();
#else
    virtual ~exception() throw() {}
#endif
    virtual const char* what() const;
};

class bad_exception : public exception {
public:
    bad_exception() throw() {}
    virtual ~bad_exception() throw();
    virtual const char* what() const { return "bad_exception"; }
};

typedef void (*terminate_handler)();
typedef void (*unexpected_handler)();

extern void terminate();
extern void unexpected();

} // namespace std
#endif /* __cplusplus */

#endif /* NMWEXCEPTION_H */
