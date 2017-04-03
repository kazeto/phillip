#include "./util.h"
#include <cstring>
#include <cassert>
#include <errno.h>

#include "./util.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif


namespace dav
{

bool filepath_t::find_file() const
{
	bool out(true);
	std::ifstream fin(*this);

	if (not fin)
		out = false;
	else
		fin.close();

	return out;
}


filepath_t filepath_t::filename() const
{
#ifdef _WIN32
	auto idx = rfind("\\");
#else
	auto idx = rfind("/");
#endif
	return (idx >= 0) ? substr(idx + 1) : (*this);
}


filepath_t filepath_t::dirname() const
{
#ifdef _WIN32
	auto idx = rfind("\\");
#else
	auto idx = rfind("/");
#endif
	return (idx >= 0) ? substr(0, idx) : "";
}


size_t filepath_t::filesize() const
{
	struct stat filestatus;
	stat(c_str(), &filestatus);
	return filestatus.st_size;
}


bool filepath_t::mkdir() const
{
	auto makedir = [](const std::string &path) -> bool
	{
#ifdef _WIN32
		if (::_mkdir(path.c_str()))
			return true;
#else
		if (::mkdir(path.c_str(), 0755))
			return true;
#endif
		else
			return (errno == EEXIST);
	};

#ifdef _WIN32
	const std::string det = "\\";
#else
	const std::string det = "/";
#endif

	auto dirs = dirname().split(det.c_str());
	filepath_t path = "";

	for (const auto &dir : dirs)
	{
		if (not path.empty())
		{
#ifdef _WIN32
			path += '\\';
#else
			path += '/';
#endif
		}

		path += dir;

		if (not path.empty())
			if (not makedir(path))
			{
				console()->error_fmt(
					"Failed to make directory \"%s\"", path.c_str());
				return false;
			}
	}

	return true;
}


void filepath_t::reguralize()
{
#ifdef _WIN32
	assign(replace("/", "\\"));
#else
	assign(replace("\\", "/"));
#endif

	while (true)
	{
		if (find("$TIME") != std::string::npos)
		{
			std::string _replace = format(
				"%04d%02d%02d_%02d%02d%02d",
				INIT_TIME.year, INIT_TIME.month, INIT_TIME.day,
				INIT_TIME.hour, INIT_TIME.min, INIT_TIME.sec);
			assign(replace("%TIME", _replace));
		}
		else
			break;
	}

	while (true)
	{
		if (find("$DAY") != std::string::npos)
		{
			std::string _replace = format(
				"%04d%02d%02d", INIT_TIME.year, INIT_TIME.month, INIT_TIME.day);
			assign(replace("%DAY", _replace));
		}
		else
			break;
	}
}


}