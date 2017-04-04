#include <functional>
#include <algorithm>
#include <cassert>
#include <cstring>

#include "./parser.h"


namespace dav
{

namespace parse
{


condition_t operator&(const condition_t &c1, const condition_t &c2)
{
    return [=](char ch) { return c1(ch) and c2(ch); };
}

condition_t operator|(const condition_t &c1, const condition_t &c2)
{
    return [=](char ch) { return c1(ch) or c2(ch); };
}

condition_t operator!(const condition_t &c)
{
    return [=](char ch) {return not c(ch); };
}

condition_t is(char t)
{
    return [=](char ch) { return ch == t; };
}

condition_t is(const std::string &ts)
{
    return [=](char ch)
    {
        for (auto t : ts)
            if (ch == t) return true;
        return false;
    };
}

condition_t lower = [=](char ch) { return (ch >= 'a') and (ch <= 'z'); };
condition_t upper = [=](char ch) { return (ch >= 'A') and (ch <= 'Z'); };
condition_t alpha = lower | upper;
condition_t digit = [=](char ch) { return (ch >= '0') and (ch <= '9'); };
condition_t space = is(" \t\n\r");
condition_t quotation_mark = is("\'\"");
condition_t bracket = is("(){}[]<>");
condition_t newline = is('\n');
condition_t bad = [=](char ch) { return (ch == -1) or (ch == 0); };


formatter_t operator&(const formatter_t &f1, const formatter_t &f2)
{
    return [=](const std::string &s)
	{
		return static_cast<format_result_e>(std::min<int>(f1(s), f2(s)));
	};
}

formatter_t operator|(const formatter_t &f1, const formatter_t &f2)
{
    return [=](const std::string &s)
	{
		return static_cast<format_result_e>(std::max<int>(f1(s), f2(s)));
	};
}


formatter_t word(const std::string &w)
{
    return [=](const std::string &str)
    {
		if (str.empty()) return FMT_READING;

        auto len = str.length();

		if (len > w.length())
			return FMT_BAD;
		else 
		{
			if (str.back() == w.at(len - 1))
				return (len == w.length()) ? FMT_GOOD : FMT_READING;
			else
				return FMT_BAD;
		}
    };
}


formatter_t many(const condition_t &c)
{
    return [=](const std::string &str)
	{
		if (str.empty()) return FMT_READING;
		else return c(str.back()) ? FMT_GOOD : FMT_BAD;
	};
}


formatter_t startswith(const condition_t &c)
{
    return [=](const std::string &str)
	{
		if (str.empty()) return FMT_READING;
		else return (c(str.front())) ? FMT_GOOD : FMT_BAD;
	};
}


formatter_t enclosed(char begin, char last)
{
    return [=](const std::string &str)
    {
		if (str.empty()) return FMT_READING;

		auto len = str.length();

        if (bad(str.back())) return FMT_BAD;

		if (str.front() != begin) return FMT_BAD;
		else
		{
			auto i = str.find(last, 1);

			if (i < 0)             return FMT_READING;
			else if (i == len - 1) return FMT_GOOD;
			else                   return FMT_BAD;
		}
    };
}


formatter_t quotation = many(not newline) & (enclosed('\'', '\'') | enclosed('\"', '\"'));
formatter_t comment = enclosed('#', '\n');
formatter_t general = many(not (bad | space | bracket | quotation_mark | is("#^!|=")));
formatter_t argument = (startswith(alpha) & many(alpha | digit)) | quotation;
formatter_t parameter = general | quotation;
formatter_t name = general | quotation;
const formatter_t &predicate = general;


stream_t::stream_t(std::istream *is)
    : std::unique_ptr<std::istream>(is), m_row(1), m_column(1)
{}


stream_t::stream_t(const std::string &path)
    : m_row(1), m_column(1)
{
    std::ifstream *fin = new std::ifstream(path);

    if (*fin)
        reset(fin);
    else
        throw;
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


string_t stream_t::read(const formatter_t &f)
{
	format_result_e past = FMT_READING;
    string_t out;
	auto pos = (*this)->tellg();
    char ch = (*this)->get();

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
			break;

		default:
			out += ch;
			ch = (*this)->get();
			break;
		}
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



parser_t::parser_t(std::istream *is)
    : m_stream(is)
{}


parser_t::parser_t(const std::string &path)
    : m_stream(path)
{}


void parser_t::read()
{
    string_t line;
	m_problem.reset();
	m_rule.reset();
	m_property.reset();

    auto expect = [&](const condition_t &c)
    {
        if (bad(m_stream.get(c)))
            throw;
    };

    auto expects = [&](const string_t &s)
    {
        for (auto c : s)
            if (bad(m_stream.get(is(c))))
                throw;
    };

    auto read_parameter = [&]() -> string_t
    {
        if (m_stream.get(is(':')))
            return m_stream.read(parameter);
        else
            return "";
    };

    /** Reads an atom from input-stream.
        If success, returns an atom read.
		If failed, returns an empty atom and roles the stream position back. */
    auto read_atom = [&]() -> atom_t
    {
		auto pos = m_stream->tellg();
        bool naf = false;
        bool neg = false;
        string_t pred;
        std::vector<term_t> terms;

		auto cancel = [&]() -> atom_t
		{
			m_stream->seekg(pos);
			return atom_t();
		};

		auto check = [&](const condition_t &c)
		{
			return bad(m_stream.get(c));
		};

        m_stream.skip();

        // READ NEGATION AS FAILURE
        if (m_stream.read(word("not ")))
        {
            naf = true;
            m_stream.skip();
        }

        // READ EQUALITY ATOM, LIKE (x = y)
        if (m_stream.get(is('(')))
        {
            m_stream.skip();
            string_t t1 = m_stream.read(argument);
            if (not t1) return cancel();

            m_stream.skip();
            neg = not bad(m_stream.get(is('!')));
            if (check(is('='))) return cancel();

            m_stream.skip();
            string_t t2 = m_stream.read(argument);
            if (not t2) return cancel();

            m_stream.skip();
            if (check(is(')'))) return cancel();
            m_stream.skip();
        }

        // READ BASIC ATOM, LIKE p(x)
        else
        {
            // READ TYPICAL NEGATION
            neg = not bad(m_stream.get(is('!')));
            m_stream.skip();

            // ---- READ PREDICATE
            pred = m_stream.read(predicate);
            if (pred.empty()) return cancel();

            m_stream.skip();
            if (check(is('('))) return cancel();
            m_stream.skip();

            // ---- READ ARGUMENTS
            while (true)
            {
                auto s = m_stream.read(argument);
                if (s.empty()) return cancel();

                terms.push_back(term_t(s));
                m_stream.skip();

                if (check(is(')')))
                {
                    m_stream.skip();
                    break;
                }
                else
                {
                    if (check(is(','))) return cancel();
                    m_stream.skip();
                }
            }
        }

        // ---- READ PARAMETER OF THE ATOM
        string_t param = read_parameter();

        return atom_t(pred, terms, neg, naf);
    };

    /** A function to parse conjunctions and disjunctions.
        If success, returns the pointer of the vector. */
    auto read_atom_array = [&](char delim, bool must_be_enclosed = false) -> conjunction_t
    {
        conjunction_t out;
        bool is_enclosed = bad(m_stream.get(is('{')));

		if (must_be_enclosed and not is_enclosed)
			throw;

        m_stream.skip();

        // ---- READ ATOMS
		atom_t atom = read_atom();
        while (atom.good())
        {
            out.push_back(atom);
            m_stream.skip();

            if (m_stream.get(is('}')))
            {
                m_stream.skip();
                break;
            }
            else
            {
                expect(is(delim));
                m_stream.skip();
            }

            atom = read_atom();
        }

        // ---- READ PARAMETER OF THE ATOMS
        string_t param;
        if (is_enclosed)
        {
            expect(is('}'));
            m_stream.skip();
            param = read_parameter();
        }

        return out;
    };

    /** A function to parse observations. */
    auto read_observation = [&]() -> problem_t
    {
        string_t n = m_stream.read(name);
        m_stream.skip();

        expect(is('{'));
        m_stream.skip();

		problem_t out;
        while (m_stream.get(is('}')))
        {
            string_t key = m_stream.read(many(alpha));

            if (key == "observe" and not out.observation().empty())
                throw; // Observation already exists.

            else if (key == "require" and not out.requirement().empty())
                throw; // Requirement already exists.

            m_stream.skip();
            auto atoms = read_atom_array('^', true);

			if (key == "observe")
				out.observation() = atoms;
			else if (key == "require")
				out.requirement() = atoms;
            else if (key == "choices")
				out.choices().push_back(atoms);

            m_stream.skip();
        }

        if (out.observation().empty())
            throw; /// Empty observation

		return out;
    };

    /** A function to parse definitions of rules. */
    auto read_rule = [&]() -> rule_t
    {
		rule_t out;
        string_t n = m_stream.read(name);
        m_stream.skip();

        expect(is('{'));
        m_stream.skip();

        out.lhs() = read_atom_array('^');
        m_stream.skip();

        expects("=>");
        m_stream.skip();

        out.rhs() = read_atom_array('^');
        m_stream.skip();

        expect(is('}'));

		if (out.lhs().empty() or out.rhs().empty())
			throw;

		return out;
    };

    /** A function to parse properties of predicates. */
    auto read_property = [&]() -> predicate_property_t
    {
        string_t pred = m_stream.read(predicate);
        m_stream.skip();
        expect(is('{'));
        m_stream.skip();

		predicate_id_t pid = predicate_library_t::instance()->add(pred);

		auto str2prop = [](const string_t &s) -> predicate_property_type_e
		{
			if (s == "irreflexive") return PRP_IRREFLEXIVE;
			if (s == "symmetric") return PRP_SYMMETRIC;
			if (s == "asymmetric") return PRP_ASYMMETRIC;
			if (s == "transitive") return PRP_TRANSITIVE;
			if (s == "right-unique") return PRP_RIGHT_UNIQUE;
			return PRP_NONE;
		};

        predicate_property_t::properties_t props;
        while (true)
        {
            string_t str = m_stream.read(many(alpha | digit | is('-')));
            m_stream.skip();

			predicate_property_type_e prop = str2prop(str);
			if (prop != PRP_NONE)
				props.insert(prop);
			else
				throw;

            if (m_stream.get(is('}')))
                break;
            else
            {
                expect(is(','));
                m_stream.skip();
            }
        }

		return predicate_property_t(pid, props);
    };

    while (not m_stream->eof())
    {
        m_stream.skip();
        
        string_t key = m_stream.read(many(alpha)).lower();

        if (key == "problem")
            m_problem.reset(new problem_t(read_observation()));

        else if (key == "rule")
            m_rule.reset(new rule_t(read_rule()));

        else if (key == "property")
            m_property.reset(new predicate_property_t(read_property()));
    }
}


}

}