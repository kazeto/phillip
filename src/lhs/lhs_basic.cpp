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
    std::string depth_str = sys()->param("depth");
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
    pg::proof_graph_t *out = new pg::proof_graph_t(sys()->get_input()->name);
    add_observations(out);

    for (int d = 0; (m_depth_max < 0 or d < m_depth_max); ++d)
    {
        const hash_set<pg::node_idx_t>
            *targets = out->search_nodes_with_depth(d);
        
        if (targets == NULL) break;

        for (auto it = targets->begin(); it != targets->end(); ++it)
            chain(*it, out);
    }

    return out;
}


void basic_lhs_enumerator_t::chain(
    pg::node_idx_t idx, pg::proof_graph_t *graph) const
{
    const kb::knowledge_base_t *base = sys()->knowledge_base();
    const pg::node_t &node = graph->node(idx);
    std::string arity = node.literal().get_predicate_arity();
    std::list<axiom_id_t> axioms_abd = base->search_axioms_with_rhs(arity);
    std::list<axiom_id_t> axioms_ded = base->search_axioms_with_lhs(arity);

    std::list< std::pair<lf::axiom_t, bool> > axioms; // <axiom, is_deduction>
    for (auto it = axioms_ded.begin(); it != axioms_ded.end(); ++it)
        axioms.push_back(std::make_pair(base->get_axiom(*it), true));
    for (auto it = axioms_abd.begin(); it != axioms_abd.end(); ++it)
        axioms.push_back(std::make_pair(base->get_axiom(*it), false));

    for (auto it = axioms.begin(); it != axioms.end(); ++it)
    {
        const lf::axiom_t &ax(it->first);
        bool is_forward(it->second);
        std::list< std::vector<pg::node_idx_t> > targets =
            graph->enumerate_targets_of_chain(ax, !is_forward, node.depth());

        for (auto v = targets.begin(); v != targets.end(); ++v)
        {
            pg::hypernode_idx_t from = graph->add_hypernode(*v);
            pg::hypernode_idx_t to = is_forward ?
                graph->forward_chain(from, ax) :
                graph->backward_chain(from, ax);

            // ---- DEBUG
            if (sys()->verbose() == FULL_VERBOSE)
            {
                const std::vector<pg::node_idx_t>
                    &hn_from = graph->hypernode(from),
                    &hn_to = graph->hypernode(to);
                std::string
                    str_from = join(hn_from.begin(), hn_from.end(), "%d", ","),
                    str_to = join(hn_to.begin(), hn_to.end(), "%d", ",");
                std::string disp(
                    is_forward ? "ForwardChain: " : "BackwardChain: ");
                disp += format("%d:[%s] <= %s <= %d:[%s]",
                    from, str_from.c_str(), ax.name.c_str(), to, str_to.c_str());
                std::cerr << time_stamp() << disp << std::endl;
            }
        }
    }
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
