#include "./lib/getopt_win.h"
#include "./binary.h"


namespace phil
{

namespace bin
{


/** String for usage printing. */
const std::string USAGE =
    "Usage: phil -m [MODE] [OPTIONS]\n"
    "  Mode:\n"
    "    -m inference : Inference mode.\n"
    "    -m compile_kb : Compiling knowledge-base mode.\n"
    "  Common Options:\n"
    "    -l <NAME> : Load a config-file.\n"
    "    -p <NAME>=<VALUE> : set a parameter.\n"
    "        kb_max_distance : limitation of distance between literals.\n"
    "    -f <NAME> : Set flag.\n"
    "        do_compile_kb : In inference-mode, compile knowledge base at first.\n"
    "    -v <INT>  : Set verbosity.\n"
    "  Options in inference-mode:\n"
    "    -c lhs=<NAME> : Set component for making latent hypotheses sets.\n"
    "    -c ilp=<NAME> : Set component for making ILP problems.\n"
    "    -c sol=<NAME> : Set component for making solution hypotheses.\n"
    "    -k <NAME> : Set filename of knowledge-base.\n"
    "    -o <NAME> : Set name of the observation to solve.\n"
    "    -T <INT>  : Set timeout. [second]\n"
    "  Options in compile_kb mode:\n"
    "    -k <NAME> : Set filename of output of compile_kb.\n"
    "    -d <NAME> : Set the distance-provider of knowledge-base.";

char ACCEPTABLE_OPTIONS[] = "c:d:f:k:l:m:o:p:t:v:P:T:";


bool _load_config_file(
    const char *filename,
    execution_configure_t *option, std::vector<std::string> *inputs );

/** @return Validity of input. */
bool _interpret_option(
    int opt, const std::string &arg,
    execution_configure_t *option, std::vector<std::string> *inputs );

kb::distance_provider_type_e _get_distance_provider_type(const std::string &arg);
lhs_enumerator_t* _new_lhs_enumerator( const std::string &key );
ilp_converter_t* _new_ilp_converter( const std::string &key );
ilp_solver_t* _new_ilp_solver( const std::string &key );



execution_configure_t::execution_configure_t()
    : mode(EXE_MODE_UNDERSPECIFIED), kb_name( "kb.cdb" ),
      lhs(NULL), ilp(NULL), sol(NULL), kb(NULL)
{}


execution_configure_t::~execution_configure_t()
{
    if (lhs != NULL) delete lhs;
    if (ilp != NULL) delete ilp;
    if (sol != NULL) delete sol;
    if (kb  != NULL) delete kb;
}


/** Parse command line options.
 * @return False if any error occured. */
bool parse_options(
    int argc, char* argv[],
    execution_configure_t *config,
    std::vector<std::string> *inputs )
{
    int opt;
    
    while ((opt = getopt(argc, argv, ACCEPTABLE_OPTIONS)) != -1)
    {
        std::string arg((optarg == NULL) ? "" : optarg);
        int ret = _interpret_option( opt, arg, config, inputs );
        
        if( not ret )
        {
            print_error(
                "Any error occured during parsing command line options:"
                + format("-%c %s", opt, arg.c_str()) );
        }
    }

    for (int i = optind; i < argc; i++)
        inputs->push_back(normalize_path(argv[i]));

    if( config->lhs != NULL ) delete config->lhs;
    if( config->ilp != NULL ) delete config->ilp;
    if( config->sol != NULL ) delete config->sol;

    config->lhs = _new_lhs_enumerator(config->lhs_key);
    config->ilp = _new_ilp_converter(config->ilp_key);
    config->sol = _new_ilp_solver(config->sol_key);

    return true;
}


/** Load the setting file. */
bool _load_config_file(
    const char* filename,
    execution_configure_t *config, std::vector<std::string> *inputs )
{
    char line[2048];
    std::ifstream fin( filename );

    if (not fin)
    {
        print_error_fmt("Cannot open setting file \"%s\"", filename);
        return false;
    }

    print_console_fmt("Loading setting file \"%s\"", filename);
    
    while( not fin.eof() )
    {
        fin.getline( line, 2048 );
        if( line[0] == '#' ) continue; // COMMENT

        std::string sline(line);
        auto spl = phil::split(sline, " \t", 1);
        
        if( spl.empty() ) continue;

        if( spl.at(0).at(0) == '-' )
        {
            int opt = static_cast<int>( spl.at(0).at(1) );
            std::string arg = (spl.size() <= 1) ? "" : strip(spl.at(1), "\n");
            int ret = _interpret_option( opt, arg, config, inputs );
            
            if (not ret)
            {
                print_error(
                    "Any error occured during parsing command line options:"
                    + std::string(line));
            }
        }
        if (spl.at(0).at(0) != '-' and spl.size() == 1)
            inputs->push_back(normalize_path(spl.at(0)));
    }

    fin.close();
    return true;
}


bool _interpret_option(
    int opt, const std::string &arg,
    execution_configure_t *config, std::vector<std::string> *inputs)
{
    switch(opt)
    {
        
    case 'c': // ---- SET COMPONENT
    {
        int idx( arg.find('=') );
        if (idx >= 0)
        {
            std::string type = arg.substr(0, idx);
            std::string key = arg.substr(idx + 1);
            if (type == "lhs")
            {
                config->lhs_key = key;
                return true;
            }
            if (type == "ilp")
            {
                config->ilp_key = key;
                return true;
            }
            if (type == "sol")
            {
                config->sol_key = key;
                return true;
            }
        }
        return false;
    }

    case 'd': // ---- SET DISTANCE-PROVIDER
    {
        kb::distance_provider_type_e type =
            _get_distance_provider_type(arg);
        if (type != kb::DISTANCE_PROVIDER_UNDERSPECIFIED)
        {
            config->distance_type = type;
            return true;
        }
        else return false;
    }

    case 'f':
        phil::sys()->set_flag(arg);
        return true;
    
    case 'k': // ---- SET FILENAME OF KNOWLEDGE-BASE
    {
        config->kb_name = normalize_path(arg);
        return true;
    }

    case 'l': // ---- SET THE PATH OF PHILLIP CONFIGURE FILE
    {
        std::string path = normalize_path(arg);
        _load_config_file(path.c_str(), config, inputs);
        return true;
    }
    
    case 'm': // ---- SET MODE
    {
        if      (arg == "inference")  config->mode = EXE_MODE_INFERENCE;
        else if (arg == "compile_kb") config->mode = EXE_MODE_COMPILE_KB;
        else                          config->mode = EXE_MODE_UNDERSPECIFIED;

        return (config->mode != EXE_MODE_UNDERSPECIFIED);
    }
        
    case 'o': // ---- SET NAME OF THE OBSERVATION TO SOLVE
    {
        config->target_obs_name = arg;
        return true;
    }
        
    case 'p': // ---- SET PARAMETER
    {
        int idx(arg.find('='));
        
        if( idx != std::string::npos )
        {
            std::string key = arg.substr(0, idx);
            std::string val = arg.substr(idx + 1);
            if (startswith(key, "path"))
                val = normalize_path(val);
            phil::sys()->set_param(key, val);
        }
        else
            phil::sys()->set_param(arg, "");
        
        return true;
    }
    
    case 'v': // ---- SET VERBOSITY
    {
        int v(-1);
        int ret = _sscanf( arg.c_str(), "%d", &v );

        if( v >= 0 and v <= FULL_VERBOSE )
        {
            phil::sys()->set_verbose(v);
            return true;
        }
        else
            return false;
    }
        
    case 'T': // ---- SET TIMEOUT [SECOND]
    {
        int t;
        _sscanf( arg.c_str(), "%d", &t );
        phil::sys()->set_timeout(t);
        return true;
    }
    
    case ':':
    case '?':
        return false;
    }
    
    return false;
}


kb::distance_provider_type_e _get_distance_provider_type(const std::string &arg)
{
    if (arg == "basic")
        return kb::DISTANCE_PROVIDER_BASIC;
    if (arg == "cost_based")
        return kb::DISTANCE_PROVIDER_COST_BASED;

    return kb::DISTANCE_PROVIDER_UNDERSPECIFIED;
}


lhs_enumerator_t* _new_lhs_enumerator( const std::string &key )
{
    if (key == "bidirection")
    {
        return new lhs::basic_lhs_enumerator_t(
            true, true,
            sys()->param_float("max_depth"),
            sys()->param_float("max_redundancy"));
    }
    if (key == "abduction")
    {
        return new lhs::basic_lhs_enumerator_t(
            false, true,
            sys()->param_float("max_depth"),
            sys()->param_float("max_redundancy"));
    }
    if (key == "deduction")
    {
        return new lhs::basic_lhs_enumerator_t(
            true, false,
            sys()->param_float("max_depth"),
            sys()->param_float("max_redundancy"));
    }
    return NULL;
}


ilp_converter_t* _new_ilp_converter( const std::string &key )
{
    if (key == "null") return new ilp::null_converter_t();
    return NULL;
}


ilp_solver_t* _new_ilp_solver( const std::string &key )
{
    if (key == "null")   return new sol::null_solver_t();
    if (key == "gltk")   return new sol::gnu_linear_programming_kit_t();
    if (key == "lpsol")  return new sol::lp_solve_t();
    if (key == "gurobi") return new sol::gurobi_t();
    return NULL;
}


bool preprocess( execution_configure_t *config )
{
    if (config->mode == EXE_MODE_UNDERSPECIFIED)
    {
        print_error("Execution mode is underspecified!!");
        return false;
    }
    
    float max_dist = -1;
    _sscanf(sys()->param("kb_max_distance").c_str(), "%f", &max_dist);

    config->kb = new kb::knowledge_base_t(
        config->kb_name, config->distance_type, max_dist);

    switch (config->mode)
    {
    case EXE_MODE_INFERENCE:
        if (config->lhs) phil::sys()->set_lhs_enumerator(config->lhs);
        if (config->ilp) phil::sys()->set_ilp_convertor(config->ilp);
        if (config->sol) phil::sys()->set_ilp_solver(config->sol);
        if (config->kb ) phil::sys()->set_knowledge_base(config->kb);
        return true;
    case EXE_MODE_COMPILE_KB:
        return true;
    default:
        return false;
    }
}


}

}
