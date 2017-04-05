#include "./parse.h"

namespace dav
{

namespace parse
{

stream_t::stream_t(std::istream *is)
	: std::unique_ptr<std::istream>(is), m_row(1), m_column(1)
{}


stream_t::stream_t(const filepath_t &path)
	: m_row(1), m_column(1)
{
	std::ifstream *fin = new std::ifstream(path);

	if (*fin)
		reset(fin);
	else
		throw exception_t(format("cannot open \"%s\"", path.c_str()));
}


char stream_t::get(const condition_t &f)
{
	char c = (*this)->get();

	if (c != std::istream::traits_type::eof())
	{
		if (f(c))
		{
			if (c == '\n')
			{
				++m_row;
				m_column = 1;
			}
			else
				++m_column;

			return c;
		}
		else
		{
			(*this)->unget();
			return 0;
		}
	}

	return -1;
}


bool stream_t::peek(const condition_t &c) const
{
	return c((*this)->peek());
}


string_t stream_t::read(const formatter_t &f)
{
	format_result_e past = FMT_READING;
	string_t out;
	auto pos = (*this)->tellg();
	char ch = (*this)->get();
	int n_endl = 0;

	while (not bad(ch))
	{
		format_result_e res = f(out + ch);

		switch (f(out + ch))
		{
		case FMT_BAD:
			switch (past)
			{
			case FMT_GOOD:
				(*this)->unget();
				break;
			default:
				out = "";
				(*this)->seekg(pos);
				break;
			}
			goto END_READ;

		default:
			out += ch;
			ch = (*this)->get();
			break;
		}

		past = res;
	}

END_READ:

	for (const auto &c : out)
	{
		if (c == '\n')
		{
			++m_row;
			m_column = 1;
		}
		else
			++m_column;
	}

	return out;
}


void stream_t::ignore(const condition_t &f)
{
	char ch = get(f);

	while (not bad(ch))
		ch = get(f);
}


void stream_t::skip()
{
	do ignore(space);
	while (read(comment));
}


exception_t stream_t::exception(const string_t &str) const
{
	return exception_t(str + format(" at line %d, column %d.", row(), column()));
}


}

}