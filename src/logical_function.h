#pragma once


#include "./s_expression.h"
#include "./define.h"


namespace phil
{

/** Namespace of logical-functions. */
namespace lf
{


enum logical_operator_t
{
    OPR_UNDERSPECIFIED,   /// Not defined
    OPR_LITERAL,          /// Literal
    OPR_AND,              /// And (as "^")
    OPR_OR,               /// Or (as "v")
    OPR_IMPLICATION,      /// Implication (as "=>")
    OPR_INCONSISTENT,     /// Inconsistent (as "xor")
    OPR_REQUIREMENT,      /// Requirement (as "require")
    OPR_UNIPP,            /// Unification-Postponement (as "unipp")
};


extern const std::string OPR_STR_NAME;
extern const std::string OPR_STR_AND;
extern const std::string OPR_STR_OR;
extern const std::string OPR_STR_IMPLICATION;
extern const std::string OPR_STR_INCONSISTENT;
extern const std::string OPR_STR_REQUIREMENT;
extern const std::string OPR_STR_UNIPP;
extern const std::string OPR_STR_EXARGSET; /// Exclusive argument set.
extern const std::string OPR_STR_ASSERTION;


/** A struct type of logical function with s-expression. */
class logical_function_t
{    
public:
    inline logical_function_t();
    inline logical_function_t(logical_operator_t _opr);
    inline logical_function_t(const literal_t &lit);
    logical_function_t(
        logical_operator_t _opr, const std::vector<literal_t> &literals);
    logical_function_t(const sexp::stack_t &s);

    inline bool is_operator(logical_operator_t opr) const;
    inline const std::vector<logical_function_t>& branches() const;
    inline const logical_function_t& branch(int i) const;
    inline const literal_t& literal() const;
    inline const std::string& param() const;
    bool param2int(int *out) const;
    bool param2double(double *out) const;

    /** Return string of logical-function. */
    inline std::string to_string(bool f_colored = false) const;

    /** Return true if lit is included in this. */
    bool do_include(const literal_t& lit) const;

    bool find_parameter(const std::string &query) const;
    bool scan_parameter(const std::string &format, ...) const;

    bool is_valid_as_implication() const;
    bool is_valid_as_inconsistency() const;
    bool is_valid_as_unification_postponement() const;
    bool is_valid_as_argument_set() const;
    bool is_valid_as_requirements() const;
    
    /** Return literals included in this. */
    inline std::vector<const literal_t*> get_all_literals() const;
    void get_all_literals(std::list<literal_t> *out) const;

    void enumerate_literal_branches(
        std::vector<const logical_function_t*> *out) const;

    inline std::vector<const literal_t*> get_lhs() const;
    inline std::vector<const literal_t*> get_rhs() const;

    size_t write_binary(char *bin) const;
    size_t read_binary(const char *bin);
    
    void print(std::string *p_out_str, bool f_colored = false) const;

    void add_branch(const logical_function_t &lf);

private:
    void get_all_literals_sub(
        std::vector<const literal_t*> *p_out_list) const;
    
    logical_operator_t m_operator;

    /** This is used when opr==OPR_LITERAL. */
    literal_t m_literal;

    /** Instances which are children of this. */
    std::vector<logical_function_t> m_branches;
    
    /** Optional parameters for each implements. */
    std::string m_param;
};


struct input_t
{
    std::string name;
    lf::logical_function_t obs;
    lf::logical_function_t req;
    lf::logical_function_t label;
};


struct axiom_t
{
    axiom_t() : id(-1) {}

    axiom_id_t  id;
    std::string name;
    lf::logical_function_t func;
};


/** Parses given string as S-expression and returns the result of parsing. */
void parse(const std::string &str, std::list<logical_function_t> *out);


}

}


#include "./logical_function.inline.h"


