/* -*- coding: utf-8 -*- */

#pragma once

#include <ciso646>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include <iostream>
#include <vector>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "./lib/cdbpp.h"
#include "./s_expression.h"

#define hash_map std::unordered_map
#define hash_set std::unordered_set

#ifdef _WIN32
#define _sprintf sprintf_s
#define _sscanf  sscanf_s
#else
#define _sprintf sprintf
#define _sscanf  sscanf
#endif


/** A namespace of Henry. */
namespace phil
{

class phillip_main_t;

typedef long int index_t;
typedef long int axiom_id_t;
typedef std::string predicate_t;

namespace kb { class knowledge_base_t; }


/** Verboseness of debug printing */
enum verboseness_t
{
    NOT_VERBOSE,
    VERBOSE_1, VERBOSE_2, VERBOSE_3, VERBOSE_4,
    FULL_VERBOSE
};


/** Hash of string.
 * Use instead of std::string for acceleration. */
class string_hash_t
{
public:
    inline string_hash_t() : m_hash(0) {}
    inline string_hash_t(const string_hash_t& h) : m_hash(h.m_hash) {}
    inline string_hash_t(const std::string& s);

    static inline string_hash_t get_unknown_hash();

    const std::string& string() const { return ms_strs.at(m_hash); }
    operator const std::string& () const { return ms_strs.at(m_hash); }
    
    inline string_hash_t& operator=(const std::string &s);
    inline string_hash_t& operator=(const string_hash_t &h);
    
    inline bool operator>(const string_hash_t &x) const;
    inline bool operator<(const string_hash_t &x) const;
    inline bool operator==(const char *s) const;
    inline bool operator!=(const char *s) const;
    inline bool operator==(const string_hash_t &h) const;
    inline bool operator!=(const string_hash_t &h) const;

    inline const unsigned& get_hash() const { return m_hash; }

    inline bool is_constant() const;
    inline bool is_unknown()  const;
    
private:
    /** Assign a hash to str if needed, and return the hash of str. */
    static inline unsigned get_hash(std::string str);
    
    static hash_map<std::string, unsigned> ms_hashier;
    static std::vector<std::string> ms_strs;
    static unsigned ms_issued_variable_count;

    unsigned m_hash;    
};


typedef string_hash_t term_t;
typedef std::pair<term_t, term_t> substitution_t;


/** A struct of literal. */
struct literal_t
{
    inline literal_t() {}
    inline literal_t(const std::string &_pred, bool _truth = true);
    inline literal_t(
        predicate_t _predicate, const std::vector<term_t> _terms,
        bool _truth = true);
    inline literal_t(
        const std::string &_predicate, const std::vector<term_t> _terms,
        bool _truth = true);
    inline literal_t(
        const std::string &_predicate,
        const term_t& term1, const term_t& term2,
        bool _truth = true);
    inline literal_t(
        const std::string &_predicate,
        const std::string &term1, const std::string &term2,
        bool _truth = true);
    literal_t(const sexp::stack_t &s);
    
    bool operator > (const literal_t &x) const;
    bool operator < (const literal_t &x) const;
    bool operator == (const literal_t &x) const;
    bool operator != (const literal_t &x) const;

    inline std::string to_string(bool f_colored = false) const;
    inline std::string get_predicate_arity(
        bool do_distinguish_negation = true) const;
    
    size_t write_binary( char *bin ) const;
    size_t read_binary( const char *bin );
    
    void print(std::string *p_out_str, bool f_colored = false) const;
    
    static const int MAX_ARGUMENTS_NUM = 12;
    
    predicate_t predicate;
    std::vector<term_t> terms;
    bool truth;
};


/** A base class of components of phillip_main_t. */
class henry_component_interface_t
{    
public:
    /** Return ability to execute this component on current setting.
     *  @param[out] disp Error messages to be printed when return false. */
    virtual bool is_available(std::list<std::string> *disp) const = 0;
    virtual std::string repr() const = 0;
};


/** A wrapper class of cdb++. */
class cdb_data_t
{
public:
    cdb_data_t(std::string filename);
    ~cdb_data_t();

    void prepare_compile();
    void prepare_query();
    void finalize();

    inline void put(
        const void *key, size_t ksize, const void *value, size_t vsize);
    inline const void* get(
        const void *key, size_t ksize, size_t *vsize) const;
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


/* -------- Functions -------- */

/** Call this function on starting phillip. */
void initialize();

inline void print_console(const std::string &str);
inline void print_error(const std::string &str);
inline void print_warning(const std::string &str);
void print_console_fmt(const char *format, ...);
void print_error_fmt(const char *format, ...);
void print_warning_fmt(const char *format, ...);

inline void now(
    int *year, int *month, int *day, int *hour, int *min, int *sec);
void beginning_time(
    int *year, int *month, int *day, int *hour, int *min, int *sec);

std::string format(const char *format, ...);
std::string time_stamp();

/** Split string with separator.
 *  @param str       Target string.
 *  @param separator A list of separation character.
 *  @param MAX_NUM   A maximum number of separation. Default is unlimited.
 *  @return Result of split. */
std::vector<std::string> split(
    const std::string &str, const char *separator, const int MAX_NUM = -1);

/** Replace string.
 *  @param input input string.
 *  @param find string to replace.
 *  @param replace string to this.
 *  @return replacing result. */
std::string replace(
    const std::string &input,
    const std::string &find,
    const std::string &replace);

std::string strip(const std::string &input, const char *targets);
bool startswith(const std::string &str, const std::string &query);
bool endswith(const std::string &str, const std::string &query);

inline bool do_exist_file(const std::string &path);
inline std::string get_file_name(const std::string &path);
inline size_t get_file_size(const std::string &filename);
inline size_t get_file_size(std::istream &ifs);

std::string normalize_path(const std::string &target);

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
template <class It, bool USE_STREAM = false> std::string join(
    const It &s_begin, const It &s_end, const std::string &delim);
template <class It> std::string join(
    const It &s_begin, const It &s_end,
    const std::string &fmt, const std::string &delim);

/** Returns whether given map's keys includes given key. */
template <class T, class K> inline bool has_key(const T& map, const K& key);

/** Returns whether set1 and set2 have any intersection. */
template <class T> bool has_intersection(
    T s1_begin, T s1_end, T s2_begin, T s2_end);

/** Returns intersection of set1 and set2. */
template <class T> hash_set<T> intersection(
    const hash_set<T> &set1, const hash_set<T> &set2);

} // end phil


namespace std
{
template <> struct hash<phil::string_hash_t>
{
    size_t operator() (const phil::string_hash_t &s) const
    { return s.get_hash(); }
};
}

#include "./define.inline.h"
