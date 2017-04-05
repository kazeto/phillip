#include "./util.h"

namespace dav
{


cdb_data_t::cdb_data_t(std::string _filename)
	: m_filename(_filename), m_fout(NULL), m_fin(NULL),
	m_builder(NULL), m_finder(NULL)
{}


cdb_data_t::~cdb_data_t()
{
	finalize();
}


void cdb_data_t::prepare_compile()
{
	if (is_readable()) finalize();

	if (not is_writable())
	{
		m_fout = new std::ofstream(
			m_filename.c_str(),
			std::ios::binary | std::ios::trunc);
		if (m_fout->fail())
			throw exception_t(
				format("cannot open \"%s\"", m_filename.c_str()));
		else
			m_builder = new cdbpp::builder(*m_fout);
	}
}


void cdb_data_t::prepare_query()
{
	if (is_writable()) finalize();

	if (not is_readable())
	{
		m_fin = new std::ifstream(
			m_filename.c_str(), std::ios_base::binary);
		if (m_fin->fail())
			throw exception_t(
				format("cannot open \"%s\"", m_filename.c_str()));
		else
		{
			m_finder = new cdbpp::cdbpp(*m_fin);

			if (not m_finder->is_open())
				throw exception_t(
					format("cannot open \"%s\"", m_filename.c_str()));
		}
	}
}


void cdb_data_t::finalize()
{
	if (m_builder != NULL)
	{
		delete m_builder;
		m_builder = NULL;
	}
	if (m_fout != NULL)
	{
		delete m_fout;
		m_fout = NULL;
	}

	if (m_finder != NULL)
	{
		delete m_finder;
		m_finder = NULL;
	}

	if (m_fin != NULL)
	{
		delete m_fin;
		m_fin = NULL;
	}
}


void cdb_data_t::put(const void *key, size_t ksize, const void *value, size_t vsize)
{
	if (is_writable())
		m_builder->put(key, ksize, value, vsize);
}


const void* cdb_data_t::get(const void *key, size_t ksize, size_t *vsize) const
{
	return is_readable() ? m_finder->get(key, ksize, vsize) : NULL;
}


size_t cdb_data_t::size() const
{
	return is_readable() ? m_finder->size() : 0;
}

}