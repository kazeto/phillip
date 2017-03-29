#include <string>

#include "./ctypes.h"
#include "./phillip.h"


#define CAST(ptr) reinterpret_cast<phil::phillip_main_t*>(ptr)


void* create_phillip()
{
    return new phil::phillip_main_t();
}


void delete_phillip(void *phillip)
{
    delete CAST(phillip);
}


void set_timeout_lhs(void *phillip, int t)
{
    CAST(phillip)->set_timeout_lhs(t);
}


void set_timeout_ilp(void *phillip, int t)
{
    CAST(phillip)->set_timeout_ilp(t);
}


void set_timeout_sol(void *phillip, int t)
{
    CAST(phillip)->set_timeout_sol(t);
}


void set_verbosity(int v)
{
    phil::phillip_main_t::set_verbose(v);
}


void set_parameter(void *phillip, const char *key, const char *value)
{
    CAST(phillip)->set_param(std::string(key), std::string(value));
}


void set_flag(void *phillip, const char* key)
{
    CAST(phillip)->set_flag(std::string(key));
}