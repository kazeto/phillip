#include "./lib/getopt_win.h"
#include "./binary.h"


namespace phil
{

namespace bin
{


char ACCEPTABLE_OPTIONS[] = "hc:e:f:k:l:m:o:p:t:v:T:";


bool _load_config_file(
    const char *filename, phillip_main_t *phillip,
    execution_configure_t *option, std::vector<std::string> *inputs);

/** @return Validity of input. */
bool _interpret_option(
    int opt, const std::string &arg, phillip_main_t *phillip,
    execution_configure_t *option, std::vector<std::string> *inputs);

kb::distance_provider_type_e _get_distance_provider_type(const std::string &arg);
lhs_enumerator_t* _new_lhs_enumerator(phillip_main_t *phillip, const std::string &key);
ilp_converter_t* _new_ilp_converter(phillip_main_t *phillip, const std::string &key);
ilp_solver_t* _new_ilp_solver(phillip_main_t *phillip, const std::string &key);



execution_configure_t::execution_configure_t()
    : mode(EXE_MODE_UNDERSPECIFIED), kb_name("kb.cdb")
{}


/** Parse command line options.
 * @return False if any error occured. */
bool parse_options(
    int argc, char* argv[], phillip_main_t *phillip,
    execution_configure_t *config, std::vector<std::string> *inputs)
{
    int opt;
    
    while ((opt = getopt(argc, argv, ACCEPTABLE_OPTIONS)) != -1)
    {
        std::string arg((optarg == NULL) ? "" : optarg);
        int ret = _interpret_option( opt, arg, phillip, config, inputs );
        
        if (not ret)
        {
            print_error(
                "Any error occured during parsing command line options:"
                + format("-%c %s", opt, arg.c_str()));
            print_usage();
        }
    }

    for (int i = optind; i < argc; i++)
        inputs->push_back(normalize_path(argv[i]));

    return true;
}


/** Load the setting file. */
bool _load_config_file(
    const char* filename, phillip_main_t *phillip,
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
            int ret = _interpret_option( opt, arg, phillip, config, inputs );
            
            if (not ret)
            {
                print_error(
                    "Any error occured during parsing command line options:"
                    + std::string(line));
                print_usage();
            }
        }
        if (spl.at(0).at(0) != '-' and spl.size() == 1)
            inputs->push_back(normalize_path(spl.at(0)));
    }

    fin.close();
    return true;
}


bool _interpret_option(
    int opt, const std::string &arg, phillip_main_t *phillip,
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

    case 'e': // ---- SET NAME OF THE OBSERVATION TO EXCLUDE
    {
        config->excluded_obs_names.insert(arg);
        return true;
    }
    
    case 'f':
        phillip->set_flag(arg);
        return true;

    case 'h':
        config->mode = EXE_MODE_HELP;
        return true;
    
    case 'k': // ---- SET FILENAME OF KNOWLEDGE-BASE
    {
        config->kb_name = normalize_path(arg);
        return true;
    }

    case 'l': // ---- SET THE PATH OF PHILLIP CONFIGURE FILE
    {
        std::string path = normalize_path(arg);
        _load_config_file(path.c_str(), phillip, config, inputs);
        return true;
    }
    
    case 'm': // ---- SET MODE
    {
        if (config->mode != EXE_MODE_HELP)
        {
            if (arg == "inference")
                config->mode = EXE_MODE_INFERENCE;
            else if (arg == "compile_kb")
                config->mode = EXE_MODE_COMPILE_KB;
            else
                config->mode = EXE_MODE_UNDERSPECIFIED;
        }

        return (config->mode != EXE_MODE_UNDERSPECIFIED);
    }
        
    case 'o': // ---- SET NAME OF THE OBSERVATION TO SOLVE
    {
        config->target_obs_names.insert(arg);
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
            phillip->set_param(key, val);
        }
        else
            phillip->set_param(arg, "");
        
        return true;
    }

    case 't': // ---- SET THREAD NUM
    {
        auto spl = split(arg, "=");
        if (spl.size() == 1)
        {
            phillip->set_param("kb_thread_num", spl[0]);
            phillip->set_param("gurobi_thread_num", spl[0]);
            return true;
        }
        else if (spl.size() == 2)
        {
            if (spl[0] == "kb")
            {
                phillip->set_param("kb_thread_num", spl[1]);
                return true;
            }
            else if (spl[0] == "grb")
            {
                phillip->set_param("gurobi_thread_num", spl[1]);
                return true;
            }
            else
                return false;
        }
        else
            return false;
    }
    
    case 'v': // ---- SET VERBOSITY
    {
        int v(-1);
        int ret = _sscanf( arg.c_str(), "%d", &v );

        if( v >= 0 and v <= FULL_VERBOSE )
        {
            phillip->set_verbose(v);
            return true;
        }
        else
            return false;
    }
        
    case 'T': // ---- SET TIMEOUT [SECOND]
    {                  
        int t;
        auto spl = split(arg, "=");
        if (spl.size() == 1)
        {
            _sscanf(arg.c_str(), "%d", &t);
            phillip->set_timeout_all(t);
            return true;
        }
        else if (spl.size() == 2)
        {
            if (spl[0] == "lhs" or spl[0] == "ilp" or spl[0] == "sol")
            {
                _sscanf(spl[1].c_str(), "%d", &t);
                if (spl[0] == "lhs") phillip->set_timeout_lhs(t);
                if (spl[0] == "ilp") phillip->set_timeout_ilp(t);
                if (spl[0] == "sol") phillip->set_timeout_sol(t);
                return true;
            }
            else
                return false;
        }
        else
            return false;
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


lhs_enumerator_t* _new_lhs_enumerator(phillip_main_t *phillip, const std::string &key)
{
    if (key == "a*")
        return new lhs::a_star_based_enumerator_t(
            phillip,
            phillip->param_float("max_distance"),
            phillip->param_int("max_depth"));
    
    if (key == "depth")
        return new lhs::depth_based_enumerator_t(
        phillip,
        phillip->param_int("max_depth"),
        phillip->param_float("max_distance"),
        phillip->param_float("max_redundancy"),
        phillip->flag("disable_reachable_matrix"));

    return NULL;
}


ilp_converter_t* _new_ilp_converter(phillip_main_t *phillip, const std::string &key)
{
    if (key == "null")
        return new ilp::null_converter_t(phillip);
    if (key == "weighted")
    {
        double obs = phillip->param_float("default_obs_cost", 10.0);
        const std::string &param = phillip->param("weight_provider");
        ilp::weighted_converter_t::weight_provider_t *ptr =
            ilp::weighted_converter_t::parse_string_to_weight_provider(param);

        return new ilp::weighted_converter_t(phillip, obs, ptr);
    }
    if (key == "costed")
    {
        const std::string &param = phillip->param("cost_provider");
        ilp::costed_converter_t::cost_provider_t *ptr =
            ilp::costed_converter_t::parse_string_to_cost_provider(param);
        return new ilp::costed_converter_t(phillip, ptr);
    }
    return NULL;
}


ilp_solver_t* _new_ilp_solver(phillip_main_t *phillip, const std::string &key)
{
    if (key == "null")
        return new sol::null_solver_t(phillip);
    if (key == "lpsolve")
        return new sol::lp_solve_t(phillip);
    if (key == "gurobi")
        return new sol::gurobi_t(
        phillip,
        phillip->param_int("gurobi_thread_num"),
        phillip->flag("activate_gurobi_log"));
    return NULL;
}


bool preprocess(const execution_configure_t &config, phillip_main_t *phillip)
{
    if (config.mode == EXE_MODE_UNDERSPECIFIED)
    {
        print_error("Execution mode is underspecified!!");
        return false;
    }

    kb::distance_provider_type_e dist_type(kb::DISTANCE_PROVIDER_BASIC);
    float max_dist = phillip->param_float("kb_max_distance", -1.0);
    int thread_num = phillip->param_int("kb_thread_num", 1);
    bool disable_stop_word = phillip->flag("disable_stop_word");

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

    for (auto n : config.target_obs_names)
        phillip->add_target(n);
    for (auto n : config.excluded_obs_names)
        phillip->add_exclusion(n);

    lhs_enumerator_t *lhs = _new_lhs_enumerator(phillip, config.lhs_key);
    ilp_converter_t *ilp = _new_ilp_converter(phillip, config.ilp_key);
    ilp_solver_t *sol = _new_ilp_solver(phillip, config.sol_key);

    kb::knowledge_base_t::setup(
        config.kb_name, dist_type, max_dist, thread_num, disable_stop_word);

    switch (config.mode)
    {
    case EXE_MODE_INFERENCE:
        if (lhs != NULL) phillip->set_lhs_enumerator(lhs);
        if (ilp != NULL) phillip->set_ilp_convertor(ilp);
        if (sol != NULL) phillip->set_ilp_solver(sol);
        return true;
    case EXE_MODE_COMPILE_KB:
        return true;
    default:
        return false;
    }
}


void print_usage()
{
    /** String for usage printing. */
    const std::vector<std::string> USAGE = {
        "Usage:",
        "  $phil -m [MODE] [OPTIONS] [INPUTS]",
        "",
        "  Mode:",
        "    -m inference : Inference mode.",
        "    -m compile_kb : Compiling knowledge-base mode.",
        "",
        "  Common Options:",
        "    -l <NAME> : Load a config-file.",
        "    -p <NAME>=<VALUE> : set a parameter.",
        "    -f <NAME> : Set a flag.",
        "    -t <INT> : Set the number of threads for parallelization.",
        "    -v <INT> : Set verbosity (0 ~ 5).",
        "    -h : Print this usage.",
        "",
        "  Options in inference-mode:",
        "    -c lhs=<NAME> : Set component for making latent hypotheses sets.",
        "    -c ilp=<NAME> : Set component for making ILP problems.",
        "    -c sol=<NAME> : Set component for making solution hypotheses.",
        "    -k <NAME> : Set the prefix of the path of the compiled knowledge base.",
        "    -o <NAME> : Solve only the observation of corresponding name.",
        "    -e <NAME> : Exclude the observation of corresponding name from inference.",
        "    -T <INT>  : Set timeout of the whole inference in seconds.",
        "    -T lhs=<INT> : Set timeout of the creation of latent hypotheses sets in seconds.",
        "    -T ilp=<INT> : Set timeout of the conversion into ILP problem in seconds.",
        "    -T sol=<INT> : Set timeout of the optimization of ILP problem in seconds.",
        "",
        "  Options in compile_kb mode:",
        "    -c dist=<NAME> : Set component to define relatedness between predicates.",
        "    -k <NAME> : Set the prefix of the path of the compiled knowledge base.",
        "",
        "  Wiki: https://github.com/kazeto/phillip/wiki"};

    for (auto s : USAGE)
        print_console(s);
}


}

}
