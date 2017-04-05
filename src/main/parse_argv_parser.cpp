#include "./parse.h"

namespace dav
{

namespace parse
{


std::set<string_t> argv_parser_t::ACCEPTABLE_MODES
{
	"compile", "c",
	"infer", "i",
	"learn", "l",
};


std::list<argv_parser_t::option_t> argv_parser_t::ACCEPTABLE_OPTS
{
	{ "-k", "PATH",   "Path of knowledge base.", "./compiled" },
	{ "-T", "SECOND", "Timeout in seconds.",     "None"       },
	{ "-P", "NUM",    "Multi-threading.",        "1"          },
	{ "-h", "",       "Print help.",             "1"          },
};


string_t argv_parser_t::help()
{
	std::list<string_t> strs
	{
		"dav MODE [OPTIONS] [INPUTS]",
		"",
		"MODE:",
		"\tcompile, c :: Compiles knowledge-base."
		"\tinfer, i :: Performs abductive reasoning."
		"\tlearn, l :: Supervised learning."
		"",
		"OPTIONS:"
	};

	// GENERATE DESCRIPTIONS OF OPTIONS
	for (const auto &opt : ACCEPTABLE_OPTS)
	{
		string_t s = "\t" + opt.name;
		if (opt.arg.empty())
		{
			if (opt.name.startswith("--"))
				s += "=" + opt.arg;
			else
				s += " " + opt.arg;
		}

		s += " :: " + opt.help;

		if (not opt.def.empty())
			s += " (default: " + opt.def + ")";

		strs.push_back(s);
	}

	return join(strs.begin(), strs.end(), "\n");
}


const argv_parser_t::option_t* argv_parser_t::find_opt(const string_t &name)
{
	for (const auto &opt : ACCEPTABLE_OPTS)
		if (name == opt.name)
			return &opt;

	throw exception_t(format("unknown option \"%s\"", name.c_str()));
}


argv_parser_t::argv_parser_t(int argc, char *argv[])
{
	if (argc <= 1)
		throw;

	m_mode.assign(argv[1]);
	if (ACCEPTABLE_MODES.count(m_mode) == 0)
		throw exception_t(format("unknown mode \"%s\"", m_mode), true);

	const option_t *prev = nullptr;
	bool do_get_input = false;

	auto parse_short_opt = [&](const string_t &arg)
	{
		bool can_take_arg = (arg.length() == 2);
		for (auto c : arg.substr(1))
		{
			const auto *opt = find_opt(format("-%c", c));
			if (opt->do_take_arg())
			{
				if (not can_take_arg)
					throw exception_t(format("option \"-%c\" takes argument", c));
				else
					prev = opt;
			}
			else
				add_opt(format("-%c", c), "");
		}
	};

	auto parse_long_opt = [&](const string_t &arg)
	{
		auto idx = arg.find('=');
		string_t name, value;

		if (idx == std::string::npos)
		{
			name = arg;
			value = "";
		}
		else
		{
			name = arg.substr(0, idx);
			value = arg.substr(idx + 1);
		}

		add_opt(name, value);
	};

	for (int i = 2; i < argc; ++i)
	{
		string_t arg(argv[i]);

		// GET INPUT
		if (do_get_input)
			m_inputs.push_back(arg);

		// GET OPTION
		else if (prev == nullptr)
		{
			// LONG OPTION
			if (arg.startswith("--"))
				parse_long_opt(arg);

			// SHORT OPTION
			else if (arg.startswith("-"))
				parse_short_opt(arg);

			// STARTS GETTING INPUTS
			else
			{
				do_get_input = true;
				m_inputs.push_back(arg);
			}
		}

		// GET ARGUMENT OF PREVIOUS OPTION
		else
		{
			add_opt(prev->name, arg);
			prev = nullptr;
		}
	}
}


void argv_parser_t::add_opt(const string_t &n, const string_t &v)
{
	m_opts.push_back(std::make_pair(n, v));
}


}

}