/*

 Package: dyncall
 Library: dyncall
 File: dyncall/dyncall_callvm_x64.c
 Description: 
 License:

   Copyright (c) 2007-2020 Daniel Adler <dadler@uni-goettingen.de>, 
                           Tassilo Philipp <tphilipp@potion-studios.com>

   Permission to use, copy, modify, and distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
   ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/




/* MS Windows x64 calling convention, AMD64 SystemV ABI. */


#include "dyncall_callvm_x64.h"
#include "dyncall_alloc.h"
#include "dyncall_struct.h"


/* 
** x64 SystemV calling convention 
**
** - hybrid return-type call (bool ... pointer)
**
*/

void dcCall_x64_sysv(DCsize stacksize, DCpointer stackdata, DCpointer regdata_i, DCpointer regdata_f, DCpointer target);
void dcCall_x64_win64(DCsize stacksize, DCpointer stackdata, DCpointer regdata, DCpointer target);
void dcCall_x64_syscall_sysv(DCpointer argdata, DCpointer target);


static void dc_callvm_free_x64(DCCallVM* in_self)
{
  dcFreeMem(in_self);
}


static void dc_callvm_reset_x64(DCCallVM* in_self)
{
  DCCallVM_x64* self = (DCCallVM_x64*)in_self;
  dcVecReset(&self->mVecHead);
  self->mRegCount.i = self->mRegCount.f = 0;
}


static void dc_callvm_argLongLong_x64(DCCallVM* in_self, DClonglong x)
{
  /* A long long always has 64 bits on the supported x64 platforms (lp64 on unix and llp64 on windows). */
  DCCallVM_x64* self = (DCCallVM_x64*)in_self;
  if(self->mRegCount.i < numIntRegs)
    self->mRegData.i[self->mRegCount.i++] = x;
  else
    dcVecAppend(&self->mVecHead, &x, sizeof(DClonglong));
}


static void dc_callvm_argBool_x64(DCCallVM* in_self, DCbool x)
{
  dc_callvm_argLongLong_x64(in_self, (DClonglong)x);
}


static void dc_callvm_argChar_x64(DCCallVM* in_self, DCchar x)
{
  dc_callvm_argLongLong_x64(in_self, x);
}


static void dc_callvm_argShort_x64(DCCallVM* in_self, DCshort x)
{
  dc_callvm_argLongLong_x64(in_self, x);
}


static void dc_callvm_argInt_x64(DCCallVM* in_self, DCint x)
{
  dc_callvm_argLongLong_x64(in_self, x);
}


static void dc_callvm_argLong_x64(DCCallVM* in_self, DClong x)
{
  dc_callvm_argLongLong_x64(in_self, x);
}


static void dc_callvm_argFloat_x64(DCCallVM* in_self, DCfloat x)
{
  DCCallVM_x64* self = (DCCallVM_x64*)in_self;

  /* Although not promoted to doubles, floats are stored with 64bits in this API.*/
  union {
    DCdouble d;
    DCfloat  f;
  } f;
  f.f = x;

  if(self->mRegCount.f < numFloatRegs)
    *(DCfloat*)&self->mRegData.f[self->mRegCount.f++] = x;
  else
    dcVecAppend(&self->mVecHead, &f.f, sizeof(DCdouble));
}


static void dc_callvm_argDouble_x64(DCCallVM* in_self, DCdouble x)
{
  DCCallVM_x64* self = (DCCallVM_x64*)in_self;
  if(self->mRegCount.f < numFloatRegs)
    self->mRegData.f[self->mRegCount.f++] = x;
  else
    dcVecAppend(&self->mVecHead, &x, sizeof(DCdouble));
}


static void dc_callvm_argPointer_x64(DCCallVM* in_self, DCpointer x)
{
  DCCallVM_x64* self = (DCCallVM_x64*)in_self;
  if(self->mRegCount.i < numIntRegs)
    *(DCpointer*)&self->mRegData.i[self->mRegCount.i++] = x;
  else
    dcVecAppend(&self->mVecHead, &x, sizeof(DCpointer));
}

static void dc_callvm_argStruct_x64(DCCallVM* in_self, DCstruct* s, DCpointer x)
{
  DCCallVM_x64* self = (DCCallVM_x64*)in_self;
  dcVecAppend(&self->mVecHead, x, s->size);
  /*printf("dc_callvm_argStruct_x64 size = %d\n", (int)s->size);@@@*/
  if (s->size <= 64)
  	  dcArgStructUnroll(in_self, s, x);
  /*else@@@*/
  /*	  dcVecAppend(&self->mVecHead, &x, sizeof(DCpointer));@@@*/
}


/* Call. */
void dc_callvm_call_x64(DCCallVM* in_self, DCpointer target)
{
  DCCallVM_x64* self = (DCCallVM_x64*)in_self;
#if defined(DC_UNIX)
  dcCall_x64_sysv(
#else
  dcCall_x64_win64(
#endif
    dcVecSize(&self->mVecHead),  /* Size of stack data.                           */
    dcVecData(&self->mVecHead),  /* Pointer to stack arguments.                   */
    self->mRegData.i,            /* Pointer to register arguments (ints on SysV). */
#if defined(DC_UNIX)
    self->mRegData.f,            /* Pointer to floating point register arguments. */
#endif
    target
  );
}


static void dc_callvm_mode_x64(DCCallVM* in_self, DCint mode);

DCCallVM_vt gVT_x64 =
{
  &dc_callvm_free_x64
, &dc_callvm_reset_x64
, &dc_callvm_mode_x64
, &dc_callvm_argBool_x64
, &dc_callvm_argChar_x64
, &dc_callvm_argShort_x64
, &dc_callvm_argInt_x64
, &dc_callvm_argLong_x64
, &dc_callvm_argLongLong_x64
, &dc_callvm_argFloat_x64
, &dc_callvm_argDouble_x64
, &dc_callvm_argPointer_x64
, &dc_callvm_argStruct_x64
, (DCvoidvmfunc*)       &dc_callvm_call_x64
, (DCboolvmfunc*)       &dc_callvm_call_x64
, (DCcharvmfunc*)       &dc_callvm_call_x64
, (DCshortvmfunc*)      &dc_callvm_call_x64
, (DCintvmfunc*)        &dc_callvm_call_x64
, (DClongvmfunc*)       &dc_callvm_call_x64
, (DClonglongvmfunc*)   &dc_callvm_call_x64
, (DCfloatvmfunc*)      &dc_callvm_call_x64
, (DCdoublevmfunc*)     &dc_callvm_call_x64
, (DCpointervmfunc*)    &dc_callvm_call_x64
, NULL /* callStruct */
};


/* --- syscall ------------------------------------------------------------- */

#include <assert.h>
void dc_callvm_call_x64_syscall_sysv(DCCallVM* in_self, DCpointer target)
{
  DCCallVM_x64* self;

  /* syscalls can have up to 6 args, required to be "Only values of class INTEGER or class MEMORY" (from */
  /* SysV manual), so we can use self->mRegData.i directly; verify this has space for at least 6 values, though. */
  assert(numIntRegs >= 6);

  self = (DCCallVM_x64*)in_self;
  dcCall_x64_syscall_sysv(self->mRegData.i, target);
}

DCCallVM_vt gVT_x64_syscall_sysv =
{
  &dc_callvm_free_x64
, &dc_callvm_reset_x64
, &dc_callvm_mode_x64
, &dc_callvm_argBool_x64
, &dc_callvm_argChar_x64
, &dc_callvm_argShort_x64
, &dc_callvm_argInt_x64
, &dc_callvm_argLong_x64
, &dc_callvm_argLongLong_x64
, &dc_callvm_argFloat_x64
, &dc_callvm_argDouble_x64
, &dc_callvm_argPointer_x64
, NULL /* argStruct */
, (DCvoidvmfunc*)       &dc_callvm_call_x64_syscall_sysv
, (DCboolvmfunc*)       &dc_callvm_call_x64_syscall_sysv
, (DCcharvmfunc*)       &dc_callvm_call_x64_syscall_sysv
, (DCshortvmfunc*)      &dc_callvm_call_x64_syscall_sysv
, (DCintvmfunc*)        &dc_callvm_call_x64_syscall_sysv
, (DClongvmfunc*)       &dc_callvm_call_x64_syscall_sysv
, (DClonglongvmfunc*)   &dc_callvm_call_x64_syscall_sysv
, (DCfloatvmfunc*)      &dc_callvm_call_x64_syscall_sysv
, (DCdoublevmfunc*)     &dc_callvm_call_x64_syscall_sysv
, (DCpointervmfunc*)    &dc_callvm_call_x64_syscall_sysv
, NULL /* callStruct */
};



/* mode */

static void dc_callvm_mode_x64(DCCallVM* in_self, DCint mode)
{
  DCCallVM_x64* self = (DCCallVM_x64*)in_self;
  DCCallVM_vt* vt;

  switch(mode) {
    case DC_CALL_C_DEFAULT:
#if defined(DC_UNIX)
    case DC_CALL_C_X64_SYSV:
#else
    case DC_CALL_C_X64_WIN64:
#endif
    case DC_CALL_C_ELLIPSIS:
    case DC_CALL_C_ELLIPSIS_VARARGS:
      vt = &gVT_x64;
      break;
    case DC_CALL_SYS_DEFAULT:
# if defined DC_UNIX
    case DC_CALL_SYS_X64_SYSCALL_SYSV:
      vt = &gVT_x64_syscall_sysv; break;
# else
      self->mInterface.mError = DC_ERROR_UNSUPPORTED_MODE; return;
# endif
    default:
      self->mInterface.mError = DC_ERROR_UNSUPPORTED_MODE; 
      return;
  }
  dc_callvm_base_init(&self->mInterface, vt);
}

/* Public API. */
DCCallVM* dcNewCallVM(DCsize size)
{
  DCCallVM_x64* p = (DCCallVM_x64*)dcAllocMem(sizeof(DCCallVM_x64)+size);

  dc_callvm_mode_x64((DCCallVM*)p, DC_CALL_C_DEFAULT);

  /* Since we store register parameters in DCCallVM_x64 directly, adjust the stack size. */
  size -= sizeof(DCRegData_x64);
  size = (((signed long)size) < 0) ? 0 : size;
  dcVecInit(&p->mVecHead, size);
  dc_callvm_reset_x64((DCCallVM*)p);

  return (DCCallVM*)p;
}

