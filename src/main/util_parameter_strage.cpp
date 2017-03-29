#include "./util.h"

namespace dav
{


std::unique_ptr<parameter_strage_t> parameter_strage_t::ms_instance;
const string_t EMPTY_STRING = "";


parameter_strage_t* parameter_strage_t::instance()
{
	if (not ms_instance)
		ms_instance.reset(new parameter_strage_t());

	return ms_instance.get();
}


void parameter_strage_t::add(const string_t &key, const string_t &value)
{
	insert(std::make_pair(key, value));
}


const string_t& parameter_strage_t::get(const string_t &key, const string_t &def) const
{
	auto it = find(key);
	return (it == end()) ? def : it->second;
}


int parameter_strage_t::geti(const string_t &key, int def) const
{
	auto it = find(key);

	if (it == end()) return def;

	try
	{
		return std::stoi(it->second);
	}
	catch (std::invalid_argument e)
	{
		console()->warn_fmt(
			"INVALID-ARGUMENT: Failed to convert a parameter into integer. (\"%s\" : \"%s\")",
			it->first.c_str(), it->second.c_str());
		return def;
	}
	catch (std::out_of_range e)
	{
		console()->warn_fmt(
			"OUT-OF-RANGE: Failed to convert a parameter into integer: (\"%s\" : \"%s\")",
			it->first.c_str(), it->second.c_str());
		return def;
	}
}


double parameter_strage_t::getf(const string_t &key, double def) const
{
	auto it = find(key);

	if (it == end()) return def;

	try
	{
		return std::stof(it->second);
	}
	catch (std::invalid_argument e)
	{
		console()->warn_fmt(
			"INVALID-ARGUMENT: Failed to convert a parameter into float. (\"%s\" : \"%s\")",
			it->first.c_str(), it->second.c_str());
		return def;
	}
	catch (std::out_of_range e)
	{
		console()->warn_fmt(
			"OUT-OF-RANGE: Failed to convert a parameter into float: (\"%s\" : \"%s\")",
			it->first.c_str(), it->second.c_str());
		return def;
	}
}


bool parameter_strage_t::has(const string_t &key) const
{
	return count(key) > 0;
}


}