/* -*- coding: utf-8 -*- */

#pragma once

#include <ciso646>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <sys/stat.h>
#include <iostream>
#include <initializer_list>
#include <vector>
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


/** A namespace of Henry. */
namespace phil
{

class phillip_main_t;
class string_t;

typedef unsigned int bits_t;
typedef unsigned char small_size_t;
typedef long int index_t;
typedef std::string file_path_t;

typedef long int axiom_id_t;
typedef small_size_t arity_t;
typedef small_size_t term_idx_t;
typedef string_t predicate_t;
typedef string_t predicate_with_arity_t;
typedef float duration_time_t;

namespace sexp
{
class sexp_t;
}

namespace kb
{
class knowledge_base_t;
class conjunction_pattern_t;

typedef unsigned long int argument_set_id_t;
typedef size_t predicate_id_t;
typedef bool is_backward_t;

typedef std::pair<index_t, term_idx_t> term_pos_t;
typedef std::pair<std::pair<kb::predicate_id_t, term_idx_t>,
                  std::pair<kb::predicate_id_t, term_idx_t> > hard_term_pair_t;

extern const axiom_id_t INVALID_AXIOM_ID;
extern const argument_set_id_t INVALID_ARGUMENT_SET_ID;
extern const predicate_id_t INVALID_PREDICATE_ID;
extern const predicate_id_t EQ_PREDICATE_ID;

inline bool is_backward(std::pair<axiom_id_t, is_backward_t> &p)
{ return p.second; }

}


namespace pg
{
    typedef index_t entity_idx_t;
    typedef index_t node_idx_t;
    typedef index_t edge_idx_t;
    typedef index_t hypernode_idx_t;
    typedef int depth_t;
}


namespace opt
{
    typedef std::string feature_t;
    typedef double error_t;
    typedef double weight_t;
    typedef double gradient_t;
    typedef double rate_t;
    typedef int epoch_t;
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

    bool to_arity(predicate_t *p, arity_t *n) const;
    std::pair<predicate_t, arity_t> to_arity() const;

    string_t to_lower() const;

    std::vector<string_t> split(const char *separator, const int MAX_NUM = -1) const;
    string_t replace(const std::string &from, const std::string &to) const;
	string_t strip(const char *targets) const;
};


/** Hash of string.
 * Use instead of std::string for acceleration. */
class string_hash_t
{
public:
    static inline string_hash_t get_unknown_hash();
    static inline void reset_unknown_hash_count();

    inline string_hash_t();
    inline string_hash_t(const string_hash_t& h);
    inline string_hash_t(const std::string& s);

    inline const std::string& string() const;
    inline operator const std::string& () const;

    inline string_hash_t& operator=(const std::string &s);
    inline string_hash_t& operator=(const string_hash_t &h);

    inline bool operator>(const string_hash_t &x) const;
    inline bool operator<(const string_hash_t &x) const;
    inline bool operator==(const char *s) const;
    inline bool operator!=(const char *s) const;
    inline bool operator==(const string_hash_t &h) const;
    inline bool operator!=(const string_hash_t &h) const;

    inline const unsigned& get_hash() const { return m_hash; }

    inline bool is_constant() const { return m_is_constant; }
    inline bool is_unknown()  const { return m_is_unknown; }
    inline bool is_hard_term() const { return m_is_hard_term; }

    inline bool is_unifiable_with(const string_hash_t&) const;

private:
    /** Assign a hash to str if needed, and return the hash of str. */
    static inline unsigned get_hash(std::string str);

    static std::mutex ms_mutex_hash, ms_mutex_unknown;
    static hash_map<std::string, unsigned> ms_hashier;
    static std::vector<std::string> ms_strs;
    static unsigned ms_issued_variable_count;

    inline void set_flags(const std::string &str);

    unsigned m_hash;
    bool m_is_constant, m_is_unknown, m_is_hard_term;

#ifdef _DEBUG
    std::string m_string;
#endif
};


typedef string_hash_t term_t;
typedef std::pair<term_t, term_t> substitution_t;


/** A struct of literal. */
class literal_t
{
public:
    static inline predicate_with_arity_t predicate_with_arity(
        const predicate_t &pred, int term_num, bool is_negated);
    static predicate_with_arity_t predicate_with_arity(
        kb::predicate_id_t id, bool is_negated);

    inline static literal_t equal(const term_t&, const term_t&);
    inline static literal_t not_equal(const term_t&, const term_t&);

    // DEFAULT CONSTRUCTOR
    inline literal_t() : m_pid(kb::INVALID_PREDICATE_ID) {}

    // CONSTRUCT A LITERAL WITH A PREDICATE-ID
    inline literal_t(kb::predicate_id_t pid, const std::vector<term_t>&, bool = true);

    // CONSTRUCT A LITERAL WITH A STRING AS THE PREDICATE
    inline literal_t(const predicate_t&, const std::vector<term_t>&, bool = true);
    inline literal_t(const predicate_t&, const std::initializer_list<std::string>&, bool = true);

    // CONSTRUCT A LITERAL WITH S-EXPRESSION
    literal_t(const sexp::sexp_t &s);

    bool operator > (const literal_t &x) const;
    bool operator < (const literal_t &x) const;
    bool operator == (const literal_t &x) const;
    bool operator != (const literal_t &x) const;

    inline std::string to_string(bool f_colored = false) const;

    predicate_t predicate() const;
    inline predicate_with_arity_t predicate_with_arity() const;
    kb::predicate_id_t pid() const { return m_pid; }

    inline std::vector<term_t>& terms()             { return m_terms; }
    inline const std::vector<term_t>& terms() const { return m_terms; }

    inline bool& truth()       { return m_truth; }
    inline bool  truth() const { return m_truth; }

    inline bool is_valid() const;
    inline bool is_equality() const;
    inline bool is_valid_as_functional_predicate() const;

    inline bool do_share_predicate_with(const literal_t &x) const;
    inline bool is_unifiable_with(const literal_t &x) const;

    bool is_functional() const;

    size_t write_binary(char *bin) const;
    size_t read_binary(const char *bin);

    void print(std::string *p_out_str, bool f_colored = false) const;

    static const int MAX_ARGUMENTS_NUM = 12;

private:
    inline void regularize();

    kb::predicate_id_t m_pid;
    predicate_t m_predicate;
    std::vector<term_t> m_terms;
    bool m_truth;
};


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


/** Functions for GoogleTest. */
std::ostream& operator<<(std::ostream& os, const term_t& t);
std::ostream& operator<<(std::ostream& os, const literal_t& t);


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


class timeout_t
{
public:
    timeout_t() : m_time(-1.0f) {}
    timeout_t(duration_time_t t) : m_time(t) {}

    inline void set(duration_time_t t) { m_time = t; }
    inline duration_time_t get() const { return m_time; }

    inline bool empty() const { return m_time <= 0.0f; }
    inline bool do_time_out(duration_time_t duration) const;
    inline bool do_time_out(const std::chrono::system_clock::time_point &begin) const;

private:
    duration_time_t m_time;
};


class xml_element_t
{
public:
    inline xml_element_t(const std::string &name, const std::string &text)
        : m_name(name), m_text(text) {}

    inline const std::string& name() const { return m_name; }
    inline const std::string& text() const { return m_text; }
    inline hash_map<std::string, std::string> attributes() const { return m_attr; }
    inline std::list<xml_element_t> children() const { return m_children; }

    inline void add_attribute(const std::string &key, const std::string &val);
    inline void remove_attribute(const std::string &key);

    inline void add_child(const xml_element_t &elem);
    inline xml_element_t& last_child() { return m_children.back(); }

    void print(std::ostream *os) const;

private:
    std::string m_name;
    std::string m_text;
    hash_map<std::string, std::string> m_attr;
    std::list<xml_element_t> m_children;
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

phil::duration_time_t duration_time(const std::chrono::system_clock::time_point &begin);

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

template <class Map, class Key, class Value, class Operator>
inline bool find_then(
    const Map &map, const Key &key, const Value &value, const Operator &opr);

/** Returns whether set1 and set2 have any intersection. */
template <class It> bool has_intersection(
    It s1_begin, It s1_end, It s2_begin, It s2_end);

/** Returns intersection of set1 and set2. */
template <class T> hash_set<T> intersection(
    const hash_set<T> &set1, const hash_set<T> &set2);

template <class Container, class Element>
inline bool has_element(const Container&, const Element&);

template <class T> inline std::pair<T, T> make_sorted_pair(const T &x, const T &y);

template <class Container> void erase(Container &c, size_t i);

extern std::mutex g_mutex_for_print;


} // end util

} // end phil


namespace std
{

template <> struct hash<phil::string_hash_t>
{
    size_t operator() (const phil::string_hash_t &s) const
    { return s.get_hash(); }
};

template <> struct hash<phil::string_t> : public hash<std::string>
{
    size_t operator() (const phil::string_t &s) const
    { return hash<std::string>::operator()(s); }
};

}

#include "./define.inline.h"
