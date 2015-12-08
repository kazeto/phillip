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


struct normalizer_t
{
    virtual gradient_t operator()(weight_t w) const = 0;
    virtual void write(std::ostream *os) const = 0;
};


namespace norm /// A namespace for normalizer.
{

class l1_norm : public normalizer_t
{
public:
    l1_norm(rate_t r) : m_rate(r) {}
    virtual gradient_t operator()(weight_t w) const override { return m_rate; };
    virtual void write(std::ostream *os) const;
private:
    rate_t m_rate;
};


class l2_norm : public normalizer_t
{
public:
    l2_norm(rate_t r) : m_rate(r) {}
    virtual gradient_t operator()(weight_t w) const override { return m_rate * w; };
    virtual void write(std::ostream *os) const;
private:
    rate_t m_rate;
};

}


struct scheduler_t
{
    virtual rate_t operator()(epoch_t) const = 0;
    virtual void write(std::ostream *os) const = 0;
};


namespace lr /// A namespace for schedulers of learning rate.
{

class linear : public scheduler_t
{
public:
    linear(rate_t r, rate_t d) : m_r(r), m_d(d) {}
    virtual rate_t operator()(epoch_t e) const { return (std::max)(0.0, m_r - e * m_d); }
    virtual void write(std::ostream *os) const override;
private:
    rate_t m_r, m_d;
};


class exponential : public scheduler_t
{
public:
    exponential(rate_t r, rate_t d) : m_r(r), m_d(d) {}
    virtual rate_t operator()(epoch_t e) const { return m_r * std::pow(m_d, e); }
    virtual void write(std::ostream *os) const override;
private:
    rate_t m_r, m_d;
};

}


class feature_weights_t : public hash_map<feature_t, double>
{
public:
    double get(const feature_t &f);

    void load(const std::string &filename);
    void load(const opt::feature_weights_t &weights);
    void write(const std::string &filename) const; /// Outputs the feature weights.

private:
    static double get_random_weight();
};


class training_result_t
{
public:
    training_result_t(epoch_t epoch, double loss);
    virtual ~training_result_t() {}

    virtual void write(std::ostream *os) const; /// Output in XML-format.

    void add(const feature_t &name, gradient_t gradient, weight_t before, weight_t after);

protected:
    epoch_t m_epoch;
    double m_loss;
    hash_map<std::string, std::tuple<gradient_t, weight_t, weight_t> > m_update_log;
};



class activation_function_t
{
public:
    virtual double operate(const hash_set<feature_t> fs, feature_weights_t &ws) const = 0;
    virtual void backpropagate(
        const hash_set<feature_t> fs, feature_weights_t &ws, gradient_t g,
        hash_map<feature_t, gradient_t> *out) const = 0;
    virtual void write(const std::string &tag, std::ostream *os) const = 0;
};


namespace af
{

class sigmoid_t : public activation_function_t
{
public:
    sigmoid_t(double gain, double offset) : m_gain(gain), m_offset(offset) {}
    virtual double operate(const hash_set<feature_t> fs, feature_weights_t &ws) const override;
    virtual void backpropagate(
        const hash_set<feature_t> fs, feature_weights_t &ws, gradient_t g,
        hash_map<feature_t, gradient_t> *out) const override;
    virtual void write(const std::string &tag, std::ostream *os) const override;

private:
    double m_gain, m_offset;
};


class relu_t : public activation_function_t
{
public:
    relu_t(double offset) : m_offset(offset) {}
    virtual double operate(const hash_set<feature_t> fs, feature_weights_t &ws) const override;
    virtual void backpropagate(
        const hash_set<feature_t> fs, feature_weights_t &ws, gradient_t g,
        hash_map<feature_t, gradient_t> *out) const override;
    virtual void write(const std::string &tag, std::ostream *os) const override;

private:
    double m_offset;
};

}


/** The base class for error function. */
class loss_function_t
{
public:
    virtual double get(double true_obj, double false_obj) const = 0;
    virtual double gradient_true(double true_obj, double false_obj) const = 0;
    virtual double gradient_false(double true_obj, double false_obj) const = 0;
    virtual void write(std::ostream *os) const = 0;
};


class linear_loss_t : public loss_function_t
{
public:
    linear_loss_t(bool do_maximize) : m_do_maximize(do_maximize) {}
    virtual double get(double true_obj, double false_obj) const override;
    virtual double gradient_true(double true_obj, double false_obj) const override;
    virtual double gradient_false(double true_obj, double false_obj) const override;
    virtual void write(std::ostream *os) const override;
private:
    bool m_do_maximize;
};


class squared_loss_t : public loss_function_t
{
public:
    squared_loss_t(bool do_maximize, double margin);
    virtual double get(double true_obj, double false_obj) const override;
    virtual double gradient_true(double true_obj, double false_obj) const override;
    virtual double gradient_false(double true_obj, double false_obj) const override;
    virtual void write(std::ostream *os) const override;

private:
    bool m_do_maximize;
    double m_margin;
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

    /** Write the detail of this in XML-format. */
    virtual void write(std::ostream *os) const = 0;
};


class stochastic_gradient_descent_t : public optimization_method_t
{
public:
    stochastic_gradient_descent_t(scheduler_t *eta);

    virtual weight_t update(weight_t *w, gradient_t g, epoch_t e) override;
    virtual void write(std::ostream *os) const override;

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
    virtual void write(std::ostream *os) const override;

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
    virtual void write(std::ostream *os) const override;

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
    virtual void write(std::ostream *os) const override;

private:
    hash_map<const weight_t*, std::pair<double, double> > m_accumulations;
    rate_t m_d1, m_d2; /// Decay rates.
    rate_t m_a; /// Learning rate.
    double m_s; /// The parameter for stabilization.
};


normalizer_t* generate_normalizer(const std::string &key);
scheduler_t* generate_scheduler(const std::string &key);
activation_function_t* generate_activation_function(const std::string &key);
loss_function_t* generate_loss_function(const std::string &key, bool do_maximize);
optimization_method_t* generate_optimizer(const std::string &key);


}

}