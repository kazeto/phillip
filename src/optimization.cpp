#include "./optimization.h"


namespace phil
{

namespace opt
{


stochastic_gradient_descent_t::stochastic_gradient_descent_t(scheduler_t *eta)
: m_eta(eta)
{}


weight_t stochastic_gradient_descent_t::update(weight_t *w, gradient_t g, epoch_t e)
{
    weight_t old(*w);
    return ((*w) -= (g * (*m_eta)(e))) - old;
}


weight_t ada_grad_t::update(weight_t *w, gradient_t g, epoch_t e)
{
    weight_t old(*w);

    // INITIALIZE ACCUMULATION
    auto r = m_accumulations.find(w);
    if (r == m_accumulations.end())
        r = m_accumulations.insert(std::make_pair(w, 0.0)).first;
    assert(r != m_accumulations.end());

    /* FORMULA:
     * r <- r + g^2
     * w <- w - g * a / (root(r) + s)
     *   g : gradient
     *   a : learning rate
     *   r : accumulated gradient
     *   s : stabilizer */

    r->second += g * g;
    return ((*w) -= (g * (*m_eta)(e) / (std::sqrt(r->second) + m_s))) - old;
}


weight_t ada_delta_t::update(weight_t *w, gradient_t g, epoch_t e)
{
    weight_t old(*w);

    // INITIALIZE ACCUMULATION
    auto r = m_accumulations.find(w);
    if (r == m_accumulations.end())
        r = m_accumulations.insert(std::make_pair(w, std::make_pair(0.0, 0.0))).first;
    assert(r != m_accumulations.end());

    /* FORMULA:
    * r <- (r * d) + (1 - d) * g^2
    * u <- g * (root(v) + s) / (root(r) + s)
    * v <- (v * d) + (1 - d) * u^2
    * w <- w - u
    *   g : gradient
    *   d : decay rate
    *   u : update
    *   r : accumulated gradient
    *   v : accumulated update
    *   s : stabilizer */

    r->second.first = (m_d * r->second.first) + ((1 - m_d) * g * g);
    double u = g * (std::sqrt(r->second.second) + m_s) / (std::sqrt(r->second.first) + m_s);
    r->second.first = (m_d * r->second.first) + ((1 - m_d) * u * u);
    return ((*w) -= u) - old;
}


weight_t adam_t::update(weight_t *w, gradient_t g, epoch_t e)
{
    weight_t old(*w);

    // INITIALIZE ACCUMULATION
    auto r = m_accumulations.find(w);
    if (r == m_accumulations.end())
        r = m_accumulations.insert(std::make_pair(w, std::make_pair(0.0, 0.0))).first;
    assert(r != m_accumulations.end());

    /* FORMULA:
    * r1 <- (r1 * d1) + (1 - d1) * g
    * r2 <- (r2 * d2) + (1 - d2) * g^2
    * w <- w - (a * r1) / ((root(r2 / (1 - d2^t)) + s) * (1 - d1^t))
    *   g : gradient
    *   d1 : decay rate (L1)
    *   d2 : decay rate (L2)
    *   r1 : accumulated gradient (L1)
    *   r2 : accumulated gradient (L2)
    *   t : epoch
    *   a : learning rate
    *   s : stabilizer */

    // ACCUMULATE GRADIENTS
    r->second.first = (m_d1 * r->second.first) + ((1 - m_d1) * g);
    r->second.second = (m_d2 * r->second.second) + ((1 - m_d2) * g * g);

    const double &r1(r->second.first), &r2(r->second.second);
    double u =
        (m_a * r1) /
        ((std::sqrt(r2 / (1 - std::pow(m_d2, e))) + m_s) * (1 - std::pow(m_d1, e)));

    return ((*w) -= u) - old;
}


}

}