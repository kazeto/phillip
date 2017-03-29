/* -*- coding: utf-8 -*- */

#pragma once

#include <ciso646>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <cassert>
#include <sys/stat.h>
#include <iostream>
#include <initializer_list>
#include <vector>
#include <deque>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <functional>
#include <exception>

#include "./lib/cdbpp.h"

#define hash_map std::unordered_map
#define hash_set std::unordered_set

#ifdef _WIN32
#define _sprintf sprintf_s
#define _sscanf  sscanf_s
#define _vsscanf vsscanf_s
#else
#define _sprintf sprintf
#define _sscanf  sscanf
#define _vsscanf vsscanf
#endif


/** A namespace of David. */
namespace dav
{

class phillip_main_t;
class string_t;

typedef unsigned int bits_t;
typedef unsigned char small_size_t;
typedef long int index_t;
typedef std::string file_path_t;


typedef float duration_time_t;


namespace pg
{
    typedef index_t entity_idx_t;
    typedef index_t node_idx_t;
    typedef index_t edge_idx_t;
    typedef index_t hypernode_idx_t;
    typedef int depth_t;
}


/** Verboseness of debug printing */
enum verboseness_e
{
    NOT_VERBOSE,
    VERBOSE_1, VERBOSE_2, VERBOSE_3, VERBOSE_4,
    FULL_VERBOSE
};


class string_t : public std::string
{
public:
    string_t() {}
    string_t(const char *s) : std::string(s) {}
    string_t(const std::string &s) : std::string(s) {}

    inline explicit operator bool() const { return not empty(); }

    string_t lower() const;

    std::vector<string_t> split(const char *delim, const int MAX_NUM = -1) const;
    string_t replace(const std::string &from, const std::string &to) const;
	string_t strip(const char *targets) const;

    bool startswith(const std::string&);
    bool endswith(const std::string&);
};


/** Hash of string.
 * Use instead of std::string for acceleration.
 * Length of string must be less than 256. */
class string_hash_t
{
public:
    static string_hash_t get_unknown_hash();
    static void reset_unknown_hash_count();

    string_hash_t() : m_hash(0) {}
    string_hash_t(const string_hash_t& h);
    string_hash_t(const std::string& s);

    const std::string& string() const;
    operator const std::string& () const;

    string_hash_t& operator=(const std::string &s);
    string_hash_t& operator=(const string_hash_t &h);

    bool operator>(const string_hash_t &x) const { return m_hash > x.m_hash; }
    bool operator<(const string_hash_t &x) const { return m_hash < x.m_hash; }
    bool operator==(const char *s) const { return m_hash == ms_hashier.at(s); }
    bool operator!=(const char *s) const { return m_hash != ms_hashier.at(s); }
    bool operator==(const string_hash_t &x) const { return m_hash == x.m_hash; }
    bool operator!=(const string_hash_t &x) const { return m_hash != x.m_hash; }

    const unsigned& get_hash() const { return m_hash; }

    bool is_constant() const { return m_is_constant; }
    bool is_unknown()  const { return m_is_unknown; }

    bool is_unifiable_with(const string_hash_t&) const;

protected:
    /** Assign a hash to str if needed, and return the hash of str. */
    static unsigned get_hash(std::string str);

    static std::mutex ms_mutex_hash, ms_mutex_unknown;
    static hash_map<std::string, unsigned> ms_hashier;
    static std::deque<string_t> ms_strs;
    static unsigned ms_issued_variable_count;

    inline void set_flags(const std::string &str);

    unsigned m_hash;
    bool m_is_constant, m_is_unknown;

#ifdef _DEBUG
    std::string m_string;
#endif
};

std::ostream& operator<<(std::ostream& os, const string_hash_t& t);



/** A base class of components of phillip_main_t. */
class phillip_component_interface_t
{
public:
    phillip_component_interface_t(const phillip_main_t *master) : m_phillip(master) {};
    virtual ~phillip_component_interface_t() {}

    /** Returns whether this component can be used on current setting.
     *  @param[out] disp Error messages to be printed when this method returns false. */
    virtual bool is_available(std::list<std::string> *disp) const = 0;

    /** Write the detail of this component in XML-format. */
    virtual void write(std::ostream *os) const = 0;

    /** Returns whether the output is non-available or sub-optimal
     *  when this component has timed out. */
    virtual bool do_keep_validity_on_timeout() const = 0;

    const phillip_main_t *phillip() const { return m_phillip; }

protected:
    const phillip_main_t *m_phillip;
};


class phillip_exception_t : public std::runtime_error
{
public:
    phillip_exception_t(const std::string &what, bool do_print_usage = false)
        : std::runtime_error(what), m_do_print_usage(do_print_usage) {}
    bool do_print_usage() const { return m_do_print_usage; }
private:
    bool m_do_print_usage;
};


template <class T> class component_generator_t
{
public:
    virtual T* operator()(const phillip_main_t*) const { return NULL; }
};


namespace util
{

/** A wrapper class of cdb++. */
class cdb_data_t
{
public:
    cdb_data_t(std::string filename);
    ~cdb_data_t();

    void prepare_compile();
    void prepare_query();
    void finalize();

    inline void put(const void *key, size_t ksize, const void *value, size_t vsize);
    inline const void* get(const void *key, size_t ksize, size_t *vsize) const;
    inline size_t size() const;

    inline const std::string& filename() const { return m_filename; }
    inline bool is_writable() const { return m_builder != NULL; }
    inline bool is_readable() const { return m_finder != NULL; }

private:
    std::string m_filename;
    std::ofstream  *m_fout;
    std::ifstream  *m_fin;
    cdbpp::builder *m_builder;
    cdbpp::cdbpp   *m_finder;
};


class time_watcher_t
{
public:
    time_watcher_t() : m_begin(std::chrono::system_clock::now()) {}

    /** Returns duration time from m_begin in seconds. */
    duration_time_t duration() const;

    /** Returns whether the duration time exceeds timeout. */
    bool timed_out(duration_time_t timeout) const;

private:
    std::chrono::system_clock::time_point m_begin;
};



template <class T> class symmetric_pair : public std::pair<T, T>
{
public:
    symmetric_pair(T x, T y) : std::pair<T, T>(x, y)
    {
        if (first > second) std::swap(first, second);
    }
};



template <class Key, class Value> class triangular_matrix_t
: public hash_map<Key, hash_map<Key, Value> >
{
public:
    inline void insert(Key k1, Key k2, const Value &v)
    {
        this->regularize_keys(k1, k2);
        (*this)[k1].insert(std::make_pair(k2, v));
    }

    Value* find(Key k1, Key k2)
    {
        this->regularize_keys(k1, k2);

        auto found1 = this->hash_map<Key, hash_map<Key, Value> >::find(k1);
        if (found1 == this->end()) return NULL;

        auto found2 = found1->second.find(k2);
        return (found2 == found1->second.end()) ? NULL : &(found2->second);
    }

    const Value* find(Key k1, Key k2) const
    {
        this->regularize_keys(k1, k2);

        auto found1 = this->hash_map<Key, hash_map<Key, Value> >::find(k1);
        if (found1 == this->end()) return NULL;

        auto found2 = found1->second.find(k2);
        return (found2 == found1->second.end()) ? NULL : &(found2->second);
    }

protected:
    inline void regularize_keys(Key &k1, Key &k2) const { if (k1 > k2) std::swap(k1, k2); }
};



template <class T> class pair_set_t : public hash_map<T, hash_set<T> >
{
public:
    inline void insert(T x, T y)
    {
        this->regularize(x, y);
        (*this)[x].insert(y);
    }

    inline int count(T x, T y) const
    {
        this->regularize(x, y);
        auto found1 = this->find(x);
        if (found1 == this->end()) return 0;
        auto found2 = found1->second.find(y);
        return (found2 == found1->second.end()) ? 0 : 1;
    }

protected:
    inline void regularize(T &x, T &y) const { if (x > y) std::swap(x, y); }
};


/** A template class of list to be used as a key of std::map. */
template <class T> class comparable_list : public std::list<T>
{
public:
    comparable_list() : std::list<T>() {}
    comparable_list(const comparable_list<T> &x) : std::list<T>(x) {}
    comparable_list(const std::list<T> &x) : std::list<T>(x) {}

    bool operator>(const comparable_list<T> &x) const
    {
        if (this->size() != x.size()) return this->size() > x.size();
        auto it1(this->begin()), it2(x.begin());
        for (; it1 != this->end(); ++it1, ++it2)
        {
            if ((*it1) != (*it2)) return (*it1) > (*it2);
        }
        return false;
    }
};


/** This class is used to define a singleton class. */
template <class T> class deleter_t
{
public:
    void operator()(T const* const p) const { delete p; }
};


/* -------- Functions -------- */


/** Call this function on starting phillip. */
void initialize();

inline void print_console(const std::string &str);
inline void print_error(const std::string &str);
inline void print_warning(const std::string &str);

void print_console_fmt(const char *format, ...);
void print_error_fmt(const char *format, ...);
void print_warning_fmt(const char *format, ...);

void now(int *year, int *month, int *day, int *hour, int *min, int *sec);
void beginning_time(int *year, int *month, int *day, int *hour, int *min, int *sec);

std::string format(const char *format, ...);
std::string time_stamp();


inline bool do_exist_file(const std::string &path);
inline std::string get_file_name(const std::string &path);
inline std::string get_directory_name(const std::string &path);
inline size_t get_file_size(const std::string &filename);
inline size_t get_file_size(std::istream &ifs);
void mkdir(std::string path);

std::string reguralize_path(const std::string &target);
std::string indexize_path(std::string str, int idx);

bool parse_string_as_function_call(
    const std::string &str,
    std::string *pred, std::vector<std::string> *terms);

/** Convert string into binary and return size of binary.
 *  The size of string must be less than 255. */
inline size_t string_to_binary(const std::string &str, char *out);
inline size_t num_to_binary(const int num, char *out);
inline size_t bool_to_binary(const bool _bool, char *out);
template <class T> inline size_t to_binary(const T &value, char *out);

inline size_t binary_to_string(const char *bin, std::string *out);
inline size_t binary_to_num(const char *bin, int *out);
inline size_t binary_to_bool(const char *bin, bool *out);
template <class T> inline size_t binary_to(const char *bin, T *out);

/** Returns joined string.
 *  If USE_STREAM is true, uses ostringstream to join. */
template <class It> std::string join(
    const It &s_begin, const It &s_end, const std::string &delim);
template <class Container, class Function> std::string join_f(
    const Container &container, Function func, const std::string &delim);


/** Returns whether set1 and set2 have any intersection. */
template <class It> bool has_intersection(
    It s1_begin, It s1_end, It s2_begin, It s2_end);

/** Returns intersection of set1 and set2. */
template <class T> hash_set<T> intersection(
    const hash_set<T> &set1, const hash_set<T> &set2);

template <class Container, class Element>
inline bool has_element(const Container&, const Element&);

template <class T> inline std::pair<T, T> make_symmetric_pair(const T &x, const T &y);

template <class Container> void erase(Container &c, size_t i);

extern std::mutex g_mutex_for_print;


} // end util

} // end phil


namespace std
{

template <> struct hash<dav::string_hash_t>
{
    size_t operator() (const dav::string_hash_t &s) const
    { return s.get_hash(); }
};

template <> struct hash<dav::string_t> : public hash<std::string>
{
    size_t operator() (const dav::string_t &s) const
    { return hash<std::string>::operator()(s); }
};

}

#include "./util.inline.h"
