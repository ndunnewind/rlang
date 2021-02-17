#include <rlang.h>
#include "decl/dyn-list-of-decl.h"

#define R_DYN_LOF_GROWTH_FACTOR 2

#define R_DYN_LOF_INIT_SIZE 32

enum shelter_dyn_list_of {
  SHELTER_DYN_LOF_raw,
  SHELTER_DYN_LOF_data,
  SHELTER_DYN_LOF_data_arr_locs,
  SHELTER_DYN_LOF_extra_array,
  SHELTER_DYN_LOF_extra_shelter_array,
  SHELTER_DYN_LOF_moved_arr,
  SHELTER_DYN_LOF_moved_shelter_arr,
  SHELTER_DYN_LOF_arrays,
  SHELTER_DYN_LOF_SIZE
};

struct r_dyn_list_of* r_new_dyn_list_of(enum r_type type,
                                        r_ssize capacity,
                                        r_ssize width) {
  switch (type) {
  case r_type_character:
  case r_type_list:
    r_abort("Can't create a dynamic list of barrier vectors.");
  default:
    break;
  }

  sexp* shelter = KEEP(r_new_list(SHELTER_DYN_LOF_SIZE));

  sexp* lof_raw = r_new_raw(sizeof(struct r_dyn_list_of));
  r_list_poke(shelter, SHELTER_DYN_LOF_raw, lof_raw);

  struct r_dyn_array* p_moved_arr = r_new_dyn_array(sizeof(struct r_dyn_array*), R_DYN_LOF_INIT_SIZE);
  r_list_poke(shelter, SHELTER_DYN_LOF_moved_arr, p_moved_arr->shelter);

  struct r_dyn_array* p_moved_shelter_arr = r_new_dyn_vector(r_type_list, R_DYN_LOF_INIT_SIZE);
  r_list_poke(shelter, SHELTER_DYN_LOF_moved_shelter_arr, p_moved_shelter_arr->shelter);

  sexp* data = r_new_vector(type, r_ssize_mult(capacity, width));
  r_list_poke(shelter, SHELTER_DYN_LOF_data, data);
  void* v_data = r_vec_deref(data);

  sexp* data_arr_locs = r_new_raw(sizeof(r_ssize) * capacity);
  r_list_poke(shelter, SHELTER_DYN_LOF_data_arr_locs, data_arr_locs);
  r_ssize* v_data_arr_locs = r_raw_deref(data_arr_locs);
  R_MEM_SET(r_ssize, v_data_arr_locs, -1, capacity);

  struct r_dyn_array* p_arrays = r_new_dyn_array(sizeof(struct r_pair_ptr_ssize), capacity);
  r_list_poke(shelter, SHELTER_DYN_LOF_arrays, p_arrays->shelter);

  struct r_dyn_list_of* p_lof = r_raw_deref(lof_raw);
  *p_lof = (struct r_dyn_list_of) {
    .shelter = shelter,
    .count = 0,
    .capacity = capacity,
    .growth_factor = R_DYN_LOF_GROWTH_FACTOR,

    .p_arrays = p_arrays,

    // private:
    .width = width,

    .data = data,
    .v_data = v_data,
    .data_arr_locs = data_arr_locs,
    .v_data_arr_locs = v_data_arr_locs,

    .p_moved_arr = p_moved_arr,
    .p_moved_shelter_arr = p_moved_shelter_arr,

    .type = type,
    .elt_byte_size = r_vec_elt_sizeof0(type),
  };

  FREE(1);
  return p_lof;
}

sexp* r_lof_unwrap(struct r_dyn_list_of* p_lof) {
  sexp* out = KEEP(r_new_list(p_lof->count));

  enum r_type type = p_lof->type;
  r_ssize n = p_lof->count;
  struct r_pair_ptr_ssize* v_arrays = r_arr_ptr_front(p_lof->p_arrays);

  for (r_ssize i = 0; i < n; ++i) {
    struct r_pair_ptr_ssize array = v_arrays[i];
    r_list_poke(out, i, r_vec_n(type, array.ptr, array.size));
  }

  FREE(1);
  return out;
}

static
void r_lof_resize(struct r_dyn_list_of* p_lof, r_ssize capacity) {
  r_ssize count = p_lof->count;

  // Resize reserve
  sexp* data = r_vec_resize0(p_lof->type,
                             p_lof->data,
                             r_ssize_mult(capacity, p_lof->width));
  r_list_poke(p_lof->shelter, SHELTER_DYN_LOF_data, data);

  p_lof->data = data;
  p_lof->v_data = r_vec_deref0(p_lof->type, data);
  p_lof->capacity = capacity;

  // Resize array indirections
  sexp* data_arr_locs = r_raw_resize(p_lof->data_arr_locs,
                                     r_ssize_mult(sizeof(r_ssize), capacity));
  r_list_poke(p_lof->shelter, SHELTER_DYN_LOF_data_arr_locs, data_arr_locs);

  r_ssize* v_data_arr_locs = r_raw_deref(data_arr_locs);
  r_ssize n_new = capacity - count;
  R_MEM_SET(r_ssize, v_data_arr_locs + count, -1, n_new);

  p_lof->data_arr_locs = data_arr_locs;
  p_lof->v_data_arr_locs = v_data_arr_locs;

  // Resize addresses and update them to point to the new memory
  r_arr_resize(p_lof->p_arrays, capacity);

  struct r_pair_ptr_ssize* v_arrays = r_arr_ptr_front(p_lof->p_arrays);
  unsigned char* v_data_u = (unsigned char*) p_lof->v_data;
  r_ssize bytes = p_lof->width * p_lof->elt_byte_size;

  for (r_ssize i = 0; i < count; ++i) {
    // Preserve addresses of moved arrays
    if (v_data_arr_locs[i] < 0) {
      r_ssize offset = i * bytes;
      v_arrays[i].ptr = v_data_u + offset;
    }
  }
}

void r_lof_push_back(struct r_dyn_list_of* p_lof) {
  r_ssize count = p_lof->count + 1;

  if (count > p_lof->capacity) {
    r_ssize new_size = r_ssize_mult(p_lof->capacity, R_DYN_LOF_GROWTH_FACTOR);
    r_lof_resize(p_lof, new_size);
  }
  p_lof->count = count;

  unsigned char* v_data_u = (unsigned char*) p_lof->v_data;
  r_ssize offset = (count - 1) * p_lof->width * p_lof->elt_byte_size;

  struct r_pair_ptr_ssize info = {
    .ptr = v_data_u + offset,
    .size = 0
  };
  r_arr_push_back(p_lof->p_arrays, &info);
}

void r_lof_arr_push_back(struct r_dyn_list_of* p_lof,
                         r_ssize i,
                         void* p_elt) {
  if (i >= p_lof->count) {
    r_stop_internal("r_lof_arr_push_back",
                    "Location %d does not exist.",
                    i);
  }

  if (reserve_push_back(p_lof, i, p_elt)) {
    return;
  }

  struct r_dyn_array* p_arr = p_lof->p_moved_arr;
  r_ssize arr_i = p_lof->v_data_arr_locs[i];

  if (arr_i >= p_arr->count) {
    r_stop_internal("r_lof_arr_push_back",
                    "Location %d does not exist in the extra array",
                    arr_i);
  }

  struct r_dyn_array* p_inner_arr = R_ARR_GET(struct r_dyn_array*, p_arr, arr_i);
  r_arr_push_back(p_inner_arr, p_elt);

  // Also update pointer in case of resize
  R_ARR_POKE(struct r_pair_ptr_ssize, p_lof->p_arrays, i, ((struct r_pair_ptr_ssize) {
    .ptr = r_arr_ptr_front(p_inner_arr),
    .size = p_inner_arr->count
  }));
}

static
bool reserve_push_back(struct r_dyn_list_of* p_lof, r_ssize i, void* p_elt) {
  if (p_lof->v_data_arr_locs[i] >= 0) {
    return false;
  }

  struct r_pair_ptr_ssize* p_arr_info = r_arr_ptr(p_lof->p_arrays, i);
  if (p_arr_info->size >= p_lof->width) {
    // Inner array is getting too big for the reserve. Move it to a
    // dynamic array.
    reserve_move(p_lof, i, p_elt);
    return false;
  }

  r_ssize count = ++p_arr_info->size;

  r_ssize offset = (i * p_lof->width + count - 1) * p_lof->elt_byte_size;
  void* p = ((unsigned char*) p_lof->v_data) + offset;

  if (p_elt) {
    memcpy(p, p_elt, p_lof->elt_byte_size);
  } else {
    memset(p, 0, p_lof->elt_byte_size);
  }

  return true;
}

static
void reserve_move(struct r_dyn_list_of* p_lof, r_ssize i, void* p_elt) {
  struct r_dyn_array* p_moved_arr = p_lof->p_moved_arr;
  r_ssize n = p_lof->width;

  struct r_dyn_array* p_new = r_new_dyn_vector(p_lof->type, p_lof->width);
  r_list_push_back(p_lof->p_moved_shelter_arr, p_new->shelter);
  r_arr_push_back(p_moved_arr, &p_new);

  void* v_new = r_arr_ptr_front(p_new);
  void* v_old = R_ARR_GET(struct r_pair_ptr_ssize, p_lof->p_arrays, i).ptr;
  memcpy(v_new, v_old, r_ssize_mult(n, p_lof->elt_byte_size));

  p_new->count = n;

  R_ARR_POKE(struct r_pair_ptr_ssize, p_lof->p_arrays, i, ((struct r_pair_ptr_ssize) {
    .ptr = v_new,
    .size = n
  }));

  p_lof->v_data_arr_locs[i] = p_moved_arr->count - 1;
}
