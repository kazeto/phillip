#include <string>

#include "./ctypes.h"
#include "./phillip.h"


void delete_phillip(void *phillip)
{
    delete reinterpret_cast<phil::phillip_main_t*>(phillip);
}


void set_timeout_lhs(void *phillip, int t)
{
    reinterpret_cast<phil::phillip_main_t*>(phillip)->set_timeout_lhs(t);
}


void set_timeout_ilp(void *phillip, int t)
{
    reinterpret_cast<phil::phillip_main_t*>(phillip)->set_timeout_ilp(t);
}


void set_timeout_sol(void *phillip, int t)
{
    reinterpret_cast<phil::phillip_main_t*>(phillip)->set_timeout_sol(t);
}


void set_verbosity(int v)
{
    phil::phillip_main_t::set_verbose(v);
}


void set_parameter(void *phillip, const char* key, const char* value)
{
    reinterpret_cast<phil::phillip_main_t*>(phillip)->
        set_param(std::string(key), std::string(value));
}
