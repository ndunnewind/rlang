#include <rlang.h>

#include "internal.h"


static bool is_callable(sexp* x) {
  switch (r_typeof(x)) {
  case r_type_symbol:
  case r_type_call:
  case r_type_closure:
  case r_type_builtin:
  case r_type_special:
    return true;
  default:
    return false;
  }
}

static sexp* rlang_call2(sexp* fn, sexp* args, sexp* ns) {
  if (r_typeof(fn) == r_type_character) {
    if (r_length(fn) != 1) {
      r_abort("`.fn` must be a string, a symbol, a call, or a function");
    }
    fn = r_sym(r_chr_get_c_string(fn, 0));
  } else if (!is_callable(fn)) {
    r_abort("Can't create call to non-callable object");
  }

  int n_protect = 0;

  if (ns != r_null) {
    if (!r_is_string(ns, NULL)) {
      r_abort("`ns` must be a string");
    }
    ns = r_sym(r_chr_get_c_string(ns, 0));
    fn = KEEP_N(r_call3(r_namespace_sym, ns, fn), n_protect);
  }

  sexp* out = r_new_call(fn, args);

  FREE(n_protect);
  return out;
}

// From dots.c
sexp* dots_values_node_impl(sexp* frame_env,
                            sexp* named,
                            sexp* ignore_empty,
                            sexp* preserve_empty,
                            sexp* unquote_names,
                            sexp* homonyms,
                            sexp* check_assign,
                            bool splice);

sexp* rlang_call2_external(sexp* call, sexp* op, sexp* args, sexp* env) {
  args = r_node_cdr(args);

  sexp* fn = KEEP(r_eval(r_sym(".fn"), env));
  sexp* ns = KEEP(r_eval(r_sym(".ns"), env));

  sexp* dots = KEEP(dots_values_node_impl(env,
                                          r_shared_false,
                                          rlang_objs_trailing,
                                          r_shared_true,
                                          r_shared_true,
                                          rlang_objs_keep,
                                          r_shared_false,
                                          true));
  sexp* out = rlang_call2(fn, dots, ns);

  FREE(3);
  return out;
}
