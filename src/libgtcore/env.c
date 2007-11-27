/*
  Copyright (c) 2007 Gordon Gremme <gremme@zbh.uni-hamburg.de>
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

#include "libgtcore/cstr.h"
#include "libgtcore/env.h"
#include "libgtcore/option.h"
#include "libgtcore/splitter.h"
#include "libgtcore/versionfunc.h"
#include "libgtcore/warning.h"
#include "libgtcore/xansi.h"

struct Env {
  MA *ma; /* the memory allocator */
  FA *fa; /* the file allocator */
  Error *error;
  bool spacepeak;
};

static OPrval parse_env_options(int argc, const char **argv, Env *env)
{
  OptionParser *op;
  Option *o;
  OPrval oprval;
  assert(env);
  op = option_parser_new("GT_ENV_OPTIONS='[option ...]' ...",
                         "Parse the options contained in the "
                         "environment variable GT_ENV_OPTIONS.", env);
  o = option_new_bool("spacepeak", "show space peak on stdout upon deletion",
                      &env->spacepeak, false, env);
  option_parser_add_option(op, o, env);
  oprval = option_parser_parse_max_args(op, NULL, argc, argv, versionfunc, 0,
                                        env);
  option_parser_delete(op, env);
  return oprval;
}

static void proc_gt_env_options(Env *env)
{
  int argc;
  char *env_options, **argv;
  Splitter *splitter;
  assert(env);
  /* construct argument vector from $GT_ENV_OPTIONS */
  env_options = getenv("GT_ENV_OPTIONS");
  if (!env_options)
    return;
  env_options = cstr_dup(env_options, env); /* make writeable copy */
  splitter = splitter_new(env);
  splitter_split(splitter, env_options, strlen(env_options), ' ', env);
  argc = splitter_size(splitter);
  argv = cstr_array_preprend((const char**) splitter_get_tokens(splitter),
                             "env", env);
  argc++;
  /* parse options contained in $GT_ENV_OPTIONS */
  switch (parse_env_options(argc, (const char**) argv, env)) {
    case OPTIONPARSER_OK: break;
    case OPTIONPARSER_ERROR:
      fprintf(stderr, "error parsing $GT_ENV_OPTIONS: %s\n",
              env_error_get(env));
      env_error_unset(env);
      break;
    case OPTIONPARSER_REQUESTS_EXIT: break;
  }
  env_ma_free(env_options, env);
  splitter_delete(splitter, env);
  cstr_array_delete(argv, env);
}

Env* env_new(void)
{
  const char *bookkeeping;
  Env *env = xcalloc(1, sizeof (Env));
  env->ma = ma_new();
  bookkeeping = getenv("GT_MEM_BOOKKEEPING");
  ma_init(env->ma, bookkeeping && !strcmp(bookkeeping, "on"), env);
  env->fa = fa_new(env);
  env->error = error_new(env->ma);
  proc_gt_env_options(env);
  if (env->spacepeak && !(bookkeeping && !strcmp(bookkeeping, "on")))
    warning("GT_ENV_OPTIONS=-spacepeak used without GT_MEM_BOOKKEEPING=on");
  return env;
}

MA* env_ma(const Env *env)
{
  assert(env && env->ma);
  return env->ma;
}

FA* env_fa(const Env *env)
{
  assert(env && env->fa);
  return env->fa;
}

Error* env_error(const Env *env)
{
  assert(env);
  return env->error;
}

void env_set_spacepeak(Env *env, bool spacepeak)
{
  assert(env);
  env->spacepeak = spacepeak;
}

int env_delete(Env *env)
{
  int fa_fptr_rval, fa_mmap_rval, ma_rval;
  assert(env);
  error_delete(env->error, env->ma);
  env->error = NULL;
  if (env->spacepeak) {
    ma_show_space_peak(env->ma, stdout);
    fa_show_space_peak(env->fa, stdout);
  }
  fa_fptr_rval = fa_check_fptr_leak(env->fa, env);
  fa_mmap_rval = fa_check_mmap_leak(env->fa, env);
  fa_delete(env->fa, env);
  ma_rval = ma_check_space_leak(env->ma, env);
  ma_clean(env->ma, env);
  ma_delete(env->ma);
  free(env);
  return fa_fptr_rval || fa_mmap_rval || ma_rval;
}

void env_ma_free_func(void *ptr, Env *env)
{
  assert(env);
  if (!ptr) return;
  ma_free(ptr, env_ma(env));
}

void env_fa_fclose(FILE *stream, Env *env)
{
  assert(env);
  if (!stream) return;
  fa_fclose(stream, env_fa(env));
}

void env_fa_xfclose(FILE *stream, Env *env)
{
  assert(env);
  if (!stream) return;
  fa_xfclose(stream, env_fa(env));
}

void env_fa_gzclose(gzFile stream, Env *env)
{
  assert(env);
  if (!stream) return;
  fa_gzclose(stream, env_fa(env));
}

void env_fa_xgzclose(gzFile stream, Env *env)
{
  assert(env);
  if (!stream) return;
  fa_xgzclose(stream, env_fa(env));
}

void env_fa_bzclose(BZFILE *stream, Env *env)
{
  assert(env);
  if (!stream) return;
  fa_bzclose(stream, env_fa(env));
}

void env_fa_xbzclose(BZFILE *stream, Env *env)
{
  assert(env);
  if (!stream) return;
  fa_xbzclose(stream, env_fa(env));
}

void env_fa_xmunmap(void *addr, Env *env)
{
  assert(env);
  if (!addr) return;
  fa_xmunmap(addr, env_fa(env));
}

void env_error_set(Env *env, const char *format, ...)
{
  va_list ap;
  assert(env && format);
  va_start(ap, format);
  error_vset(env_error(env), format, ap);
  va_end(ap);
}

void env_log_log(Env *env, const char *format, ...)
{
  va_list ap;
  assert(env && format);
  if (!log_enabled()) return;
  va_start(ap, format);
  log_vlog(format, ap);
  va_end(ap);
}
