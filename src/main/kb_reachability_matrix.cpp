#include <iomanip>

#include "./kb.h"

namespace dav
{

namespace kb
{


std::unique_ptr<heuristics_t> heuristics_t::ms_instance;
std::mutex heuristics_t::ms_mutex;


void heuristics_t::initialize(const filepath_t &path)
{
	ms_instance.reset(new heuristics_t(path));
}


heuristics_t* heuristics_t::instance()
{
	return ms_instance.get();
}


heuristics_t::heuristics_t(const filepath_t &path)
	: m_path(path)
{}


void heuristics_t::load()
{
	std::lock_guard<std::mutex> lock(ms_mutex);
	pos_t pos;
	size_t num, idx;

	m_fin.reset(new std::ifstream(m_path.c_str(), std::ios::binary | std::ios::in));

	m_fin->read((char*)&pos, sizeof(pos_t));
	m_fin->seekg(pos, std::ios::beg);

	m_fin->read((char*)&num, sizeof(size_t));
	for (size_t i = 0; i < num; ++i)
	{
		m_fin->read((char*)&idx, sizeof(idx));
		m_fin->read((char*)&pos, sizeof(pos_t));
		m_pid2pos[idx] = pos;
	}
}


void heuristics_t::construct()
{
	pos_t pos(0);

	m_fout.reset(new std::ofstream(m_path.c_str(), std::ios::binary | std::ios::out));
	m_fout->write((const char*)&pos, sizeof(pos_t)); // KEEP AREA





	// ---- WRITING MAP FROM PREDICATE-ID TO BYTE-POSITION

	pos = m_fout->tellp();
	size_t num = m_pid2pos.size();

	m_fout->write((const char*)&num, sizeof(size_t));

	for (auto it = m_pid2pos.begin(); it != m_pid2pos.end(); ++it)
	{
		m_fout->write((const char*)&it->first, sizeof(size_t));
		m_fout->write((const char*)&it->second, sizeof(pos_t));
	}

	m_fout->seekp(0, std::ios::beg);
	m_fout->write((const char*)&pos, sizeof(pos_t));

	m_fout.reset();
}


void heuristics_t::put(size_t idx1, const hash_map<size_t, float> &dist)
{
	std::lock_guard<std::mutex> lock(ms_mutex);
	size_t num(0);
	m_pid2pos[idx1] = m_fout->tellp();

	for (auto it = dist.begin(); it != dist.end(); ++it)
		if (idx1 <= it->first)
			++num;

	m_fout->write((const char*)&num, sizeof(size_t));
	for (auto it = dist.begin(); it != dist.end(); ++it)
	{
		if (idx1 <= it->first)
		{
			m_fout->write((const char*)&it->first, sizeof(size_t));
			m_fout->write((const char*)&it->second, sizeof(float));
		}
	}
}


float heuristics_t::get(predicate_id_t idx1, predicate_id_t idx2) const
{
	if (idx1 > idx2) std::swap(idx1, idx2);

	std::lock_guard<std::mutex> lock(ms_mutex);
	size_t num, idx;
	float dist;
	auto find = m_pid2pos.find(idx1);

	if (find == m_pid2pos.end()) return -1.0f;

	m_fin->seekg(find->second, std::ios::beg);
	m_fin->read((char*)&num, sizeof(size_t));

	for (size_t i = 0; i < num; ++i)
	{
		m_fin->read((char*)&idx, sizeof(size_t));
		m_fin->read((char*)&dist, sizeof(float));
		if (idx == idx2) return dist;
	}

	return -1.0f;
}


std::unordered_map<predicate_id_t, float> heuristics_t::get(predicate_id_t idx) const
{
	std::lock_guard<std::mutex> lock(ms_mutex);
	size_t num;
	float dist;
	std::unordered_map<predicate_id_t, float> out;
	auto find = m_pid2pos.find(idx);

	if (find == m_pid2pos.end()) return out;

	m_fin->seekg(find->second, std::ios::beg);
	m_fin->read((char*)&num, sizeof(size_t));

	for (size_t i = 0; i < num; ++i)
	{
		m_fin->read((char*)&idx, sizeof(predicate_id_t));
		m_fin->read((char*)&dist, sizeof(float));
		out.insert(std::make_pair(idx, dist));
	}

	return out;
}


void heuristics_t::print(const filepath_t &path)
{
	std::ofstream fo(param()->get("print-reachability"));
	auto *preds = predicate_library_t::instance();

	fo << "Reachability Matrix:" << std::endl;

	fo << std::setw(30) << std::right << "" << " | ";
	for (auto p : preds->predicates())
		fo << p.string() << " | ";
	fo << std::endl;

	for (auto p1 : preds->predicates())
	{
		predicate_id_t idx1 = preds->pred2id(p1);
		fo << std::setw(30) << std::right << p1.string() << " | ";

		for (auto p2 : preds->predicates())
		{
			predicate_id_t idx2 = preds->pred2id(p2);
			float dist = this->get(idx1, idx2);
			fo << std::setw(p2.string().length()) << dist << " | ";
		}
		fo << std::endl;
	}

}


}

}