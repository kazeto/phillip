/* -*- coding:utf-8 -*- */


#include "./lhs_enumerator.h"


namespace phil
{

namespace lhs
{


basic_lhs_enumerator_t::
basic_lhs_enumerator_t(bool do_deduction, bool do_abduction)
: m_do_deduction(do_deduction), m_do_abduction(do_abduction),
  m_depth_max(-1)
{
    std::string depth_str = sys()->param("max_depth");
    if (not depth_str.empty())
    {
        int depth;
        int ret = _sscanf(depth_str.c_str(), "%d", &depth);
        if (ret == 1 and depth > 0)
            m_depth_max = depth;
    }
}



pg::proof_graph_t* basic_lhs_enumerator_t::execute() const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    pg::proof_graph_t *out = new pg::proof_graph_t(sys()->get_input()->name);

    add_observations(out);

    for (int depth = 0; (m_depth_max < 0 or depth < m_depth_max); ++depth)
    {
        std::set<pg::chain_candidate_t> cands(enumerate_chain_candidates(out, depth));
        hash_map<axiom_id_t, lf::axiom_t> axioms;

        if (cands.empty()) break;

        // ENUMERATE AXIOMS USED HERE
        for (auto it = cands.begin(); it != cands.end(); ++it)
        if (axioms.count(it->axiom_id) == 0)
            axioms[it->axiom_id] = base->get_axiom(it->axiom_id);

        // EXECUTE CHAINING
        for (auto it = cands.begin(); it != cands.end(); ++it)
        {
            const lf::axiom_t &axiom = axioms.at(it->axiom_id);
            pg::hypernode_idx_t from = out->add_hypernode(it->nodes);
            pg::hypernode_idx_t to = it->is_forward ?
                out->forward_chain(from, axiom) :
                out->backward_chain(from, axiom);

            // PRINT FOR DEBUG
            if (sys()->verbose() == FULL_VERBOSE)
            {
                const std::vector<pg::node_idx_t>
                    &hn_from = out->hypernode(from),
                    &hn_to = out->hypernode(to);
                std::string
                    str_from = join(hn_from.begin(), hn_from.end(), "%d", ","),
                    str_to = join(hn_to.begin(), hn_to.end(), "%d", ",");
                std::string disp(
                    it->is_forward ? "ForwardChain: " : "BackwardChain: ");
                disp += format("%d:[%s] <= %s <= %d:[%s]",
                    from, str_from.c_str(), axiom.name.c_str(), to, str_to.c_str());
                std::cerr << time_stamp() << disp << std::endl;
            }
        }
    }

    return out;
}


std::set<pg::chain_candidate_t> basic_lhs_enumerator_t::
enumerate_chain_candidates(pg::proof_graph_t *graph, int depth) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    std::set<std::tuple<axiom_id_t, bool> > axioms =
        enumerate_applicable_axioms(graph, depth);
    std::set<pg::chain_candidate_t> out;

    for (auto it = axioms.begin(); it != axioms.end(); ++it)
    {
        lf::axiom_t axiom = base->get_axiom(std::get<0>(*it));
        bool is_forward(std::get<1>(*it));

        std::set<pg::chain_candidate_t> cands =
            graph->enumerate_candidates_for_chain(axiom, !is_forward, depth);
        out.insert(cands.begin(), cands.end());
    }

    return out;
}


std::set<std::tuple<axiom_id_t, bool> >
basic_lhs_enumerator_t::enumerate_applicable_axioms(
pg::proof_graph_t *graph, int depth) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    const hash_set<pg::node_idx_t>
        *nodes = graph->search_nodes_with_depth(depth);
    std::set<std::tuple<axiom_id_t, bool> > out;

    if (nodes == NULL) return out;

    for (auto it = nodes->begin(); it != nodes->end(); ++it)
    {
        const pg::node_t &n = graph->node(*it);
        std::string arity = n.literal().get_predicate_arity();

        if (m_do_deduction)
        {
            std::list<axiom_id_t> axioms = base->search_axioms_with_lhs(arity);
            for (auto ax = axioms.begin(); ax != axioms.end(); ++ax)
                out.insert(std::make_tuple(*ax, true));
        }
        if (m_do_abduction)
        {
            std::list<axiom_id_t> axioms = base->search_axioms_with_rhs(arity);
            for (auto ax = axioms.begin(); ax != axioms.end(); ++ax)
                out.insert(std::make_tuple(*ax, false));
        }
    }

    return out;
}


bool basic_lhs_enumerator_t::is_available(std::list<std::string>*) const
{ return true; }


std::string basic_lhs_enumerator_t::repr() const
{
    std::string name = m_do_deduction ?
        (m_do_abduction ? "BasicEnumerator" : "BasicDeductiveEnumerator") :
        (m_do_abduction ? "BasicAbductiveEnumerator" : "NullEnumerator");
    return name + format("(depth = %d)", m_depth_max);
}


}

}
