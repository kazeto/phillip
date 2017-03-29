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
        int s = query.size() - 1;
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


}