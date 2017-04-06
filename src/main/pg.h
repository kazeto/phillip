#pragma once

/** Definition of classes related of proof-graphs.
 *  A proof-graph is used to express a latent-hypotheses-set.
 *  @file   proof_graph.h
 *  @author Kazeto.Y
 */

#include <iostream>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <ciso646>

#include "./util.h"
#include "./fol.h"
#include "./kb.h"


namespace dav
{


/** A namespace about proof-graphs. */
namespace pg
{

class node_t;
class edge_t;
class proof_graph_t;


/** An enum of node-type. */
enum node_type_e
{
    NODE_UNSPECIFIED, /// Unknown type.
    NODE_OBSERVABLE,  /// Corresponds an observable literal.
    NODE_HYPOTHESIS,  /// Corresponds a hypothesized literal.
    NODE_REQUIRED     /// Corresponds a literal for pseudo-positive sample.
};


/** A class of nodes in proof-graphs. */
class node_t : public atom_t
{
public:
    /** @param lit     The literal assigned to this.
     *  @param type    The node type of this.
     *  @param idx     The index of this in proof_graph_t::m_nodes.
     *  @param depth   Distance from observations in the proof-graph.
     *  @param parents Indices of nodes being parents of this node. */
    node_t(const atom_t&, node_type_e, node_idx_t, depth_t);

    inline const node_type_e& type() const { return m_type; }

    /** Returns the index of this node in a proof-graph. */
    inline const node_idx_t& index() const { return m_index; }

    /** Returns the distance from nearest-observation in proof-graph.
     *  Nodes observed or required have depth 0.
     *  Nodes of equality have depth -1. */
    inline const depth_t& depth() const { return m_depth; }

	/** Returns the index of hypernode
	*  which was instantiated for instantiation of this node.
	*  CAUTION:
	*    If the node has plural parental-edges, this value is invalid.
	*    Such case can occurs when this node is an equality atom. */
	inline const hypernode_idx_t& master() const { return m_master; }
	inline       hypernode_idx_t& master() { return m_master; }

	inline const bool& active() const { return m_is_active; }
	inline       bool& active()       { return m_is_active; }

    string_t string() const;

private:
    node_type_e m_type;
    node_idx_t  m_index;
    hypernode_idx_t m_master;
    depth_t m_depth;
    predicate_id_t m_pred_id;

	/** If this is not active, this won't be included in ILP-problem. */
	bool m_is_active;

    hash_set<node_idx_t> m_parents;
    hash_set<node_idx_t> m_ancestors;
    hash_set<node_idx_t> m_relatives;
};


enum edge_type_e
{
    EDGE_UNSPECIFIED,
    EDGE_HYPOTHESIZE, /// For abduction.
    EDGE_IMPLICATION, /// For deduction.
    EDGE_UNIFICATION, /// For unification.
    EDGE_USER_DEFINED
};


/** A struct of edge to express explaining in proof-graphs.
 *  So, in abduction, the direction of implication is opposite.
 *  If this is unification edge, then id_axiom = -1. */
class edge_t
{
public:
    inline edge_t();
	inline edge_t(
		edge_type_e type, edge_idx_t idx,
		hypernode_idx_t tail, hypernode_idx_t head, rule_id_t id = -1);

    inline const edge_type_e& type() const { return m_type; }
	inline const edge_idx_t& index() const { return m_index; }
    inline const hypernode_idx_t& tail() const { return m_tail; }
    inline const hypernode_idx_t& head() const { return m_head; }
    inline const rule_id_t& rid()  const { return m_rid; }

	inline bool is_abduction() const { return type() == EDGE_HYPOTHESIZE; }
	inline bool is_deduction() const { return type() == EDGE_IMPLICATION; }
	inline bool is_unification() const { return type() == EDGE_UNIFICATION; }

private:
    edge_type_e m_type;
	edge_idx_t m_index;
    hypernode_idx_t m_tail;  /// A index of tail hypernode.
    hypernode_idx_t m_head;  /// A index of head hypernode.
    rule_id_t m_rid; /// The id of rule used.
};


class hypernode_t : std::vector<node_idx_t>
{
public:
	inline const hypernode_idx_t& index() const { return m_index; }
	inline       hypernode_idx_t& index()       { return m_index; }

private:
	hypernode_idx_t m_index;
};


/** A class of unification between atoms. */
class unifier_t : public std::pair<const atom_t*, const atom_t*>
{
public:
    unifier_t(const atom_t*, const atom_t*);
	unifier_t(proof_graph_t*, node_idx_t, node_idx_t); // TODO

	operator bool() const { return m_unifiable; }
	operator std::string() const { return string(); }

	/** The list of equality atoms which induced by this. */
    std::list<atom_t> products() const;

	edge_t edge(proof_graph_t*) const; // TODO

	const std::unordered_map<term_t, term_t>& map() const { return m_map; }

	bool unifiable() const { return m_unifiable; }

    string_t string() const;

private:
	/** Additional information which is necessary in order to unify nodes in a proof-graph. */
	struct option_t
	{
		proof_graph_t *pg;
		node_idx_t first, second;
	};

	void init();

    /** Map from the term before substitution
     *  to the term after substitution. */
    std::unordered_map<term_t, term_t> m_map;
	bool m_unifiable;

	std::unique_ptr<option_t> m_opt;
};


/** Class of forward-chaining and backward-chaining. */
class chainer_t : public std::tuple<rule_id_t, is_backward_t, std::vector<node_idx_t>>
{
public:
	chainer_t(proof_graph_t*, rule_id_t, is_backward_t, const std::vector<node_idx_t>&);

	std::list<atom_t> products() const;

	const rule_id_t& rid() const { return std::get<0>(*this); }
	const is_backward_t& is_backward() const { return std::get<1>(*this); }
	const std::vector<node_idx_t>& targets() const { return std::get<2>(*this); }

private:
	proof_graph_t *m_pg;
};


/** Class to manage nodes in a proof-graph. */
class nodes_array_t : public std::deque<node_t>
{
	friend proof_graph_t;
public:
	nodes_array_t(proof_graph_t *m) : m_master(m) {}

	node_idx_t add(node_type_e, const atom_t&, depth_t); // TODO

	one_to_many_t<hypernode_idx_t, node_idx_t> hn2nodes;
	one_to_many_t<predicate_id_t, node_idx_t>  pid2nodes;
	one_to_many_t<term_t, node_idx_t>          term2nodes;
	one_to_many_t<node_type_e, node_idx_t>     type2nodes;
	one_to_many_t<depth_t, node_idx_t>         depth2nodes;

private:
	proof_graph_t *m_master;
};


/** Class to manage hypernodes in a proof-graph. */
class hypernodes_array_t : public std::deque<hypernode_t>
{
	friend proof_graph_t;
public:
	hypernodes_array_t(proof_graph_t *m) : m_master(m) {}

	hypernode_idx_t add(const hypernode_t&); // TODO

	one_to_many_t<node_idx_t, hypernode_idx_t> node2hns;
	one_to_many_t<edge_idx_t, hypernode_idx_t> edge2hns;

private:
	proof_graph_t *m_master;
};


/** Class to manage edges in a proof-graph. */
class edges_array_t : public std::deque<edge_t>
{
	friend proof_graph_t;
public:
	edges_array_t(proof_graph_t *m) : m_master(m) {}

	edge_idx_t add(const edge_t&); // TODO

	one_to_many_t<rule_id_t, edge_idx_t>   rule2edges;
	one_to_many_t<edge_type_e, edge_idx_t> type2edges;

private:
	proof_graph_t *m_master;
};


/** Class to storage mutual-exclusions between nodes or edges. */
class mutual_exclusion_library_t
{
public:
	mutual_exclusion_library_t() {}

	const std::unordered_set<node_idx_t>* mutual_exclusive_with(const node_t&);
	const std::unordered_set<edge_idx_t>* mutual_exclusive_with(const edge_t&);

	/** Updates this library with given node.
	 *  Since it uses given node's index, the node must have a valid index. */
	void update(const node_t&);

	/** Updates this library with given edge.
     *  Since it uses given edge's index, the edge must have a valid index. */
	void update(const edge_t&);

private:
	one_to_many_t<node_idx_t, node_idx_t> m_muex_nodes;
	one_to_many_t<edge_idx_t, edge_idx_t> m_muex_edges;
	one_to_many_t<hypernode_idx_t, hypernode_idx_t> m_muex_hypernodes;

	/** Conditions where given nodes are mutual-exclusive. */
	std::map<std::pair<node_idx_t, node_idx_t>, std::set<atom_t>> m_conds;
};


/** Class to check whether the atoms can be valid on given presupposition. */
class validater_t
{
public:
	void presuppose(const node_t&);
	void presuppose(const hypernode_t&);

	void validate(const node_t&);

private:
	std::unordered_set<node_idx_t> m_pre_nodes;
	std::unordered_set<edge_idx_t> m_pre_edges;
};


/** A class to express proof-graph of latent-hypotheses-set. */
class proof_graph_t
{        
public:
    /** A class to generate candidates of chaining. */
    class chain_candidate_generator_t
    {
    public:
        class target_nodes_t : public std::vector<node_idx_t>
        {
        public:
            target_nodes_t(int size) : std::vector<node_idx_t>(size, -1) {}
            bool is_valid() const;
        };

        chain_candidate_generator_t(const proof_graph_t *g);

        void init(node_idx_t);
        void next();
        bool end() const { return m_pt_iter == m_patterns.end(); }
        bool empty() const { return m_axioms.empty(); }

        const std::vector<kb::predicate_id_t>& predicates() const;
        const std::list<target_nodes_t>& targets() const { return m_targets; }
        const std::list<std::pair<axiom_id_t, kb::is_backward_t> >& axioms() const { return m_axioms; }

    private:
        void enumerate();

        const proof_graph_t *m_graph;
        node_idx_t m_pivot;

        std::set<kb::conjunction_pattern_t> m_patterns;
        std::set<kb::conjunction_pattern_t>::const_iterator m_pt_iter;

        std::list<target_nodes_t> m_targets;
        std::list<std::pair<axiom_id_t, kb::is_backward_t> > m_axioms;
    };

    /** A class to detect potential loops in a proof-graph. */
    class loop_detector_t
    {
    public:
        loop_detector_t(const proof_graph_t *g);
        const std::list<std::list<edge_idx_t>>& loops() const { return m_loops; }

    private:
        void construct();
        const proof_graph_t *m_graph;
        std::list<std::list<edge_idx_t>> m_loops;
    };

    proof_graph_t(const phillip_main_t *main, const std::string &name = "");

    inline const phillip_main_t* phillip() const { return m_phillip; }
    inline void timeout(bool flag) { m_is_timeout = flag; }
    inline bool has_timed_out() const { return m_is_timeout; }
    inline const std::string& name() const { return m_name; }

    /** Deletes logs and enumerate hypernodes to be disregarded.
     *  Call this method after creation of proof-graph. */
    void post_process();
    
    inline node_idx_t add_observation(const atom_t &lit, int depth = 0);

    /** Add an element of requirements.
     *  The operator of req must be OPR_LITERAL or OPR_OR. */
    void add_requirement(const lf::logical_function_t &req);

    /** Add a new hypernode to this proof graph and update maps.
     *  If a hypernode which has same nodes already exists, return its index.
     *  @param indices Ordered node-indices of the new hyper-node.
     *  @return The index of added new hyper-node. */
    hypernode_idx_t add_hypernode(const std::vector<node_idx_t> &indices);

    /** Perform backward-chaining from the target node.
     *  @param axiom The logical function of implication to use.
     *  @return Index of new hypernode resulted in backward-chaining. */
    inline hypernode_idx_t backward_chain(
        const std::vector<node_idx_t> &target, const lf::axiom_t &axiom);
    
    /** Perform forward-chaining from the target node.
     *  @param axiom The logical function of implication to use.
     *  @return Index of new hypernode resulted in forward-chaining. */
    inline hypernode_idx_t forward_chain(
        const std::vector<node_idx_t> &target, const lf::axiom_t &axiom);

	nodes_array_t nodes;
	edges_array_t edges;
	hypernodes_array_t hypernodes;

    std::list<std::tuple<node_idx_t, node_idx_t, unifier_t> >
        enumerate_mutual_exclusive_nodes() const;

    std::list<hash_set<edge_idx_t> > enumerate_mutual_exclusive_edges() const;

    /** Return pointer of unifier for mutual-exclusiveness between given nodes.
     *  If not found, return NULL. */
    inline const unifier_t* search_mutual_exclusion_of_node(node_idx_t n1, node_idx_t n2) const;

    inline const hash_set<node_idx_t>*
        search_nodes_with_same_predicate_as(const atom_t&) const;

    /** Return pointer of set of nodes whose depth is equal to given value.
     *  If any node was found, return NULL. */
    inline const hash_set<node_idx_t>* search_nodes_with_depth(depth_t depth) const;

    /** Return the indices of edges connected with given hypernode.
    *  If any edge was not found, return NULL. */
    inline const hash_set<edge_idx_t>*
        search_edges_with_hypernode(hypernode_idx_t idx) const;
    inline const hash_set<edge_idx_t>*
        search_edges_with_node_in_tail(node_idx_t idx) const;
    inline const hash_set<edge_idx_t>*
        search_edges_with_node_in_head(node_idx_t idx) const;

    /** Return the indices of edges which are related with given node. */
    hash_set<edge_idx_t> enumerate_edges_with_node(node_idx_t idx) const;

    /** Return the index of edge connects between
     *  the given hypernode and its parent hypernode.
     *  If any edge is not found, return -1. */
    edge_idx_t find_parental_edge(hypernode_idx_t idx) const;

    /** Return one of parent hypernodes of a given hypernode.
     *  This method is not available to hypernodes for unification,
     *  because they can have plural parent.
     *  If any hypernode was not found, return -1. */
    inline hypernode_idx_t find_parental_hypernode(hypernode_idx_t idx) const;

    void enumerate_parental_edges(hypernode_idx_t idx, hash_set<edge_idx_t> *out) const;
    void enumerate_children_edges(hypernode_idx_t idx, hash_set<edge_idx_t> *out) const;
    void enumerate_parental_hypernodes(hypernode_idx_t idx, hash_set<hypernode_idx_t> *out) const;
    void enumerate_children_hypernodes(hypernode_idx_t idx, hash_set<hypernode_idx_t> *out) const;

    /** Returns indices of nodes whose evidences include given node. */
    void enumerate_descendant_nodes(node_idx_t idx, hash_set<node_idx_t> *out) const;

    void enumerate_overlapping_hypernodes(hypernode_idx_t idx, hash_set<hypernode_idx_t> *out) const;
    
    /** Return pointer of set of indices of hypernode which has the given node as its element.
     *  If any set was found, return NULL. */
    inline const hash_set<hypernode_idx_t>* search_hypernodes_with_node(node_idx_t i) const;

    /** Return the index of first one of hypernodes whose elements are same as given indices.
     *  If any hypernode was not found, return -1.  */
    template<class It> const hash_set<hypernode_idx_t>*
        find_hypernode_with_unordered_nodes(It begin, It end) const;

    /** Return the index of hypernode whose elements are same as given indices.
     *  If any hypernode was not found, return -1.  */
    hypernode_idx_t find_hypernode_with_ordered_nodes(
        const std::vector<node_idx_t> &indices) const;

    node_idx_t find_sub_node(term_t t1, term_t t2) const;
    node_idx_t find_neg_sub_node(term_t t1, term_t t2) const;

    /** Return index of the substitution nodes which hypothesized by transitivity */
    node_idx_t find_transitive_sub_node(node_idx_t i, node_idx_t j) const;

    /** Returns index of the unifying edge which unifies node i & j. */
    edge_idx_t find_unifying_edge(node_idx_t i, node_idx_t j) const;

    inline const hash_set<term_t>* find_variable_cluster(term_t t) const;
    std::list< const hash_set<term_t>* > enumerate_variable_clusters() const;

    /** Returns a list of chains which are needed to hypothesize given node. */
    hash_set<edge_idx_t> enumerate_dependent_edges(node_idx_t) const;

    /** Returns a list of chains which are needed to hypothesize given node. */
    void enumerate_dependent_edges(node_idx_t, hash_set<edge_idx_t>*) const;

    /** Returns a list of nodes which are needed to hypothesize given node. */
    void enumerate_dependent_nodes(node_idx_t, hash_set<node_idx_t>*) const;

    /** Enumerates unification nodes
     *  which are needed to satisfy conditions for given chaining.
     *  @param subs1 Unifying nodes which must be true.
     *  @param subs2 Unifying nodes which must not be true.
     *  @return Whether the chaining is possible. */
    bool check_availability_of_chain(
        pg::edge_idx_t idx,
        hash_set<node_idx_t> *subs1, hash_set<node_idx_t> *subs2) const;

    /** Returns whether nodes in given array can coexist. */
    template <class IterNodesArray>
    bool check_nodes_coexistability(IterNodesArray begin, IterNodesArray end) const;

    std::string hypernode2str(hypernode_idx_t i) const;
    std::string edge_to_string(edge_idx_t i) const;
        
    /** Returns whether the hypernode of hn includes only sub-nodes. */
    inline bool is_hypernode_for_unification(hypernode_idx_t hn) const;

    /** Returns whether given axioms has already applied to given hypernode. */
    bool axiom_has_applied(
        hypernode_idx_t hn, const lf::axiom_t &ax, bool is_backward) const;

    inline void add_attribute(const std::string &name, const std::string &value);

    virtual void print(std::ostream *os) const;

protected:
    /** A class of variable cluster.
     *  Elements of this are terms which are unifiable each other. */
    class unifiable_variable_clusters_set_t
    {
    public:
        unifiable_variable_clusters_set_t() : m_idx_new_cluster(0) {}
        
        /** Add unifiability of terms t1 & t2. */
        void add(term_t t1, term_t t2);

        void merge(const unifiable_variable_clusters_set_t &vc);

        inline const hash_map<index_t, hash_set<term_t> >& clusters() const;
        inline const hash_set<term_t>* find_cluster(term_t t) const;
        
        /** Check whether terms t1 & t2 are unifiable. */
        inline bool is_in_same_cluster(term_t t1, term_t t2) const;
        
    private:
        int m_idx_new_cluster;
        /** List of clusters.
         *  We use hash-map for erasure with keeping indices. */
        hash_map<index_t, hash_set<term_t> > m_clusters;

        /** Mapping from name of a variable
         *  to the index of cluster which the variable joins. */
        hash_map<term_t, index_t> m_map_v2c;

        /** All variables included in this cluster-set. */
        hash_set<term_t> m_variables;
    };

    /** Get whether it is possible to unify literals p1 and p2.     
     *  @param[in]  p1,p2 Target literals of unification.
     *  @param[out] out   The unifier of p1 and p2.
     *  @return Possibility to unify literals p1 & p2. */
    static bool check_unifiability(
        const atom_t &p1, const atom_t &p2,
        bool do_ignore_truthment, unifier_t *out = NULL);

    /** Return hash of node indices' list. */
    static size_t get_hash_of_nodes(std::list<node_idx_t> nodes);

    /** Adds a new node and updates maps.
     *  Here, mutual-exclusion and unification-assumptions for the new node
     *  are considered automatically.
     *  @param lit   A literal which the new node has.
     *  @param type  The type of the new node.
     *  @param depth The depth of the new node.
     *  @return The index of added new node. */
    node_idx_t add_node(
        const atom_t &lit, node_type_e type, int depth,
        const hash_set<node_idx_t> &parents);

    /** Adds a new edge.
     *  @return The index of added new edge. */
    edge_idx_t add_edge(const edge_t &edge);

    /** Performs backward-chaining or forward-chaining.
     *  Correspondence of each term is considered on chaining.
     *  @return Index of the new hypernode. If chaining has failed, returns -1. */
    hypernode_idx_t chain(
        const std::vector<node_idx_t> &from,
        const lf::axiom_t &axiom, bool is_backward);

    /** Get mutual exclusions around the literal 'target'. */
    void get_mutual_exclusions(
        const atom_t &target,
        std::list<std::tuple<node_idx_t, unifier_t> > *muexs) const;

    /** Is a sub-routine of add_node.
     *  Generates unification assumptions between target node
     *  and other nodes which have same predicate as target node has. */
    void _generate_unification_assumptions(node_idx_t target);

    /** Is a sub-routine of add_node.
     *  Generates mutual exclusiveness between target node and other nodes.
     *  @param muexs A list of mutual exclusions to create. They are needed to be enumerate by get_mutual_exclusions. */
    void _generate_mutual_exclusions(
        node_idx_t target,
        const std::list<std::tuple<node_idx_t, unifier_t> > &muexs);

    /** Is a sub-routine of _get_mutual_exclusion.
     *  Adds mutual-exclusions for target and nodes being inconsistent with it. */
    void _enumerate_mutual_exclusion_for_inconsistent_nodes(
        const atom_t &target,
        std::list<std::tuple<node_idx_t, unifier_t> > *out) const;

    /** Is a sub-routine of _get_mutual_exclusion.
     *  Adds mutual-exclusions between target and its counter nodes. */
    void _enumerate_mutual_exclusion_for_counter_nodes(
        const atom_t &target,
        std::list<std::tuple<node_idx_t, unifier_t> > *out) const;

    /** Is a sub-routine of chain.
     *  @param is_node_base Gives the mode of enumerating candidate edges.
     *                      If true, enumeration is performed on node-base.
     *                      Otherwise, it is on hypernode-base. */
    void _generate_mutual_exclusion_for_edges(edge_idx_t target, bool is_node_base);

    /** Returns whether given two nodes can coexistence in a hypothesis.
     *  @param uni The pointer of unifier between n1 and n2.
     *  @return Whether given nodes can coexist. */
    bool _check_nodes_coexistability(
        node_idx_t n1, node_idx_t n2, const unifier_t *uni = NULL) const;

    /** This is sub-routine of generate_unification_assumptions.
     *  Add a node and an edge for unification between node[i] & node[j].
     *  And, update m_vc_unifiable and m_maps.terms_to_sub_node. */
    void _chain_for_unification(node_idx_t i, node_idx_t j);

    inline bool _is_considered_unification(node_idx_t i, node_idx_t j) const;

    /** Return highest depth in nodes which given hypernode includes. */
    inline int get_depth_of_deepest_node(hypernode_idx_t idx) const;
    int get_depth_of_deepest_node(const std::vector<node_idx_t> &nodes) const;

    /** If you want to make conditions for unification,
     *  you can override this method. */
    virtual bool can_unify_nodes(node_idx_t, node_idx_t) const { return true; }

    void print_nodes(std::ostream *os) const;
    void print_axioms(std::ostream *os) const;
    void print_edges(std::ostream *os) const;
    void print_subs(std::ostream *os) const;
    void print_mutual_exclusive_nodes(std::ostream *os) const;
    void print_mutual_exclusive_edges(std::ostream *os) const;

    // ---- VARIABLES

    const phillip_main_t *m_phillip;
    
    std::string m_name;
    bool m_is_timeout; /// For timeout.

    std::vector<node_t> m_nodes;
    std::vector< std::vector<node_idx_t> > m_hypernodes;
    std::vector<edge_t> m_edges;

    hash_set<node_idx_t> m_observations; /// Indices of observation nodes.

    /** These are written in xml-file of output as attributes. */
    hash_map<std::string, std::string> m_attributes;
    
    /** Mutual exclusiveness betwen two nodes.
     *  If unifier of third value is satisfied, the node of the first key and the node of the second key cannot be hypothesized together. */
    util::triangular_matrix_t<node_idx_t, unifier_t> m_mutual_exclusive_nodes;

    hash_map<edge_idx_t, hash_set<edge_idx_t> > m_mutual_exclusive_edges;
    
    unifiable_variable_clusters_set_t m_vc_unifiable;

    /** Indices of hypernodes which include unification-nodes. */
    hash_set<hypernode_idx_t> m_indices_of_unification_hypernodes;

    /** Substitutions which is needed for the edge of key being true. */
    hash_map<edge_idx_t, std::list< std::pair<term_t, term_t> > > m_subs_of_conditions_for_chain;
    hash_map<edge_idx_t, std::list< std::pair<term_t, term_t> > > m_neqs_of_conditions_for_chain;

    std::hash<std::string> m_hasher_for_nodes;

    struct temporal_variables_t
    {
        void clear();

        /** Set of pair of nodes whose unification was postponed. */
        util::pair_set_t<node_idx_t> postponed_unifications;

        /** Set of pair of nodes
        *  whose unifiability has been already considered.
        *  KEY and VALUE express node pair, and KEY is less than VALUE. */
        util::pair_set_t<node_idx_t> considered_unifications;

        /** Used in _check_nodes_coexistability. */
        mutable util::triangular_matrix_t<node_idx_t, bool> coexistability_logs;
    } m_temporal;

    struct maps_t
    {
        /** Map from terms to the node index.
         *   - KEY1, KEY2 : Terms. KEY1 is less than KEY2.
         *   - VALUE : Index of node of "KEY1 == KEY2". */
        util::triangular_matrix_t<term_t, node_idx_t> terms_to_sub_node;

        /** Map from terms to the node index.
         *   - KEY1, KEY2 : Terms. KEY1 is less than KEY2.
         *   - VALUE : Index of node of "KEY1 != KEY2". */
        util::triangular_matrix_t<term_t, node_idx_t> terms_to_negsub_node;

        /** Map from depth to indices of nodes assigned the depth. */
        hash_map<depth_t, hash_set<node_idx_t> > depth_to_nodes;

        /** Map from axiom-id to hypernodes which have been applied the axiom. */
        hash_map< axiom_id_t, hash_set<hypernode_idx_t> >
            axiom_to_hypernodes_forward, axiom_to_hypernodes_backward;

        /** Map to get node from predicate.
         *  This is used on enumerating unification assumputions.
         *   - KEY1  : Predicate of the literal.
         *   - KEY2  : Num of terms of the literal.
         *   - VALUE : Indices of nodes which have the corresponding literal. */
        hash_map<predicate_t, hash_map<int, hash_set<node_idx_t> > >
            predicate_to_nodes;

        /** Map to get hypernodes which include given node. */
        hash_map<node_idx_t, hash_set<hypernode_idx_t> > node_to_hypernode;

        /** Map to get hypernodes from hash of unordered-nodes. */
        hash_map<size_t, hash_set<hypernode_idx_t> > unordered_nodes_to_hypernode;

        /** Map to get edges connecting given node. */
        hash_map<hypernode_idx_t, hash_set<edge_idx_t> > hypernode_to_edge;

        hash_map<node_idx_t, hash_set<edge_idx_t> > tail_node_to_edges, head_node_to_edges;

        /** Map to get nodes which have given term. */
        hash_map<term_t, hash_set<node_idx_t> > term_to_nodes;

        hash_map<kb::predicate_id_t, hash_set<node_idx_t> > pid_to_nodes;
    } m_maps;
};


}

}

#include "proof_graph.inline.h"


