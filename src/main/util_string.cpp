#include "./util.h"


namespace dav
{

string_t string_t::lower() const
{
    string_t out(*this);
    for (auto &c : out) c = std::tolower(c);
    return out;
}


std::vector<string_t> string_t::split(const char *separator, const int MAX_NUM) const
{
    auto _find_split_index = [this](const char *separator, int begin) -> index_t
    {
        for (size_t i = begin; i < size(); ++i)
        {
            if (strchr(separator, at(i)) != NULL)
                return static_cast<int>(i);
        }
        return -1;
    };

    std::vector<string_t> out;
    int idx(0);

    while (idx < size())
    {
        int idx2(_find_split_index(separator, idx));

        if (idx2 < 0)
            idx2 = size();

        if (idx2 - idx > 0)
        {
            if (MAX_NUM > 0 and out.size() >= MAX_NUM)
                idx2 = size();
            out.push_back(substr(idx, idx2 - idx));
        }

        idx = idx2 + 1;
    }

    return out;
}


string_t string_t::replace(const std::string &from, const std::string &to) const
{
    size_t pos(0);
    string_t out(*this);

    if (from.empty()) return out;

    while ((pos = out.find(from, pos)) != std::string::npos)
    {
        out.std::string::replace(pos, from.length(), to);
        pos += to.length();
    }

    return out;
}


string_t string_t::strip(const char *targets) const
{
    size_t idx1(0);
    for (; idx1 < this->length(); ++idx1)
        if (strchr(targets, this->at(idx1)) == NULL)
            break;

    size_t idx2(this->length());
    for (; idx2 > idx1; --idx2)
        if (strchr(targets, this->at(idx2 - 1)) == NULL)
            break;

    return (idx1 == idx2) ? "" : this->substr(idx1, idx2 - idx1);
}


string_t string_t::slice(int i, int j) const
{
	return substr(i, j - i);
}


bool string_t::startswith(const std::string &query)
{
    if (query.size() <= this->size())
    {
        for (int i = 0; i < query.size(); ++i)
        {
            if (query.at(i) != this->at(i))
                return false;
        }
        return true;
    }
    else
        return false;
}


bool string_t::endswith(const std::string &query)
{
    if (query.size() <= this->size())
    {
        int q = query.size() - 1;
        int s = this->size() - 1;
        for (int i = 0; i < query.size(); ++i)
        {
            if (query.at(q - i) != this->at(s - i))
                return false;
        }
        return true;
    }
    else
        return false;
}


bool string_t::parse_as_function(string_t *pred, std::vector<string_t> *args) const
{
	int num_open(0), num_close(0);
	int idx_open(-1), idx_close(-1);
	std::list<int> commas;

	// FIND BRACKETS AND COMMAS
	for (int i = 0; i < size(); ++i)
	{
		char c = at(i);

		if (c == '(')
		{
			if (++num_open == 1)
				idx_open = i;
		}

		if (c == ')')
		{
			++num_close;
			if (num_open == num_close)
				idx_close = i;
			if (num_open < num_close)
				return false; // INVALID FORMAT
		}

		if (c == ',')
		{
			if (num_open == num_close + 1)
				commas.push_back(i);
		}
	}

	// SPLIT THE STRING INTO PREDICATE AND ARGUMENTS
	if (idx_open >= 0 and idx_close >= 0)
	{
		(*pred) = string_t(substr(0, idx_open)).strip(" ");
		args->clear();

		if (commas.empty())
		{
			if (idx_close - idx_open > 1)
			{
				string_t s = slice(idx_open + 1, idx_close).strip(" ");
				if (not s.empty())
					args->push_back(s);
			}
		}
		else
		{
			args->push_back(slice(idx_open + 1, commas.front()).strip(" "));
			for (auto it = commas.begin(); std::next(it) != commas.end(); ++it)
				args->push_back(slice((*it) + 1, (*std::next(it))).strip(" "));
			args->push_back(slice(commas.back() + 1, idx_close).strip(" "));
		}
	}
	else if (idx_open < 0 and idx_close < 0)
		(*pred) = strip(" ");

	if (pred->empty()) return false;

	for (auto t : (*args))
		if (t.empty()) return false;

	return true;
}

}