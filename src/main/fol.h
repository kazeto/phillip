#pragma once

#include "./util.h"


namespace dav
{

typedef long int axiom_id_t;
typedef small_size_t arity_t;
typedef small_size_t term_idx_t;
typedef string_t predicate_with_arity_t;
typedef size_t predicate_id_t;

typedef string_hash_t term_t;
typedef std::pair<term_t, term_t> substitution_t;

enum predicate_id_e : predicate_id_t
{
    INVALID_PREDICATE_ID = 0,
    EQ_PREDICATE_ID = 1,
};


namespace kb
{
class knowledge_base_t;
class conjunction_pattern_t;

typedef unsigned long int argument_set_id_t;
typedef bool is_backward_t;

typedef std::pair<index_t, term_idx_t> term_pos_t;
typedef std::pair<std::pair<predicate_id_t, term_idx_t>,
    std::pair<predicate_id_t, term_idx_t> > hard_term_pair_t;

extern const axiom_id_t INVALID_AXIOM_ID;
extern const argument_set_id_t INVALID_ARGUMENT_SET_ID;
extern const predicate_id_t EQ_PREDICATE_ID;

inline bool is_backward(std::pair<axiom_id_t, is_backward_t> &p)
{
    return p.second;
}

} // end of kb


/** A class to represent predicates. */
class predicate_t
{
public:
    predicate_t() : m_arity(0), m_pid(INVALID_PREDICATE_ID) {}
    predicate_t(const string_t &s, arity_t a);
    predicate_t(const string_t &s);
    predicate_t(predicate_id_t pid);
    predicate_t(std::ifstream *fi);

    void write(std::ofstream *fo) const;

    bool operator > (const predicate_t &x) const;
    bool operator < (const predicate_t &x) const;
    bool operator == (const predicate_t &x) const;
    bool operator != (const predicate_t &x) const;

    string_t string() const;
    inline operator std::string() const { return string(); }

    const string_t& predicate() const { return m_pred; }

    arity_t  arity() const { return m_arity; }
    arity_t& arity()       { return m_arity; }

    bool is_unary() const { return m_arity == 1; }
    bool is_binary() const { return m_arity == 2; }

    predicate_id_t  pid() const { return m_pid; }
    predicate_id_t& pid()       { return m_pid; }

    bool good() const;

private:
    string_t m_pred;
    arity_t m_arity;
    predicate_id_t m_pid;
};


/** A class of atoms in first-order logic. */
class literal_t
{
public:
    static literal_t equal(const term_t&, const term_t&, bool naf = false);
    static literal_t not_equal(const term_t&, const term_t&, bool naf = false);

    literal_t() {}
    literal_t(predicate_id_t pid, const std::vector<term_t>&, bool neg, bool naf);
    literal_t(const string_t&, const std::vector<term_t>&, bool neg, bool naf);
    literal_t(const string_t&, const std::initializer_list<std::string>&, bool neg, bool naf);

    bool operator > (const literal_t &x) const;
    bool operator < (const literal_t &x) const;
    bool operator == (const literal_t &x) const;
    bool operator != (const literal_t &x) const;

    const predicate_t& predicate() const { return m_predicate; }

    inline const std::vector<term_t>& terms() const { return m_terms; }
    inline const term_t& term(term_idx_t i) const { return m_terms.at(i); }
    inline       term_t& term(term_idx_t i)       { return m_terms.at(i); }

    inline bool truth() const { return not m_naf and not m_neg; }
    inline bool  naf() const { return m_naf; }
    inline bool& naf() { return m_naf; }
    inline bool  neg() const { return m_neg; }
    inline bool& neg() { return m_neg; }

    inline const string_t& param() const { return m_param; }
    inline       string_t& param()       { return m_param; }

    bool good() const; /// Returns whether this is valid.

    size_t write_binary(char *bin) const;
    size_t read_binary(const char *bin);

    string_t string() const;
    inline operator std::string() const { return string(); }

private:
    inline void regularize();

    predicate_t m_predicate;
    std::vector<term_t> m_terms;

    bool m_naf;
    bool m_neg;

    string_t m_param;
};

/** Functions for output-stream. */
std::ostream& operator<<(std::ostream& os, const literal_t& t);


/** A class to represent properties of predicates */
class predicate_property_t
{
public:
    enum unifiability_type_e : char
    {
        UNI_STRONGLY_LIMITED, /// Is expressed as '*'.
        UNI_WEAKLY_LIMITED,   /// Is expressed as '+'.
        UNI_UNLIMITED,        /// Is expressed as '.'.
    };

    enum property_type_e : char
    {
        PRP_NONE,
        PRP_IRREFLEXIVE, /// p(x,y) => (x != y)
        PRP_SYMMETRIC,   /// p(x,y) => p(y,x)
        PRP_ASYMMETRIC,  /// p(x,y) => !p(y,x)
        PRP_TRANSITIVE,  /// p(x,y) ^ p(y,z) => p(x,z)
        PRP_RIGHT_UNIQUE /// p(x,z) ^ p(y,z) => (x=y)
    };

    typedef std::unordered_set<property_type_e> properties_t;

    predicate_property_t();
    predicate_property_t(predicate_id_t pid, const properties_t &prp);
    predicate_property_t(std::ifstream *fi);

    void write(std::ofstream *fo) const;

    predicate_id_t pid() const { return m_pid; }
    const std::vector<unifiability_type_e>& unifiability() const { return m_unifiability; }

    // bool do_postpone(const pg::proof_graph_t*, index_t n1, index_t n2) const;

    bool is_irreflexive() const  { return m_properties.count(PRP_IRREFLEXIVE) > 0; }
    bool is_symmetric() const    { return m_properties.count(PRP_SYMMETRIC) > 0; }
    bool is_asymmetric() const   { return m_properties.count(PRP_ASYMMETRIC) > 0; }
    bool is_transitive() const   { return m_properties.count(PRP_TRANSITIVE) > 0; }
    bool is_right_unique() const { return m_properties.count(PRP_RIGHT_UNIQUE) > 0; }

    bool good() const;
    string_t string() const;

private:
    void assign_unifiability();

    predicate_id_t m_pid;
    properties_t m_properties;

    std::vector<unifiability_type_e> m_unifiability;
};


/** A class of the database of predicates. */
class predicate_library_t
{
public:
    static predicate_library_t* instance();

    predicate_library_t();

    void init();
    void load();
    void write() const;

    inline const string_t& filepath() const { return m_filename; }
    inline       string_t& filepath()       { return m_filename; }

    predicate_id_t add(const predicate_t&);
    predicate_id_t add(const literal_t&);

    void add_predicate_property(const predicate_property_t&);

    inline const std::vector<predicate_with_arity_t>& arities() const;
    inline predicate_id_t pred2id(const predicate_with_arity_t&) const;
    inline const predicate_with_arity_t& id2pred(predicate_id_t) const;

    inline const hash_map<predicate_id_t, predicate_property_t>& functional_predicates() const;
    inline const predicate_property_t* find_functional_predicate(predicate_id_t) const;
    inline const predicate_property_t* find_functional_predicate(const predicate_with_arity_t&) const;
    inline bool is_functional(predicate_id_t) const;
    inline bool is_functional(const predicate_with_arity_t&) const;

    inline const std::list<std::pair<term_idx_t, term_idx_t> >*
        find_inconsistent_terms(predicate_id_t, predicate_id_t) const;

private:
    void init();

    static std::unique_ptr<predicate_library_t> ms_instance; /// For singleton pattern.

    string_t m_filename;

    std::deque<predicate_t> m_predicates;
    hash_map<string_t, predicate_id_t> m_pred2id;

    hash_map<predicate_id_t, predicate_property_t> m_properties;
};


/** A class to represents conjunctions and disjunction. */
class atom_array_t : std::vector<literal_t>
{
public:
    atom_array_t() {}

    inline const string_t& param() const { return m_param; }
    inline       string_t& param()       { return m_param; }

private:
    string_t m_param;
};


/** A class to represents implication rules. */
class implication_t
{
public:
    implication_t() {}

    inline const string_t& name() const { return m_name; }
    inline       string_t& name()       { return m_name; }

    inline const atom_array_t& lhs() const { return m_lhs; }
    inline       atom_array_t& lhs()       { return m_lhs; }

    inline const atom_array_t& rhs() const { return m_rhs; }
    inline       atom_array_t& rhs()       { return m_rhs; }

private:
    string_t m_name;
    atom_array_t m_lhs, m_rhs;
};


} // end of dav

