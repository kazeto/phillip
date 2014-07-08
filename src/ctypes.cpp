#include <string>

#include "./ctypes.h"
#include "./phillip.h"


void set_timeout_lhs(int t)
{ phil::phillip_main_t::get_instance()->set_timeout_lhs(t); }


void set_timeout_ilp(int t)
{ phil::phillip_main_t::get_instance()->set_timeout_ilp(t); }


void set_timeout_sol(int t)
{ phil::phillip_main_t::get_instance()->set_timeout_sol(t); }


void set_verbosity( int v )
{ phil::phillip_main_t::get_instance()->set_verbose(v); }


void set_parameter( const char* key, const char* value )
{
    phil::phillip_main_t::get_instance()->
        set_param( std::string(key), std::string(value) );
}
