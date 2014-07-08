#ifndef _INCLUDE_HENRY_CTYPES
#define _INCLUDE_HENRY_CTYPES

#ifndef _cplusplus
extern "C" {
#endif

    void delete_henry();
    void set_timeout_lhs(int t);
    void set_timeout_ilp(int t);
    void set_timeout_sol(int t);
    void set_verbosity(int v);
    void set_parameter( const char* key, const char* value );
    
#ifndef _cplusplus
}
#endif

#endif
