#ifndef _INCLUDE_HENRY_CTYPES
#define _INCLUDE_HENRY_CTYPES

#ifndef _cplusplus
extern "C" {
#endif

    void delete_phillip(void *phillip);
    void set_timeout_lhs(void *phillip, int t);
    void set_timeout_ilp(void *phillip, int t);
    void set_timeout_sol(void *phillip, int t);
    void set_verbosity(int v);
    void set_parameter(void *phillip, const char* key, const char* value);
    
#ifndef _cplusplus
}
#endif

#endif
