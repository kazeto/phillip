#include "./util.h"
#include <algorithm>
#include <cstdarg>


namespace dav
{

std::unique_ptr<console_t> console_t::ms_instance;
std::mutex console_t::ms_mutex;


console_t* console_t::instance()
{
	if (not ms_instance)
		ms_instance.reset(new console_t());

	return ms_instance.get();
}


void console_t::print(const std::string &str) const
{
	std::lock_guard<std::mutex> lock(ms_mutex);
	std::cerr << time_stamp() << indent() << str << std::endl;
}


void console_t::error(const std::string &str) const
{
	std::lock_guard<std::mutex> lock(ms_mutex);
	
#ifdef _WIN32
	std::cerr << " * ERROR * ";
#else
	std::cerr << "\33[0;41m * ERROR * \33[0m";
#endif

	std::cerr << str << std::endl;
}


void console_t::warn(const std::string &str) const
{
	std::lock_guard<std::mutex> lock(ms_mutex);

#ifdef _WIN32
	std::cerr << " * WARNING * ";
#else
	std::cerr << "\33[0;41m * WARNING * \33[0m";
#endif

	std::cerr << str << std::endl;
}


void console_t::print_fmt(const char *format, ...) const
{
	char buffer[BUFFER_SIZE_FOR_FMT];
	va_list arg;
	va_start(arg, format);

#ifdef _WIN32
	vsprintf_s(buffer, BUFFER_SIZE_FOR_FMT, format, arg);
#else
	vsprintf(buffer, format, arg);
#endif
	va_end(arg);

	print(buffer);
}


void console_t::error_fmt(const char *format, ...) const
{
	char buffer[BUFFER_SIZE_FOR_FMT];
	va_list arg;
	va_start(arg, format);

#ifdef _WIN32
	vsprintf_s(buffer, BUFFER_SIZE_FOR_FMT, format, arg);
#else
	vsprintf(buffer, format, arg);
#endif
	va_end(arg);

	error(buffer);
}


void console_t::warn_fmt(const char *format, ...) const
{
	char buffer[BUFFER_SIZE_FOR_FMT];
	va_list arg;
	va_start(arg, format);
#ifdef _WIN32
	vsprintf_s(buffer, BUFFER_SIZE_FOR_FMT, format, arg);
#else
	vsprintf(buffer, format, arg);
#endif
	va_end(arg);

	warn(buffer);
}


void console_t::add_indent()
{
	m_indent = std::min(5, m_indent + 1);
}


void console_t::sub_indent()
{
	m_indent = std::max(0, m_indent - 1);
}


std::string console_t::time_stamp() const
{
	time_point_t now;

#ifdef _WIN32
	return format(
		"# %02d/%02d/%04d %02d:%02d:%02d | ",
		now.month, now.day, now.year, now.hour, now.min, now.sec);
#else
	return format(
		"\33[0;34m# %02d/%02d/%04d %02d:%02d:%02d\33[0m] ",
		now.month, now.day, now.year, now.hour, now.min, now.sec);
#endif
}


std::string console_t::indent() const
{
	std::string out;

	for (int i = 0; i < m_indent; ++i)
		out += "    ";

	return out;
}


}