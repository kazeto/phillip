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
    "    -k <NAME> : Set filename of output of compile_kb.\n";

char ACCEPTABLE_OPTIONS[] = "c:f:k:l:m:o:p:t:v:P:T:";


bool _load_config_file(
    const char *filename,
    execution_configure_t *option, std::vector<std::string> *inputs);

/** @return Validity of input. */
bool _interpret_option(
    int opt, const std::string &arg,
    execution_configure_t *option, std::vector<std::string> *inputs);

kb::distance_provider_type_e _get_distance_provider_type(const std::string &arg);
lhs_enumerator_t* _new_lhs_enumerator(const std::string &key);
ilp_converter_t* _new_ilp_converter(const std::string &key);
ilp_solver_t* _new_ilp_solver(const std::string &key);



execution_configure_t::execution_configure_t()
    : mode(EXE_MODE_UNDERSPECIFIED), kb_name("kb.cdb")
{}


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
        
        if (not ret)
        {
            print_error(
                "Any error occured during parsing command line options:"
                + format("-%c %s", opt, arg.c_str()) );
        }
    }

    for (int i = optind; i < argc; i++)
        inputs->push_back(normalize_path(argv[i]));

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
            if (type == "dist")
            {
                config->dist_key = key;
                return true;
            }
        }
        return false;
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
        _sscanf(arg.c_str(), "%d", &t);
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
    if (arg == "cost")
        return kb::DISTANCE_PROVIDER_COST_BASED;

    return kb::DISTANCE_PROVIDER_UNDERSPECIFIED;
}


lhs_enumerator_t* _new_lhs_enumerator(const std::string &key)
{
    if (key == "a*:bidirection")
        return new lhs::a_star_based_enumerator_t(true, true);
    if (key == "a*:abduction")
        return new lhs::a_star_based_enumerator_t(false, true);
    if (key == "a*:deduction")
        return new lhs::a_star_based_enumerator_t(true, false);
    if (key == "depth:bidirection" or key == "bidirection")
        return new lhs::depth_based_enumerator_t(
        true, true,
        sys()->param_int("max_depth"),
        sys()->param_float("max_distance"),
        sys()->param_float("max_redundancy"));
    if (key == "depth:abduction"  or key == "abduction")
        return new lhs::depth_based_enumerator_t(
        false, true,
        sys()->param_int("max_depth"),
        sys()->param_float("max_distance"),
        sys()->param_float("max_redundancy"));
    if (key == "depth:deduction" or key == "deduction")
        return new lhs::depth_based_enumerator_t(
        true, false,
        sys()->param_int("max_depth"),
        sys()->param_float("max_distance"),
        sys()->param_float("max_redundancy"));
    return NULL;
}


ilp_converter_t* _new_ilp_converter( const std::string &key )
{
    if (key == "null") return new ilp::null_converter_t();
    if (key == "weighted")
    {
        double obs = sys()->param_float("default_obs_cost", 10.0);
        const std::string &param = sys()->param("weight_provider");
        ilp::weighted_converter_t::weight_provider_t *ptr =
            ilp::weighted_converter_t::parse_string_to_weight_provider(param);

        return new ilp::weighted_converter_t(obs, ptr);
    }
    if (key == "costed")
    {
        const std::string &param = sys()->param("cost_provider");
        ilp::costed_converter_t::cost_provider_t *ptr =
            ilp::costed_converter_t::parse_string_to_cost_provider(param);
        return new ilp::costed_converter_t(ptr);
    }
    return NULL;
}


ilp_solver_t* _new_ilp_solver( const std::string &key )
{
    if (key == "null")    return new sol::null_solver_t();
    if (key == "gltk")    return new sol::gnu_linear_programming_kit_t();
    if (key == "lpsolve") return new sol::lp_solve_t();
    if (key == "gurobi")  return new sol::gurobi_t();
    return NULL;
}


bool preprocess(const execution_configure_t &config)
{
    if (config.mode == EXE_MODE_UNDERSPECIFIED)
    {
        print_error("Execution mode is underspecified!!");
        return false;
    }

    kb::distance_provider_type_e dist_type(kb::DISTANCE_PROVIDER_BASIC);
    if (not config.dist_key.empty())
    {
        kb::distance_provider_type_e _type =
            _get_distance_provider_type(config.dist_key);
        if (_type != kb::DISTANCE_PROVIDER_UNDERSPECIFIED)
            dist_type = _type;
        else
        {
            print_warning_fmt(
                "The key of distance-provider is invalid: %s",
                config.dist_key.c_str());
        }
    }

    float max_dist = sys()->param_float("kb_max_distance", -1.0);

    lhs_enumerator_t *lhs = _new_lhs_enumerator(config.lhs_key);
    ilp_converter_t *ilp = _new_ilp_converter(config.ilp_key);
    ilp_solver_t *sol = _new_ilp_solver(config.sol_key);

    kb::knowledge_base_t *kb = new kb::knowledge_base_t(
        config.kb_name, dist_type, max_dist);

    switch (config.mode)
    {
    case EXE_MODE_INFERENCE:
        if (lhs != NULL) phil::sys()->set_lhs_enumerator(lhs);
        if (ilp != NULL) phil::sys()->set_ilp_convertor(ilp);
        if (sol != NULL) phil::sys()->set_ilp_solver(sol);
        if (kb != NULL)  phil::sys()->set_knowledge_base(kb);
        return true;
    case EXE_MODE_COMPILE_KB:
        if (kb != NULL)  phil::sys()->set_knowledge_base(kb);
        return true;
    default:
        return false;
    }
}


}

}
