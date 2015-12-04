#pragma once

#include <functional>
#include <memory>
#include <algorithm>
#include <cmath>

#include "./define.h"
#include "./ilp_problem.h"


namespace phil
{

namespace opt
{


typedef std::function<gradient_t(weight_t)> normalizer_t;
typedef std::function<rate_t(epoch_t)> scheduler_t;


namespace norm /// A namespace for normalizer.
{
gradient_t l1_norm(weight_t w, rate_t r) { return r; }
gradient_t l2_norm(weight_t w, rate_t r) { return r * w; }
}


namespace lr /// A namespace for schedulers of learning rate.
{

rate_t linear(epoch_t e, rate_t r, rate_t d)
{
    return (std::max)(0.0, r - e * d);
}

rate_t exponential(epoch_t e, rate_t r, rate_t d)
{
    return r * std::pow(d, e);
}

}


class training_result_t
{
public:
    training_result_t(int epoch);
    virtual void write(std::ostream *os) const = 0; /// Output in XML-format.

    void add(const std::string name, weight_t before, weight_t after);

protected:
    hash_map<std::string, std::pair<weight_t, weight_t> > m_weights_before_after;
};



/** The base class for error function. */
class error_function_t
{
public:
    virtual double get(double true_obj, double false_obj) const = 0;
    virtual double gradient(
        double true_obj, double false_obj,
        double term, bool term_is_in_true_answer) const = 0;
};



class optimization_method_t
{
public:
    virtual ~optimization_method_t() {}

    /** Returns the value after updating of given weight.
     *  @param w The pointer of the weight to update.
     *  @param g Gradient of the weight.
     *  @param e Learning epoch.
     *  @reutrn Difference between the weight before/after updating. */
    virtual weight_t update(weight_t *w, gradient_t g, epoch_t e) = 0;

    /** Returns the name of this optimization method itself. */
    virtual std::string repr() const = 0;
};


class stochastic_gradient_descent_t : public optimization_method_t
{
public:
    stochastic_gradient_descent_t(scheduler_t *eta);

    virtual weight_t update(weight_t *w, gradient_t g, epoch_t e) override;
    virtual std::string repr() const override { return "stochastic-gradient-descent"; }

private:
    std::unique_ptr<scheduler_t> m_eta; /// Learning rate.
};


/** The optimization method proposed by J. Duchi et al, in 2011.
 *  Paper: http://www.jmlr.org/papers/volume12/duchi11a/duchi11a.pdf */
class ada_grad_t : public optimization_method_t
{
public:
    ada_grad_t(scheduler_t *eta, double s = 1.0);

    virtual weight_t update(weight_t *w, gradient_t g, epoch_t e) override;
    virtual std::string repr() const override { return "ada-grad"; }

private:
    hash_map<const weight_t*, double> m_accumulations;
    std::unique_ptr<scheduler_t> m_eta; /// Learning rate.
    double m_s; /// The parameter for stabilization.
};


/** The optimization method proposed by Matthew D. Zeiler in 2012.
 *  Paper: http://arxiv.org/pdf/1212.5701v1.pdf */
class ada_delta_t : public optimization_method_t
{
public:
    ada_delta_t(rate_t d, double s = 1.0);

    virtual weight_t update(weight_t *w, gradient_t g, epoch_t e) override;
    virtual std::string repr() const override { return "ada-delta"; }

private:
    /** Accumulated gradient and update for each weight. */
    hash_map<const weight_t*, std::pair<double, double> > m_accumulations;
    rate_t m_d; /// Decay rate.
    double m_s; /// The parameter for stabilization.
};


/** The optimization method proposed by D. Kingma and J. Ba in 2015.
 *  Paper: http://arxiv.org/pdf/1412.6980v8.pdf */
class adam_t : public optimization_method_t
{
public:
    adam_t(rate_t d1, rate_t d2, rate_t a, double s = 10e-8);

    virtual weight_t update(weight_t *w, gradient_t g, epoch_t e) override;
    virtual std::string repr() const override { return "adam"; }

private:
    hash_map<const weight_t*, std::pair<double, double> > m_accumulations;
    rate_t m_d1, m_d2; /// Decay rates.
    rate_t m_a; /// Learning rate.
    double m_s; /// The parameter for stabilization.
};


}

}