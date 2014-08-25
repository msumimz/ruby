#pragma once

extern "C" {

// vm_core.h
typedef struct rb_vm_struct rb_vm_t;

typedef struct rb_thread_struct rb_thread_t;
typedef struct rb_control_frame_struct rb_control_frame_t;
typedef struct rb_block_struct rb_block_t;

typedef struct rb_iseq_struct rb_iseq_t;
typedef struct rb_call_info_struct rb_call_info_t;

// method.h
typedef struct rb_method_definition_struct rb_method_definition_t;
typedef struct rb_method_entry_struct rb_method_entry_t;

typedef unsigned long rb_num_t;
typedef unsigned long long rb_serial_t;

// st.h
typedef struct st_table st_table;

}
