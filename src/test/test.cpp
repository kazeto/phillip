#include "./test.h"
#include "../processor.h"

namespace phil
{

namespace test
{

bool virtual_test_t::operator()(phillip_main_t *main) const
{
    print_console("---- " + disp() + " ----");

    try
    {
        test(main);
        return true;
    }
    catch (char *str)
    {
        print_error_fmt("Faild: %s", str);
        return false;
    }
}


void compiling_axioms_t::test(phillip_main_t *main) const
{
    kb::knowledge_base_t::setup("test.kb.", kb::DISTANCE_PROVIDER_BASIC, 6, 1, false);

    proc::processor_t processor;
    print_console("Compiling knowledge-base ...");

    kb::knowledge_base_t::instance()->prepare_compile();

    processor.add_component(new proc::compile_kb_t());
    processor.process(std::vector<std::string>(1, "data/test.kb.lisp"));

    kb::knowledge_base_t::instance()->finalize();

    print_console("Completed to compile knowledge-base.");
}


std::string compiling_axioms_t::disp() const
{
    return "Compiling Axioms";
}


}

}
