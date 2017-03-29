/* -*- coding: utf-8 -*- */

#pragma once

#include <sstream>
#include <cassert>


namespace dav
{






namespace util
{


inline void cdb_data_t::put(
    const void *key, size_t ksize, const void *value, size_t vsize)
{
    if (is_writable())
        m_builder->put(key, ksize, value, vsize);
}


inline const void* cdb_data_t::get(
    const void *key, size_t ksize, size_t *vsize) const
{
    return is_readable() ? m_finder->get(key, ksize, vsize) : NULL;
}


inline size_t cdb_data_t::size() const
{
    return is_readable() ? m_finder->size() : 0;
}


inline bool do_exist_file(const std::string &path)
{
    bool out(true);
    std::ifstream fin(path);
    if (not fin)
        out = false;
    else
        fin.close();
    return out;
}


inline std::string get_file_name( const std::string &path )
{
#ifdef _WIN32
    int idx = path.rfind("\\");
#else
    int idx = path.rfind("/");
#endif
    return ( idx >= 0 ) ? path.substr(idx+1) : path;
}


inline std::string get_directory_name(const std::string &path)
{
#ifdef _WIN32
    int idx = path.rfind("\\");
#else
    int idx = path.rfind("/");
#endif
    return (idx >= 0) ? path.substr(0, idx) : "";
}


inline size_t get_file_size(const std::string &filename)
{
    struct stat filestatus;
    stat( filename.c_str(), &filestatus );
    return filestatus.st_size;
}

  
inline size_t get_file_size(std::istream &ifs)
{
    size_t file_size =
        static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
    ifs.seekg(0, std::ios::beg);
    return file_size;
}


inline size_t string_to_binary(const std::string &str, char *out)
{
    size_t n(0);
    unsigned char size = static_cast<unsigned char>(str.size());

    std::memcpy(out + n, &size, sizeof(unsigned char));
    n += sizeof(unsigned char);

    std::memcpy(out + n, str.c_str(), sizeof(char)* str.size());
    n += sizeof(char)* str.size();

    return n;
}


inline size_t num_to_binary(const int num, char *out)
{
    unsigned char n = static_cast<unsigned char>(num);
    std::memcpy(out, &n, sizeof(unsigned char));
    return sizeof(unsigned char);
}


inline size_t bool_to_binary(const bool _bool, char *out)
{
    char c = _bool ? 1 : 0;
    std::memcpy(out, &c, sizeof(char));
    return sizeof(char);
}


template <class T> inline size_t to_binary(const T &value, char *out)
{
    std::memcpy(out, &value, sizeof(T));
    return sizeof(T);
}


inline size_t binary_to_string(const char *bin, std::string *out)
{
    size_t n(0);
    unsigned char size;
    char str[512];

    std::memcpy(&size, bin, sizeof(unsigned char));
    n += sizeof(unsigned char);

    std::memcpy(str, bin + n, sizeof(char)*size);
    str[size] = '\0';
    *out = std::string(str);
    n += sizeof(char)*size;

    return n;
}


inline size_t binary_to_num(const char *bin, int *out)
{
    unsigned char num;
    std::memcpy(&num, bin, sizeof(unsigned char));
    *out = static_cast<int>(num);
    return sizeof(unsigned char);
}


inline size_t binary_to_bool(const char *bin, bool *out)
{
    char c;
    std::memcpy(&c, bin, sizeof(char));
    *out = (c != 0);
    return sizeof(char);
}


template <class T> inline size_t binary_to(const char *bin, T *out)
{
    size_t size = sizeof(T);
    std::memcpy(out, bin, size);
    return size;
}


template <class It> std::string join(
    const It &s_begin, const It &s_end, const std::string &delimiter)
{
    std::ostringstream ss;
    for (It it = s_begin; it != s_end; ++it)
        ss << (it == s_begin ? "" : delimiter) << (*it);
    return ss.str();
}


template <class Container, class Function> std::string join_f(
    const Container &container, Function func, const std::string &delim)
{
    std::string out;
    for (auto e : container)
        out += (out.empty() ? "" : delim) + func(e);
    return out;
}


template <class It> bool has_intersection(
    It s1_begin, It s1_end, It s2_begin, It s2_end)
{
    for (It i1 = s1_begin; i1 != s1_end; ++i1)
    for (It i2 = s2_begin; i2 != s2_end; ++i2)
    if (*i1 == *i2)
        return true;

    return false;
}


template <class T> hash_set<T> intersection(
    const hash_set<T> &set1, const hash_set<T> &set2)
{
    bool set1_is_smaller = (set1.size() < set2.size());
    const hash_set<T> *smaller = (set1_is_smaller ? &set1 : &set2);
    const hash_set<T> *bigger = (set1_is_smaller ? &set2 : &set1);
    hash_set<T> out;

    for (auto it = smaller->begin(); it != smaller->end(); ++it)
    {
        if (bigger->find(*it) != bigger->end())
            out.insert(*it);
    }

    return out;
}


template <class Container, class Element>
inline bool has_element(const Container &c, const Element &e)
{
    return c.find(e) != c.end();
}


template <class T> std::pair<T, T> make_symmetric_pair(const T &x, const T &y)
{
    return (x < y) ? std::make_pair(x, y) : std::make_pair(y, x);
}


template <class Container> void erase(Container &c, size_t i)
{
    auto it = c.begin();
    for (size_t j = 0; j < i; ++j) ++it;
    c.erase(it);
}


} // end of util

} // end of phil

