#include "./lib/getopt_win.h"
#include "./binary.h"
#include "./processor.h"


namespace phil
{

namespace bin
{


char ACCEPTABLE_OPTIONS[] = "c:f:hk:l:m:o:p:t:v:GHP:T:";


std::unique_ptr<lhs_enumerator_library_t, util::deleter_t<lhs_enumerator_library_t> >
lhs_enumerator_library_t::ms_instance;


lhs_enumerator_library_t* lhs_enumerator_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new lhs_enumerator_library_t());
    return ms_instance.get();
}


lhs_enumerator_library_t::lhs_enumerator_library_t()
{
    add("depth", new lhs::depth_based_enumerator_t::generator_t());
    add("a*", new lhs::a_star_based_enumerator_t::generator_t());
}


std::unique_ptr<ilp_converter_library_t, util::deleter_t<ilp_converter_library_t> >
ilp_converter_library_t::ms_instance;


ilp_converter_library_t* ilp_converter_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new ilp_converter_library_t());
    return ms_instance.get();
}


ilp_converter_library_t::ilp_converter_library_t()
{
    add("null", new cnv::null_converter_t::generator_t());
    add("weighted", new cnv::weighted_converter_t::generator_t());
    add("costed", new cnv::costed_converter_t::generator_t());
}


std::unique_ptr<ilp_solver_library_t, util::deleter_t<ilp_solver_library_t> >
ilp_solver_library_t::ms_instance;


ilp_solver_library_t* ilp_solver_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new ilp_solver_library_t());
    return ms_instance.get();
}


ilp_solver_library_t::ilp_solver_library_t()
{
    add("null", new sol::null_solver_t::generator_t());
    add("lpsolve", new sol::lp_solve_t::generator_t());
    add("gurobi", new sol::gurobi_t::generator_t());
    add("gurobi_kbest", new sol::gurobi_k_best_t::generator_t());
}


std::unique_ptr<distance_provider_library_t, util::deleter_t<distance_provider_library_t> >
distance_provider_library_t::ms_instance;


distance_provider_library_t* distance_provider_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new distance_provider_library_t());
    return ms_instance.get();
}


distance_provider_library_t::distance_provider_library_t()
{
    add("basic", new kb::dist::basic_distance_provider_t::generator_t());
    add("cost", new kb::dist::cost_based_distance_provider_t::generator_t());
}


std::unique_ptr<category_table_library_t, util::deleter_t<category_table_library_t> >
category_table_library_t::ms_instance;


category_table_library_t* category_table_library_t::instance()
{
    if (not ms_instance)
        ms_instance.reset(new category_table_library_t());
    return ms_instance.get();
}


category_table_library_t::category_table_library_t()
{
    add("null", new kb::ct::null_category_table_t::generator_t());
    add("basic", new kb::ct::basic_category_table_t::generator_t());
}



bool _load_config_file(
    const char *filename, phillip_main_t *phillip,
    execution_configure_t *option, inputs_t *inputs);

/** @return Validity of input. */
bool _interpret_option(
    int opt, const std::string &arg, phillip_main_t *phillip,
    execution_configure_t *option, inputs_t *inputs);


execution_configure_t::execution_configure_t()
    : mode(EXE_MODE_UNDERSPECIFIED), kb_name("kb.cdb")
{}


void prepare(
    int argc, char* argv[], phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs)
{
    util::initialize();

    util::print_console("Phillip starts...");
    util::print_console("  version: " + phillip_main_t::VERSION);

    parse_options(argc, argv, phillip, config, inputs);
    IF_VERBOSE_1("Phillip has completed parsing comand options.");

    if (config->mode != bin::EXE_MODE_HELP)
        preprocess(*config, phillip);
}


void execute(
    phillip_main_t *ph,
    const execution_configure_t &config, const inputs_t &inputs)
{
    if (config.mode == bin::EXE_MODE_HELP)
    {
        bin::print_usage();
        return;
    }

    bool do_compile =
        (config.mode == bin::EXE_MODE_COMPILE_KB) or
        ph->flag("do_compile_kb");

    /* COMPILING KNOWLEDGE-BASE */
    if (do_compile)
    {
        proc::processor_t processor;
        util::print_console("Compiling knowledge-base ...");

        kb::kb()->prepare_compile();

        processor.add_component(new proc::compile_kb_t());
        processor.process(inputs);

        kb::kb()->finalize();

        util::print_console("Completed to compile knowledge-base.");
    }

    auto proc = [&](const lf::input_t &ipt, opt::epoch_t epoch)
    {
        if (config.mode == bin::EXE_MODE_INFERENCE)
        {
            ph->infer(ipt);

            // WRITE RESULTS
            ph->write([&](std::ostream *os)
            {
                auto sols = ph->get_solutions();
                for (auto sol = sols.begin(); sol != sols.end(); ++sol)
                    sol->print_graph(os);
            }, wf::WR_FOUT);
        }
        else
        {
            ph->learn(ipt, epoch);
        }
    };

    /* INFERENCE */
    if (config.mode == bin::EXE_MODE_INFERENCE or
        config.mode == bin::EXE_MODE_LEARNING)
    {
        std::vector<lf::input_t> parsed_inputs;
        proc::processor_t processor;
        bool is_training(config.mode == bin::EXE_MODE_LEARNING);

        util::print_console("Loading observations ...");

        processor.add_component(new proc::parse_obs_t(&parsed_inputs));
        processor.process(inputs);

        util::print_console("Completed to load observations.");
        util::print_console_fmt("    # of observations: %d", parsed_inputs.size());

        kb::kb()->prepare_query();
        ph->check_validity();
        ph->write_header();

        int max_epoch = is_training ? ph->param_int("max-epoch", 100) : 1;

        for (int epoch = 0; epoch < max_epoch; ++epoch)
        {
            if (is_training and is_verbose(VERBOSE_1))
                util::print_console_fmt("    -------- Training epoch #%d --------", epoch + 1);

            // PRINT CURRENT EPOCH TO OUTPUT STREAMS
            ph->write([&](std::ostream *os)
            {
                (*os) << "<inference "
                    << util::format("epoch=\"%d\"", epoch)
                    << ">" << std::endl;
            });

            // SOLVE EACH OBSERVATION
            for (int i = 0; i < parsed_inputs.size(); ++i)
            {
                const lf::input_t &ipt = parsed_inputs.at(i);

                std::string obs_name = ipt.name;
                if (obs_name.rfind("::") != std::string::npos)
                    obs_name = obs_name.substr(obs_name.rfind("::") + 2);

                if (ph->is_target(obs_name) and
                    not ph->is_excluded(obs_name))
                {
                    IF_VERBOSE_1(util::format("Observation #%d: %s", i, ipt.name.c_str()));
                    kb::kb()->clear_distance_cache();

#ifdef _DEBUG
                    /* DO NOT HANDLE EXCEPTIONS TO LET THE DEBUGGER CATCH AN EXCEPTION. */
                    proc(ipt, epoch);
#else
                    try
                    {
                        proc(ipt, epoch);
                    }
                    catch (const std::exception &e)
                    {
                        util::print_warning_fmt(
                            "Some exception was caught and then the observation \"%s\" was skipped.", obs_name.c_str());
                        util::print_warning_fmt("  -> what(): %s", e.what());
                        continue;
                    }
#endif
                }
            }

            ph->write([&](std::ostream *os)
            {
                (*os) << util::format("<inference epoch=\"%d\">", epoch) << std::endl;
            });
        }

        ph->write_footer();
    }
}


bool parse_options(
    int argc, char* argv[], phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs)
{
    int opt;
    
    while ((opt = getopt(argc, argv, ACCEPTABLE_OPTIONS)) != -1)
    {
        std::string arg((optarg == NULL) ? "" : optarg);
        int ret = _interpret_option( opt, arg, phillip, config, inputs );
        
        if (not ret)
            throw phillip_exception_t(
            "Any error occured during parsing command line options:"
            + util::format("-%c %s", opt, arg.c_str()), true);
    }

    for (int i = optind; i < argc; i++)
        inputs->push_back(util::normalize_path(argv[i]));

    return true;
}


/** Load the setting file. */
bool _load_config_file(
    const char* filename, phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs )
{
    char line[2048];
    std::ifstream fin( filename );

    if (not fin)
        throw phillip_exception_t(util::format(
        "Cannot open setting file \"%s\"", filename));

    util::print_console_fmt("Loading setting file \"%s\"", filename);
    
    while( not fin.eof() )
    {
        fin.getline( line, 2048 );
        if( line[0] == '#' ) continue; // COMMENT

        std::string sline(line);
        auto spl = phil::util::split(sline, " \t", 1);
        
        if( spl.empty() ) continue;

        if( spl.at(0).at(0) == '-' )
        {
            int opt = static_cast<int>( spl.at(0).at(1) );
            std::string arg = (spl.size() <= 1) ? "" : util::strip(spl.at(1), "\n");
            int ret = _interpret_option( opt, arg, phillip, config, inputs );
            
            if (not ret)
                throw phillip_exception_t(
                "Any error occured during parsing command line options:"
                + std::string(line), true);
        }
        if (spl.at(0).at(0) != '-' and spl.size() == 1)
            inputs->push_back(util::normalize_path(spl.at(0)));
    }

    fin.close();
    return true;
}


bool _interpret_option(
    int opt, const std::string &arg, phillip_main_t *phillip,
    execution_configure_t *config, inputs_t *inputs)
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
                phillip->set_param("distance_provider", key);
                return true;
            }
            if (type == "tab")
            {
                phillip->set_param("category_table", key);
                return true;
            }
        }
        return false;
    }
    
    case 'f':
        phillip->set_flag(arg);
        return true;

    case 'h':
        config->mode = EXE_MODE_HELP;
        return true;
    
    case 'k': // ---- SET FILENAME OF KNOWLEDGE-BASE
    {
        config->kb_name = util::normalize_path(arg);
        return true;
    }

    case 'l': // ---- SET THE PATH OF PHILLIP CONFIGURE FILE
    {
        std::string path = util::normalize_path(arg);
        _load_config_file(path.c_str(), phillip, config, inputs);
        return true;
    }
    
    case 'm': // ---- SET MODE
    {
        if (config->mode != EXE_MODE_HELP)
        {
            if (arg == "inference" or arg == "infer")
                config->mode = EXE_MODE_INFERENCE;
            else if (arg == "compile_kb" or arg == "compile")
                config->mode = EXE_MODE_COMPILE_KB;
            else if (arg == "learning" or arg == "learn")
                config->mode = EXE_MODE_LEARNING;
            else
                config->mode = EXE_MODE_UNDERSPECIFIED;
        }

        return (config->mode != EXE_MODE_UNDERSPECIFIED);
    }

    case 'o': // ---- SET OUTPUT PATH
    {
        int idx(arg.find('='));

        if (idx != std::string::npos)
        {
            std::string key = arg.substr(0, idx);
            std::string val = arg.substr(idx + 1);

            if (key == "lhs")
            {
                phillip->set_param("path_lhs_out", util::normalize_path(val));
                return true;
            }
            else if (key == "ilp")
            {
                phillip->set_param("path_ilp_out", util::normalize_path(val));
                return true;
            }
            else if (key == "sol")
            {
                phillip->set_param("path_sol_out", util::normalize_path(val));
                return true;
            }
            else
                return false;
        }
        else
        {
            phillip->set_param("path_out", util::normalize_path(arg));
            return true;
        }
    }
                
    case 'p': // ---- SET PARAMETER
    {
        int idx(arg.find('='));
        
        if( idx != std::string::npos )
        {
            std::string key = arg.substr(0, idx);
            std::string val = arg.substr(idx + 1);
            if (util::startswith(key, "path"))
                val = util::normalize_path(val);
            phillip->set_param(key, val);
        }
        else
            phillip->set_param(arg, "");
        
        return true;
    }

    case 't': // ---- SET NAME OF THE OBSERVATION TO SOLVE
    {
        if (arg.empty()) return false;
        
        if (arg.at(0) == '!')
            config->excluded_obs_names.insert(arg.substr(1));
        else
            config->target_obs_names.insert(arg);
        return true;
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

    case 'G':
        phillip->set_flag("get_pseudo_positive");
        return true;

    case 'H':
        phillip->set_flag("human_readable_output");
        return true;

    case 'P': // ---- SET PARALLEL THREAD NUM
    {
        auto spl = util::split(arg, "=");
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

    case 'T': // ---- SET TIMEOUT [SECOND]
    {                  
        float t;
        auto spl = util::split(arg, "=");
        if (spl.size() == 1)
        {
            _sscanf(arg.c_str(), "%f", &t);
            phillip->set_timeout_all(t);
            return true;
        }
        else if (spl.size() == 2)
        {
            if (spl[0] == "lhs" or spl[0] == "ilp" or spl[0] == "sol")
            {
                _sscanf(spl[1].c_str(), "%f", &t);
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


bool preprocess(const execution_configure_t &config, phillip_main_t *ph)
{
    if (config.mode == EXE_MODE_UNDERSPECIFIED)
        throw phillip_exception_t("Execution mode is underspecified.", true);

    for (auto n : config.target_obs_names)
        ph->add_target(n);
    for (auto n : config.excluded_obs_names)
        ph->add_exclusion(n);

    lhs_enumerator_t *lhs =
        lhs_enumerator_library_t::instance()->generate(config.lhs_key, ph);
    ilp_converter_t *ilp =
        ilp_converter_library_t::instance()->generate(config.ilp_key, ph);
    ilp_solver_t *sol =
        ilp_solver_library_t::instance()->generate(config.sol_key, ph);

    kb::knowledge_base_t::initialize(config.kb_name, ph);

    switch (config.mode)
    {
    case EXE_MODE_INFERENCE:
    case EXE_MODE_LEARNING:
        if (lhs != NULL) ph->set_lhs_enumerator(lhs);
        if (ilp != NULL) ph->set_ilp_convertor(ilp);
        if (sol != NULL) ph->set_ilp_solver(sol);
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
        "    -m {compile_kb|compile} : Compiling knowledge-base mode.",
        "    -m {inference|infer} : Inference mode.",
        "    -m {learning|learn} : Learning mode.",
        "",
        "  Common Options:",
        "    -l <NAME> : Loads a config-file.",
        "    -p <NAME>=<VALUE> : Sets a parameter.",
        "    -f <NAME> : Sets a flag.",
        "    -t <INT> : Sets the number of threads for parallelization.",
        "    -v <INT> : Sets verbosity (0 ~ 5).",
        "    -h : Prints simple usage.",
        "",
        "  Options in compile_kb mode:",
        "    -c dist=<NAME> : Sets a component to define relatedness between predicates.",
        "    -c tab=<NAME> : Sets a component for making category-table.",
        "    -k <NAME> : Sets the prefix of the path of the compiled knowledge base.",
        "",
        "  Options in inference-mode or learning-mode:",
        "    -c lhs=<NAME> : Sets a component for making latent hypotheses sets.",
        "    -c ilp=<NAME> : Sets a component for making ILP problems.",
        "    -c sol=<NAME> : Sets a component for making solution hypotheses.",
        "    -k <NAME> : Sets the prefix of the path of the compiled knowledge base.",
        "    -o <PATH> : Prints the XML of the solution hypothesis to the given file path.",
        "    -o lhs=<PATH> : Prints the XML of the latent hypothesis set for debug to the given file path.",
        "    -o ilp=<PATH> : Prints the XML of the ILP problem for debug to the given file path.",
        "    -o sol=<PATH> : Prints the XML of the ILP solution for debug to the given file path.",
        "    -t <NAME> : Solves only the observation of corresponding name.",
        "    -t !<NAME> : Excludes the observation which corresponds with given name.",
        "    -G : Forces to satisfy the requirements.",
        "    -H : Adds the human readable hypothesis to output XMLs.",
        "    -T <INT>  : Sets timeout of the whole inference in seconds.",
        "    -T lhs=<INT> : Sets timeout of the creation of latent hypotheses sets in seconds.",
        "    -T ilp=<INT> : Sets timeout of the conversion into ILP problem in seconds.",
        "    -T sol=<INT> : Sets timeout of the optimization of ILP problem in seconds.",
        "",
        "  Wiki: https://github.com/kazeto/phillip/wiki"};

    for (auto s : USAGE)
        util::print_console(s);
}


}

}
