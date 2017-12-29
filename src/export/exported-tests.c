#include <rlang.h>

sexp* rlang_test_r_warn(sexp* x) {
  r_warn(CHAR(STRING_ELT(x, 0)));
  return r_null;
}

sexp* rlang_r_string(sexp* str) {
  return STRING_ELT(str, 0);
}


// env.c

sexp* rlang_test_base_ns_get(sexp* name) {
  return r_base_ns_get(r_c_string(name));
}


// quo.c

sexp* rlang_test_r_quo_get_expr(sexp* quo) {
  return r_quo_get_expr(quo);
}
sexp* rlang_test_r_quo_set_expr(sexp* quo, sexp* expr) {
  return r_quo_set_expr(quo, expr);
}
sexp* rlang_test_r_quo_get_env(sexp* quo) {
  return r_quo_get_env(quo);
}
sexp* rlang_test_r_quo_set_env(sexp* quo, sexp* env) {
  return r_quo_set_env(quo, env);
}


// sym.c

sexp* rlang_test_is_special_op_sym(sexp* x) {
  return Rf_ScalarLogical(r_is_special_op_sym(x));
}


// squash.c

bool rlang_is_clevel_spliceable(sexp* x) {
  return Rf_inherits(x, "foo");
}


// stack.c

sexp* rlang_test_sys_call(sexp* n) {
  return r_sys_call(r_c_int(n), NULL);
}
sexp* rlang_test_sys_frame(sexp* n) {
  return r_sys_frame(r_c_int(n), NULL);
}
