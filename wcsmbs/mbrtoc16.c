/* Copyright (C) 2011, 2012 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@gmail.com>, 2011.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <gconv.h>
#include <uchar.h>
#include <wcsmbsload.h>

#include <sysdep.h>

#ifndef EILSEQ
# define EILSEQ EINVAL
#endif

#if __STDC__ >= 201000L
# define U(c) U##c
#else
# define U(c) L##c
#endif


/* This is the private state used if PS is NULL.  */
static mbstate_t state;

size_t
mbrtoc16 (char16_t *pc16, const char *s, size_t n, mbstate_t *ps)
{
  if (ps == NULL)
    ps = &state;

  if (ps->__count & 0x80000000)
    {
      /* We have to return the second word for a surrogate.  */
      ps->__count &= 0x7fffffff;
      *pc16 = ps->__value.__wch;
      ps->__value.__wch = L'\0';
      return (size_t) -3;
    }

  char16_t buf[2];
  struct __gconv_step_data data;
  int status;
  size_t result;
  size_t dummy;
  const unsigned char *inbuf, *endbuf;
  unsigned char *outbuf = (unsigned char *) buf;
  const struct gconv_fcts *fcts;

  /* Set information for this step.  */
  data.__invocation_counter = 0;
  data.__internal_use = 1;
  data.__flags = __GCONV_IS_LAST;
  data.__statep = ps;
  data.__trans = NULL;

  /* A first special case is if S is NULL.  This means put PS in the
     initial state.  */
  if (s == NULL)
    {
      outbuf = (unsigned char *) buf;
      s = "";
      n = 1;
    }

  /* Tell where we want the result.  */
  data.__outbuf = outbuf;
  data.__outbufend = outbuf + sizeof (char16_t);

  /* Get the conversion functions.  */
  fcts = get_gconv_fcts (_NL_CURRENT_DATA (LC_CTYPE));

  /* Do a normal conversion.  */
  inbuf = (const unsigned char *) s;
  endbuf = inbuf + n;
  if (__builtin_expect (endbuf < inbuf, 0))
    endbuf = (const unsigned char *) ~(uintptr_t) 0;
  __gconv_fct fct = fcts->toc16->__fct;
#ifdef PTR_DEMANGLE
  if (fcts->toc16->__shlib_handle != NULL)
    PTR_DEMANGLE (fct);
#endif

  /* We first have to check whether the character can be represented
     without a surrogate.  If we immediately pass in a buffer large
     enough to hold two char16_t values and the first character does
     not require a surrogate the routine will try to convert more
     input if N is larger then needed for the first character.  */
  status = DL_CALL_FCT (fct, (fcts->toc16, &data, &inbuf, endbuf,
			      NULL, &dummy, 0, 1));

  if (status == __GCONV_FULL_OUTPUT && data.__outbuf == outbuf)
    {
      data.__outbufend = outbuf + 2 * sizeof (char16_t);
      status = DL_CALL_FCT (fct, (fcts->toc16, &data, &inbuf, endbuf,
				  NULL, &dummy, 0, 1));
    }

  /* There must not be any problems with the conversion but illegal input
     characters.  The output buffer must be large enough, otherwise the
     definition of MB_CUR_MAX is not correct.  All the other possible
     errors also must not happen.  */
  assert (status == __GCONV_OK || status == __GCONV_EMPTY_INPUT
	  || status == __GCONV_ILLEGAL_INPUT
	  || status == __GCONV_INCOMPLETE_INPUT
	  || status == __GCONV_FULL_OUTPUT);

  if (status == __GCONV_OK || status == __GCONV_EMPTY_INPUT
      || status == __GCONV_FULL_OUTPUT)
    {
      if (pc16 != NULL)
	*pc16 = buf[0];

      if (data.__outbuf != outbuf && *(char16_t *) outbuf == U('\0'))
	{
	  /* The converted character is the NUL character.  */
	  assert (__mbsinit (data.__statep));
	  result = 0;
	}
      else
	{
	  result = inbuf - (const unsigned char *) s;

	  if (data.__outbuf != outbuf + 2)
	    {
	      /* This is a surrogate.  */
	      assert (buf[0] >= 0xd800 && buf[0] <= 0xdfff);
	      assert (buf[1] >= 0xdc00 && buf[1] <= 0xdfff);
	      ps->__count |= 0x80000000;
	      ps->__value.__wch = buf[1];
	    }
	}
    }
  else if (status == __GCONV_INCOMPLETE_INPUT)
    result = (size_t) -2;
  else
    {
      result = (size_t) -1;
      __set_errno (EILSEQ);
    }

  return result;
}