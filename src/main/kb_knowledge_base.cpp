/* -*- coding: utf-8 -*- */

#include <cassert>
#include <cstring>
#include <climits>
#include <algorithm>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "./kb.h"


namespace dav
{

namespace kb
{


const int BUFFER_SIZE = 512 * 512;
std::unique_ptr<knowledge_base_t, deleter_t<knowledge_base_t>> knowledge_base_t::ms_instance;


knowledge_base_t* knowledge_base_t::instance()
{
    if (not ms_instance)
        throw exception_t("An instance of knowledge-base has not been initialized.");

    return ms_instance.get();
}


void knowledge_base_t::initialize(const filepath_t &path)
{
	assert(not ms_instance);
        
	path.dirname().mkdir();
    ms_instance.reset(new knowledge_base_t(path));

	predicate_library_t::instance()->filepath() = path + ".pred.dat";
}


knowledge_base_t::knowledge_base_t(const filepath_t &path)
    : m_state(STATE_NULL), m_version(KB_VERSION_1),	m_path(path),
	rules(path + "base.dat"),
	features(path + "ft1.cdb"),
	feat2rids(path + "ft2.cdb"),
	lhs2rids(path + ".lhs.cdb"),
	rhs2rids(path + ".rhs.cdb"),
	class2rids(path + ".cls.cdb")
{}


knowledge_base_t::~knowledge_base_t()
{
    finalize();
}


void knowledge_base_t::prepare_compile()
{
    if (m_state == STATE_QUERY)
        finalize();

    if (m_state == STATE_NULL)
    {
		rules.prepare_compile();
		features.prepare_compile();
		feat2rids.prepare_compile();
		lhs2rids.prepare_compile();
		rhs2rids.prepare_compile();
		class2rids.prepare_compile();

        m_state = STATE_COMPILE;
    }
}


void knowledge_base_t::prepare_query()
{
    if (m_state == STATE_COMPILE)
        finalize();

    if (m_state == STATE_NULL)
    {
		rules.prepare_query();
		features.prepare_query();
		feat2rids.prepare_query();
		lhs2rids.prepare_query();
		rhs2rids.prepare_query();
		class2rids.prepare_query();

        m_state = STATE_QUERY;
    }
}


void knowledge_base_t::finalize()
{
    if (m_state == STATE_NULL) return;

    kb_state_e state = m_state;
    m_state = STATE_NULL;

	if (state == STATE_COMPILE)
		write_spec(m_path + ".spec.txt");

	rules.finalize();
	features.finalize();
	feat2rids.finalize();
	lhs2rids.finalize();
	rhs2rids.finalize();
	class2rids.finalize();
}


void knowledge_base_t::write_spec(const filepath_t &path) const
{
	std::ofstream fo(path);

	fo << "version: " << m_version << std::endl;
	fo << "time-stamp: " << INIT_TIME.string() << std::endl;
	fo << "num-rules: " << rules.size() << std::endl;
	fo << "num-predicates: " << predicate_library_t::instance()->predicates().size() << std::endl;
}


} // end kb

} // end dav
