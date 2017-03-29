/* -*- coding: utf-8 -*- */


#include "./util.h"
#include <cstdarg>
#include <ctime>


const int FAILURE_MKDIR = -1;


namespace dav
{


std::ostream& operator<<(std::ostream& os, const string_hash_t& t)
{
    return os << t.string();
}


duration_time_t time_watcher_t::duration() const
{
	auto now = std::chrono::system_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_begin);

	return duration.count() / 1000.0f;
}


bool time_watcher_t::timed_out(duration_time_t time_out) const
{
	return (time_out >= 0.0) and (duration() >= time_out);
}


time_point_t::time_point_t()
{
#ifdef _WIN32
	time_t t;
	struct tm ltm;
	time(&t);
	localtime_s(&ltm, &t);

	year = 1900 + ltm.tm_year;
	month = 1 + ltm.tm_mon;
	day = ltm.tm_mday;
	hour = ltm.tm_hour;
	min = ltm.tm_min;
	sec = ltm.tm_sec;
#else
	time_t t;
	tm *p_ltm;
	time(&t);
	p_ltm = localtime(&t);

	year = 1900 + p_ltm->tm_year;
	month = 1 + p_ltm->tm_mon;
	day = p_ltm->tm_mday;
	hour = p_ltm->tm_hour;
	min = p_ltm->tm_min;
	sec = p_ltm->tm_sec;
#endif
}

const time_point_t INIT_TIME;


std::string format(const char *format, ...)
{
    static const int SIZE = 256 * 256;
    char buffer[SIZE];

    va_list arg;
    va_start(arg, format);
#ifdef _WIN32
    vsprintf_s(buffer, SIZE, format, arg);
#else
    vsprintf(buffer, format, arg);
#endif
    va_end(arg);

    return std::string(buffer);
}


size_t filesize(std::istream &ifs)
{
	size_t file_size =
		static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
	ifs.seekg(0, std::ios::beg);
	return file_size;
}


} // end of dav
