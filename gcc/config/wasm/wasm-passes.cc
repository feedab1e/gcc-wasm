/* WebAssembly passes for code generation.
Copyright (C) 2025-2025 Free Software Foundation, Inc.
   Contributed by feedable.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "regs.h"
#include "stringpool.h"
#include "attribs.h"
#include "gimple.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "optabs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "diagnostic-core.h"
#include "output.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "calls.h"
#include "explow.h"
#include "expr.h"
#include "langhooks.h"
#include "cfgrtl.h"
#include "gimplify.h"
#include "reload.h"
#include "builtins.h"
#include "tree-pass.h"
#include "cfghooks.h"

namespace {

const pass_data pass_data_count_labels =
{
  RTL_PASS, /* type */
  "count_labels", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_MACH_DEP, /* tv_id */
  0, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  0, /* todo_flags_finish */
};

class pass_count_labels : public rtl_opt_pass
{
  static void put_label (function *fun, rtx_insn *l, int &idx)
  {
    gcc_assert (!fun->machine->labels_to_cfno->put (l, idx++));
    int n = CODE_LABEL_NUMBER (l);
    gcc_assert (!fun->machine->labelno_to_labels->put (n, l));
  }
  static void mark_live (bitmap regs)
  {

    df_clear_flags (DF_LR_RUN_DCE);
    df_set_flags (DF_NO_INSN_RESCAN | DF_NO_HARD_REGS);
    df_live_add_problem ();
    df_live_set_all_dirty ();
    df_analyze ();
    //regstat_init_n_sets_and_refs ();
    for (int i = FIRST_PSEUDO_REGISTER; i < max_reg_num (); ++i)
      if (DF_REG_DEF_COUNT (i) || DF_REG_USE_COUNT (i))
        bitmap_set_bit (regs, i);
  }
  static void inject_trap (function *fun, basic_block bb)
  {
    rtx_insn *insn = BB_END (bb);
    if (INSN_P (insn)
        && (recog_memoized (insn) == CODE_FOR_trap))
      return;
    if (!vec_safe_length (bb->succs))
      make_edge (bb, fun->cfg->x_exit_block_ptr, EDGE_FALLTHRU);
    edge_iterator ei;
    edge e;
    FOR_EACH_EDGE (e, ei, bb->succs)
      if (e->flags & EDGE_FALLTHRU && cfun->machine->return_mode != VOIDmode)
        if (e->dest == fun->cfg->x_exit_block_ptr)
          {
            basic_block new_bb = split_edge (e);
            emit_insn_after (gen_trap(), BB_END (new_bb));
          }
  }
  static rtx_insn *find_label (basic_block bb)
  {
    rtx_insn *insn;
    FOR_BB_INSNS (bb, insn)
      {
        if (LABEL_P (insn))
          return insn;
        if (NOTE_P (insn) && NOTE_KIND (insn) == NOTE_INSN_DELETED_LABEL)
          return insn;
      }
    return NULL;
  }
public:
  pass_count_labels(gcc::context *ctxt)
    : rtl_opt_pass(pass_data_count_labels, ctxt)
  {}

  /* opt_pass methods: */
  unsigned int execute (function *fun) override
  {
    basic_block bb;
    FOR_EACH_BB_FN (bb, fun)
      inject_trap (fun, bb);

    bb = fun->cfg->x_entry_block_ptr->next_bb;
    int i = 0;
    put_label (fun, block_label (bb), i);
    mark_live (fun->machine->regs_ever_live);

    FOR_BB_BETWEEN (bb, bb->next_bb, fun->cfg->x_exit_block_ptr, next_bb)
      {
        if (rtx_insn *x = find_label (bb))
          put_label (fun, x, i);
      }
    return 0;
  }

};

} // anon namespace

rtl_opt_pass *make_pass_count_labels (gcc::context *ctx)
{
  return new pass_count_labels (ctx);
}
