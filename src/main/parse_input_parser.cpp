#include "./parse.h"

namespace dav
{

namespace parse
{



input_parser_t::input_parser_t(std::istream *is)
	: m_stream(is)
{}


input_parser_t::input_parser_t(const std::string &path)
	: m_stream(path)
{}


void input_parser_t::read()
{
	string_t line;
	m_problem.reset();
	m_rule.reset();
	m_property.reset();

	auto expect = [&](const condition_t &c)
	{
		if (bad(m_stream.get(c)))
			throw m_stream.exception(format("expected \'%c\'", c));
	};

	auto expects = [&](const string_t &s)
	{
		for (auto c : s)
			if (bad(m_stream.get(is(c))))
				throw m_stream.exception(format("expected \"%s\"", s));
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
		bool naf = false;
		bool neg = false;
		string_t pred;
		std::vector<term_t> terms;
		auto pos = m_stream.position();

		auto cancel = [&]() -> atom_t
		{
			m_stream.restore(pos);
			return atom_t();
		};

		auto fail = [&](const condition_t &c)
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
			if (fail(is('='))) return cancel();

			m_stream.skip();
			string_t t2 = m_stream.read(argument);
			if (not t2) return cancel();

			m_stream.skip();
			if (fail(is(')'))) return cancel();
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
			if (fail(is('('))) return cancel();
			m_stream.skip();

			// ---- READ ARGUMENTS
			while (true)
			{
				auto s = m_stream.read(argument);
				if (s.empty()) return cancel();

				terms.push_back(term_t(s));
				m_stream.skip();

				if (fail(is(')')))
				{
					if (fail(is(','))) return cancel();
					m_stream.skip();
				}
				else
				{
					m_stream.skip();
					break;
				}
			}
		}
		atom_t out(pred, terms, neg, naf);

		// ---- READ PARAMETER OF THE ATOM
		out.param() = read_parameter();

		return out;
	};

	/** A function to parse conjunctions and disjunctions.
	If success, returns the pointer of the vector. */
	auto read_atom_array = [&](char delim, bool must_be_enclosed = false) -> conjunction_t
	{
		conjunction_t out;
		bool is_enclosed = not bad(m_stream.get(is('{')));

		if (must_be_enclosed and not is_enclosed)
			throw m_stream.exception("expected \'{\'");

		m_stream.skip();

		// ---- READ ATOMS
		atom_t atom = read_atom();
		while (atom.good())
		{
			out.push_back(atom);
			m_stream.skip();

			if (m_stream.peek(not (is(delim) | is_general)))
				break;
			else
			{
				expect(is(delim));
				m_stream.skip();
			}

			atom = read_atom();
		}

		// ---- READ PARAMETER OF THE ATOMS
		if (is_enclosed)
		{
			expect(is('}'));
			m_stream.skip();
			out.param() = read_parameter();
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
		while (bad(m_stream.get(is('}'))))
		{
			string_t key = m_stream.read(many(alpha));

			// Observation already exists.
			if (key == "observe" and not out.observation().empty())
				throw m_stream.exception("multiple observation");

			// Requirement already exists.
			else if (key == "require" and not out.requirement().empty())
				throw m_stream.exception("multiple requirement");

			m_stream.skip();
			auto atoms = read_atom_array('^', true);

			if (key == "observe")
				out.observation() = atoms;
			else if (key == "require")
				out.requirement() = atoms;
			else if (key == "choice")
				out.choices().push_back(atoms);
			else
				throw m_stream.exception(
					format("unknown keyword \"%s\" was found", key.c_str()));

			m_stream.skip();
		}

		if (out.observation().empty())
			throw m_stream.exception("empty observation");

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

		if (out.lhs().empty())
			throw m_stream.exception("empty conjunction on left-hand-side");
		if (out.rhs().empty())
			throw m_stream.exception("empty conjunction on right-hand-side");

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
				throw m_stream.exception(
					format("unknown keyword \"%s\" was found", str.c_str()));
			;

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

	m_stream.skip();

	string_t key = m_stream.read(many(alpha)).lower();
	m_stream.skip();

	if (key == "problem")
		m_problem.reset(new problem_t(read_observation()));

	else if (key == "rule")
		m_rule.reset(new rule_t(read_rule()));

	else if (key == "property")
		m_property.reset(new predicate_property_t(read_property()));

	else
		throw m_stream.exception(
			format("unknown keyword \"%s\" was found", key.c_str()));
}

}

}