/* -*- coding: utf-8 -*- */

#pragma once

#include <ciso646>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <cassert>
#include <cstring>

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


/** Wrapper class of std::string. */
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
	string_t slice(int i, int j) const;

	bool startswith(const std::string&);
	bool endswith(const std::string&);

	bool parse_as_function(string_t *pred, std::vector<string_t> *args) const;
};


/** A wrapper class of string_t to manage filepaths. */
class filepath_t : public string_t
{
public:
	filepath_t() {}
	filepath_t(const char *s) : string_t(s) { reguralize(); }
	filepath_t(const std::string &s) : string_t(s) { reguralize(); }
	filepath_t(const filepath_t &s) : string_t(s) {}

	bool find_file() const; /// Returns whether a file exists.

	filepath_t filename() const;
	filepath_t dirname() const;
	size_t filesize() const;

	bool mkdir() const;

private:
	void reguralize();
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

} // end of dav


namespace std
{

template <> struct hash<dav::string_hash_t>
{
	size_t operator() (const dav::string_hash_t &s) const
	{
		return s.get_hash();
	}
};

template <> struct hash<dav::string_t> : public hash<std::string>
{
	size_t operator() (const dav::string_t &s) const
	{
		return hash<std::string>::operator()(s);
	}
};

} // end of std


namespace dav
{

/** A class to strage parameters given by command-option. */
class parameter_strage_t : public std::unordered_map<string_t, string_t>
{
public:
	static parameter_strage_t* instance();

	void add(const string_t &key, const string_t &value);

	const string_t& get(const string_t &key, const string_t &def = "") const;
	int geti(const string_t &key, int def = -1) const;
	double getf(const string_t &key, double def = -1.0) const;

	bool has(const string_t &key) const;

private:
	parameter_strage_t() {}

	static std::unique_ptr<parameter_strage_t> ms_instance;
};

parameter_strage_t* param() { return parameter_strage_t::instance(); }


/** A class to print strings on the console. */
class console_t
{
public:
	static console_t* instance();

	void print(const std::string &str) const;
	void error(const std::string &str) const;
	void warn(const std::string &str) const;

	void print_fmt(const char *format, ...) const;
	void error_fmt(const char *format, ...) const;
	void warn_fmt(const char *format, ...) const;

	void add_indent();
	void sub_indent();

	int& verbosity() { return m_verbosity; }
	int verbosity() const { return m_verbosity; }

private:
	console_t() : m_indent(0) {}

	std::string time_stamp() const;
	std::string indent() const;

	static std::unique_ptr<console_t> ms_instance;
	static const int BUFFER_SIZE_FOR_FMT = 256 * 256;
	static std::mutex ms_mutex;

	int m_indent;
	int m_verbosity;
};

console_t* console() { return console_t::instance(); }

#define PRINT_VERBOSE_1(s) if (console()->verbosity() >= 1) console()->print(s)
#define PRINT_VERBOSE_2(s) if (console()->verbosity() >= 2) console()->print(s)
#define PRINT_VERBOSE_3(s) if (console()->verbosity() >= 3) console()->print(s)
#define PRINT_VERBOSE_4(s) if (console()->verbosity() >= 4) console()->print(s)
#define PRINT_VERBOSE_5(s) if (console()->verbosity() >= 5) console()->print(s)


/** A class to see duration-time. */
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


struct time_point_t
{
	time_point_t();
	string_t string() const;

	int year;
	int month;
	int day;
	int hour;
	int min;
	int sec;
};

extern const time_point_t INIT_TIME;



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
    virtual T* operator()() const { return NULL; }
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

    void put(const void *key, size_t ksize, const void *value, size_t vsize);
    const void* get(const void *key, size_t ksize, size_t *vsize) const;
    size_t size() const;

    const std::string& filename() const { return m_filename; }
    bool is_writable() const { return m_builder != NULL; }
    bool is_readable() const { return m_finder != NULL; }

private:
    std::string m_filename;
    std::ofstream  *m_fout;
    std::ifstream  *m_fin;
    cdbpp::builder *m_builder;
    cdbpp::cdbpp   *m_finder;
};



/** A class to read something from binary data. */
class binary_reader_t
{
public:
	binary_reader_t(const char *ptr, size_t len) : m_ptr(ptr), m_size(0), m_len(len) {}

	template <class T> void read(T *out)
	{
		std::memcpy(out, current(), sizeof(T));
		m_size += sizeof(T);
		assert(m_size <= m_len);
	}

	size_t size() { return m_size; }
	void reset() { m_size = 0; }

private:
	const char* current() { return m_ptr + m_size; }

	const char *m_ptr;
	size_t m_size;
	size_t m_len;
};

template <> void binary_reader_t::read<std::string>(std::string *out)
{
	small_size_t size;
	read<small_size_t>(&size);

	char str[512];
	std::memcpy(str, current(), sizeof(char)*size);
	str[size] = '\0';
	*out = std::string(str);
	m_size += sizeof(char)*size;
}



/** A class to write something as binary data. */
class binary_writer_t
{
public:
	binary_writer_t(char *ptr, size_t len) : m_ptr(ptr), m_size(0), m_len(len) {}

	template <class T> void write(const T &value)
	{
		std::memcpy(current(), &value, sizeof(T));
		m_size += sizeof(T);
		assert(m_size <= m_len);
	}

	size_t size() { return m_size; }
	void reset() { m_size = 0; }

private:
	char* current() { return m_ptr + m_size; }

	char *m_ptr;
	size_t m_size;
	size_t m_len;
};

template <> void binary_writer_t::write<std::string>(const std::string &value)
{
	size_t n(0);

	unsigned char size = static_cast<unsigned char>(value.size());
	std::memcpy(current(), &size, sizeof(unsigned char));
	m_size += sizeof(unsigned char);

	std::memcpy(current(), value.c_str(), sizeof(char)* value.size());
	m_size += sizeof(char)* value.size();
}



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


std::string format(const char *format, ...);

size_t filesize(std::istream &ifs);


/** Returns joined string. */
template <class It> std::string join(
	const It &s_begin, const It &s_end, const std::string &delimiter)
{
	std::ostringstream ss;
	for (It it = s_begin; it != s_end; ++it)
		ss << (it == s_begin ? "" : delimiter) << (*it);
	return ss.str();
}

/** Returns whether set1 and set2 have any intersection. */
template <class It> bool has_intersection(
	It s1_begin, It s1_end, It s2_begin, It s2_end)
{
	for (It i1 = s1_begin; i1 != s1_end; ++i1)
		for (It i2 = s2_begin; i2 != s2_end; ++i2)
			if (*i1 == *i2)
				return true;

	return false;
}


/** Returns intersection of set1 and set2. */
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


template <class T> std::pair<T, T> symmetric_pair(const T &x, const T &y)
{
	return (x < y) ? std::make_pair(x, y) : std::make_pair(y, x);
}


template <class Container> void erase(Container &c, size_t i)
{
	auto it = c.begin();
	for (size_t j = 0; j < i; ++j) ++it;
	c.erase(it);
}

} // end of phil


#include "./util.inline.h"
