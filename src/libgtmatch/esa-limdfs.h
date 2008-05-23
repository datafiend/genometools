/*
  Copyright (c) 2007 Stefan Kurtz <kurtz@zbh.uni-hamburg.de>
  Copyright (c) 2007 Center for Bioinformatics, University of Hamburg

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

#ifndef ESA_LIMDFS_H
#define ESA_LIMDFS_H

#include "seqpos-def.h"
#include "encseq-def.h"

typedef struct Limdfsresources Limdfsresources;

Limdfsresources *newLimdfsresources(unsigned int mapsize);

void freeLimdfsresources(Limdfsresources **ptrlimdfsresources);

void esalimiteddfs(Limdfsresources *limdfsresources,
                   const Encodedsequence *encseq,
                   const Seqpos *suftab,
                   const Uchar *pattern,
                   unsigned long patternlength,
                   unsigned long maxdistance);

#endif
