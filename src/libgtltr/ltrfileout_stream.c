/*
  Copyright (c) 2008 Sascha Steinbiss <ssteinbiss@zbh.uni-hamburg.de>
  Copyright (c) 2008 Center for Bioinformatics, University of Hamburg

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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "libgtcore/ma.h"
#include "libgtcore/range.h"
#include "libgtcore/str.h"
#include "libgtcore/unused.h"
#include "libgtext/genome_stream_rep.h"
#include "libgtext/genome_feature.h"
#include "libgtext/genome_feature_type.h"
#include "libgtext/genome_node_iterator.h"
#include "libgtltr/ltrfileout_stream.h"
#include "libgtltr/ltr_visitor.h"

struct LTRFileOutStream {
  const GenomeStream parent_instance;
  GenomeStream *in_stream;
  Bioseq *bioseq;
  FILE *fp;
  LTRVisitor *lv;
  LTRElement element;
};

#define ltr_fileout_stream_cast(GS)\
        genome_stream_cast(ltr_fileout_stream_class(), GS)

int ltr_fileout_stream_next_tree(GenomeStream *gs, GenomeNode **gn,
                               Error *e)
{
  LTRFileOutStream *ls;
  Range lltr_rng, rltr_rng, rng, ppt_rng, pbs_rng;
  char *ppt_seq, *pbs_seq;
  Str *pdoms;
  int had_err;

  error_check(e);
  ls = ltr_fileout_stream_cast(gs);

  /* initialize this element */
  memset(&ls->element, 0, sizeof (LTRElement));
  ls->element.pdoms = array_new(sizeof (GenomeFeature*));

  /* get annotations from parser */
  had_err = genome_stream_next_tree(ls->in_stream, gn, e);
  if (!had_err && *gn)
  {
    GenomeNodeIterator* gni;
    GenomeNode *mygn;
    /* fill LTRElement structure from GFF3 subgraph */
    gni = genome_node_iterator_new(*gn);
    for (mygn = *gn; mygn; mygn = genome_node_iterator_next(gni))
      genome_node_accept(mygn, (GenomeVisitor*) ls->lv, e);
    genome_node_iterator_delete(gni);
  }

  if (ls->element.mainnode)
  {
    unsigned long seqid, i;

    /* find sequence */
    const char *sreg = str_get(genome_node_get_seqid((GenomeNode*)
                                                     ls->element.mainnode));
    sscanf(sreg,"seq%lu", &seqid);
    Seq *seq = bioseq_get_seq(ls->bioseq, seqid);

    lltr_rng = genome_node_get_range((GenomeNode*) ls->element.leftLTR);
    rltr_rng = genome_node_get_range((GenomeNode*) ls->element.rightLTR);

    /* output basic retrotransposon data */
    rng = genome_node_get_range((GenomeNode*) ls->element.mainnode);
    fprintf(ls->fp, "%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t",
                    rng.start,
                    rng.end,
                    ltrelement_length(&ls->element),
                    lltr_rng.start,
                    lltr_rng.end,
                    ltrelement_leftltrlen(&ls->element),
                    rltr_rng.start,
                    rltr_rng.end,
                    ltrelement_rightltrlen(&ls->element));

    /* output PPT */
    if (ls->element.ppt)
    {
      Strand ppt_strand;
      ppt_strand = genome_feature_get_strand(ls->element.ppt);
      ppt_rng = genome_node_get_range((GenomeNode*) ls->element.ppt);
      ppt_seq = ltrelement_get_sequence(ppt_rng.start, ppt_rng.end,
                                        ppt_strand,
                                        seq, e);
      fprintf(ls->fp, "%lu\t%lu\t%s\t%c\t%d\t", ppt_rng.start, ppt_rng.end,
                      ppt_seq,
                      STRANDCHARS[ppt_strand],
                      (ppt_strand == STRAND_FORWARD ?
                          abs(rltr_rng.start - ppt_rng.end) :
                          abs(lltr_rng.end - ppt_rng.start)));
      ma_free((char*) ppt_seq);
    } else fprintf(ls->fp, "\t\t\t\t\t");

    /* output PBS */
    if (ls->element.pbs)
    {
      Strand pbs_strand;
      pbs_strand = genome_feature_get_strand(ls->element.pbs);
      pbs_rng = genome_node_get_range((GenomeNode*) ls->element.pbs);
      pbs_seq = ltrelement_get_sequence(pbs_rng.start, pbs_rng.end,
                                        pbs_strand,
                                        seq, e);
      fprintf(ls->fp, "%lu\t%lu\t%c\t%s\t%s\t%s\t%s\t%s\t",
                      pbs_rng.start,
                      pbs_rng.end,
                      STRANDCHARS[pbs_strand],
                      genome_feature_get_attribute((GenomeNode*)
                                              ls->element.pbs, "trna"),
                      pbs_seq,
                      genome_feature_get_attribute((GenomeNode*)
                                              ls->element.pbs, "trnaoffset"),
                      genome_feature_get_attribute((GenomeNode*)
                                              ls->element.pbs, "pbsoffset"),
                      genome_feature_get_attribute((GenomeNode*)
                                              ls->element.pbs, "edist"));
      ma_free((char*) pbs_seq);
    } else fprintf(ls->fp, "\t\t\t\t\t\t\t\t");

    /* output protein domains */
    pdoms = str_new();
    if(STRAND_REVERSE == genome_feature_get_strand(ls->element.mainnode))
      array_reverse(ls->element.pdoms);
    for (i=0;i<array_size(ls->element.pdoms);i++)
    {
      GenomeFeature *gf = *(GenomeFeature**) array_get(ls->element.pdoms, i);
      str_append_cstr(pdoms,
                      genome_feature_get_attribute((GenomeNode*)gf,
                                                   "pfamname"));
      str_append_cstr(pdoms,"(");
      str_append_ulong(pdoms, (unsigned long) genome_feature_get_phase(gf));
      str_append_char(pdoms, STRANDCHARS[genome_feature_get_strand(gf)]);

      str_append_cstr(pdoms,")");
      if (i != array_size(ls->element.pdoms)-1)
        str_append_cstr(pdoms, "/");
    }
    fprintf(ls->fp,"%s", str_get(pdoms));
    str_delete(pdoms);
    fprintf(ls->fp, "\n");
  }
  array_delete(ls->element.pdoms);
  fflush(ls->fp);
  return had_err;
}

void ltr_fileout_stream_free(GenomeStream *gs)
{
  LTRFileOutStream *ls = ltr_fileout_stream_cast(gs);
  genome_visitor_delete((GenomeVisitor*) ls->lv);
  genome_stream_delete(ls->in_stream);
}

const GenomeStreamClass* ltr_fileout_stream_class(void)
{
  static const GenomeStreamClass gsc = { sizeof (LTRFileOutStream),
                                         ltr_fileout_stream_next_tree,
                                         ltr_fileout_stream_free };
  return &gsc;
}

GenomeStream* ltr_fileout_stream_new(GenomeStream *in_stream,
                                     Bioseq *bioseq,
                                     FILE *fp,
                                     bool with_metadata,
                                     PPTOptions *ppt_opts,
                                     PBSOptions *pbs_opts,
                                     PdomOptions *pdom_opts,
                                     const char *trnafilename,
                                     const char *seqfilename,
                                     const char *gfffilename)
{
  GenomeStream *gs;
  LTRFileOutStream *ls;
  char buffer[PATH_MAX+1];

  assert(fp && in_stream && bioseq && ppt_opts && pbs_opts && pdom_opts);

  gs = genome_stream_create(ltr_fileout_stream_class(), true);
  ls = ltr_fileout_stream_cast(gs);
  ls->in_stream = genome_stream_ref(in_stream);
  ls->bioseq = bioseq;
  ls->fp = fp;

  if (with_metadata)
  {
    unsigned long i;
    bool has_cwd;
    /* get current working dir */
    memset(buffer, 0, PATH_MAX+1);
    has_cwd = (getcwd(buffer, PATH_MAX) != NULL);
    /* append working dir to relative paths if necessary */
    if (seqfilename[0] != '/' && has_cwd)
      fprintf(fp, "Sequence file used\t%s/%s\n", buffer, seqfilename);
    else
      fprintf(fp, "Sequence file used\t%s\n", seqfilename);
    if (gfffilename[0] != '/' && has_cwd)
      fprintf(fp, "GFF3 input used\t%s/%s\n", buffer, gfffilename);
    else
      fprintf(fp, "GFF3 input used\t%s\n", gfffilename);
    fprintf(fp, "PPT minimum length\t%i\t6\n", ppt_opts->ppt_minlen);
    fprintf(fp, "U-box minimum length\t%d\t3\n", ppt_opts->ubox_minlen);
    fprintf(fp, "PPT search radius\t%d\t30\n", ppt_opts->radius);
    if (strcmp(trnafilename, "") != 0)
    {
      if (trnafilename[0] != '/' && has_cwd)
        fprintf(fp, "tRNA library for PBS detection\t%s/%s\n",
                                              buffer, trnafilename);
      else
        fprintf(fp, "tRNA library for PBS detection\t%s\n",
                                              trnafilename);
      fprintf(fp, "PBS/tRNA minimum alignment length\t%d\t11\n",
                                              pbs_opts->ali_min_len);
      fprintf(fp, "PBS/tRNA maximal unit edit distance\t%d\t1\n",
                                              pbs_opts->max_edist);
      fprintf(fp, "PBS max offset from 5' LTR end\t%d\t5\n",
                                              pbs_opts->max_offset);
      fprintf(fp, "tRNA max offset from 3' end\t%d\t10\n",
                                              pbs_opts->max_offset_trna);
      fprintf(fp, "PBS search radius\t%d\t30\n", pbs_opts->radius);
    }
    if (array_size(pdom_opts->plan7_ts) > 0)
    {
      fprintf(fp, "Protein domains\t%lu (", array_size(pdom_opts->plan7_ts));
      for (i=0;i<array_size(pdom_opts->plan7_ts);i++)
      {
        struct plan7_s *model = *(struct plan7_s **)
                                        array_get(pdom_opts->plan7_ts, i);
        fprintf(fp, "%s", model->name);
        if (i != array_size(pdom_opts->plan7_ts)-1)
          fprintf(fp, ", ");
      }
      fprintf(fp, ")\n");
      fprintf(fp, "pHMM e-value cutoff \t%g\t1.0e-6\n",
                                         pdom_opts->evalue_cutoff);
    }
    fprintf(fp, "\n");
  }

  /* print headline */
  fprintf(fp, "element start\telement end\telement length\t"
              "lLTR start\tlLTR end\tlLTR length\t"
              "rLTR start\trLTR end\trLTR length\t"
              "PPT start\tPPT end\tPPT motif\tPPT strand\tPPT offset\t");
  if (strcmp(trnafilename, "") != 0)
    fprintf(fp, "PBS start\tPBS end\tPBS strand\ttRNA\tRNA motif\tPBS offset\t"
                "tRNA offset\tPBS/tRNA edist\t");
  if (array_size(pdom_opts->plan7_ts) > 0)
    fprintf(fp, "Protein domain hits\n");

  ls->lv = (LTRVisitor*) ltr_visitor_new(&ls->element);
  return gs;
}
