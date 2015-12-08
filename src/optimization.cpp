#include <random>
#include "./optimization.h"
#include "./phillip.h"


namespace phil
{

namespace opt
{


namespace norm
{

void l1_norm::write(std::ostream *os) const
{
    (*os)
        << "<normalizer name=\"l1-norm\" r0=\"" << m_rate
        << "\">" << util::format("%lf * W", m_rate)
        << "</normalize>" << std::endl;
}


void l2_norm::write(std::ostream *os) const
{
    (*os)
        << "<normalizer name=\"l2-norm\" r0=\"" << m_rate
        << "\">" << util::format("%lf * (W ^ 2)", m_rate)
        << "</normalize>" << std::endl;
}

}


namespace lr
{

    void linear::write(std::ostream *os) const
    {
        (*os)
            << "<scheduler name=\"linear\" r0=\"" << m_r
            << "\" d=\"" << m_d
            << "\">" << util::format("r0 - (d * t)")
            << "</scheduler>" << std::endl;
    }


    void exponential::write(std::ostream *os) const
    {
        (*os)
            << "<scheduler name=\"exponential\" r0=\"" << m_r
            << "\" k=\"" << m_d
            << "\">" << util::format("r0 * (k ^ t)")
            << "</scheduler>" << std::endl;
    }

}


double feature_weights_t::get(const feature_t &f)
{
    auto found = find(f);

    if (found == end())
    {
        double init = get_random_weight();
        (*this)[f] = init;
        return init;
    }
    else
        return found->second;
}


void feature_weights_t::load(const std::string &filename)
{
    clear();

    std::ifstream fin(filename);
    char line[256];

    if (not fin)
    {
        util::print_warning_fmt(
            "cannot open feature-weight file: \"%s\"", filename.c_str());
        return;
    }

    while (fin.good() and not fin.eof())
    {
        fin.getline(line, 256);
        auto splitted = util::split(line, "\t");

        if (splitted.size() == 2)
        {
            double w;
            _sscanf(splitted.back().c_str(), "%lf", &w);
            (*this)[splitted.front()] = w;
        }
    }
}


void feature_weights_t::load(const opt::feature_weights_t &weights)
{
    clear();
    insert(weights.begin(), weights.end());
}


void feature_weights_t::write(const std::string &filename) const
{
    std::ofstream fout(filename);

    if (not fout)
    {
        util::print_warning_fmt(
            "cannot open feature-weight file: \"%s\"", filename.c_str());
        return;
    }

    for (auto p : (*this))
        fout << p.first << '\t' << p.second << std::endl;
}


double feature_weights_t::get_random_weight()
{
    static std::mt19937 mt(std::random_device().operator()());
    return std::uniform_real_distribution<double>(-1.0, 1.0).operator()(mt);
}


training_result_t::training_result_t(epoch_t epoch, double loss)
: m_epoch(epoch), m_loss(loss)
{}


void training_result_t::write(std::ostream *os) const
{
    (*os)
        << "<train state=\"done\" epoch=\"" << m_epoch
        << "\" loss=\"" << m_loss
        << "\">" << std::endl;

    for (auto p : m_update_log)
    {
        (*os)
            << "<weight feature=\"" << p.first
            << "\" gradient=\"" << std::get<0>(p.second)
            << "\" before=\"" << std::get<1>(p.second)
            << "\" after=\"" << std::get<2>(p.second)
            << "\"></weight>" << std::endl;
    }

    (*os) << "</train>" << std::endl;
}


void training_result_t::add(const feature_t &name, gradient_t gradient, weight_t before, weight_t after)
{
    m_update_log[name] = std::make_tuple(gradient, before, after);
}


namespace af
{

double sigmoid_t::operate(const hash_set<feature_t> fs, feature_weights_t &ws) const
{
    double sum(0.0);
    for (auto f : fs) sum += ws.get(f);
    return m_offset + m_scale * (std::tanh(m_gain * sum / 2.0) + 1.0) / 2.0;
}


void sigmoid_t::backpropagate(
    const hash_set<feature_t> fs, feature_weights_t &ws, gradient_t g,
    hash_map<feature_t, gradient_t> *out) const
{
    // g: gradient of axiom weight

    double s = (operate(fs, ws) - m_offset) / m_scale; // h(a)
    double g2 = m_gain * s * (1 - s) * m_scale;        // h'(a)

    assert(g2 >= 0.0); // THE GRADIENT OF SIGMOID IS ALWAYS POSITIVE VALUE.

    gradient_t gf = g * g2;
    for (auto f : fs)
    {
        if (out->count(f) == 0) (*out)[f] = gf;
        else                    (*out)[f] += gf;
    }
}


void sigmoid_t::write(const std::string &tag, std::ostream *os) const
{
    (*os)
        << "<" << tag
        << " name=\"sigoid\" gain=\"" << m_gain
        << "\" offset=\"" << m_offset
        << "\"></" << tag << ">" << std::endl;
}


double relu_t::operate(const hash_set<feature_t> fs, feature_weights_t &ws) const
{
    double sum(0.0);
    for (auto f : fs) sum += ws.get(f);
    return m_offset + std::max(0.0, sum);
}


void relu_t::backpropagate(
    const hash_set<feature_t> fs, feature_weights_t &ws, gradient_t g,
    hash_map<feature_t, gradient_t> *out) const
{
    double x = operate(fs, ws) - m_offset; // h(a)
    if (x == 0.0) return;

    for (auto f : fs)
    {
        if (out->count(f) == 0) (*out)[f] = g;
        else                    (*out)[f] += g;
    }
}


void relu_t::write(const std::string &tag, std::ostream *os) const
{
    (*os)
        << "<" << tag
        << " name=\"relu\" offset=\"" << m_offset
        << "\"></" << tag << ">" << std::endl;
}

}



double linear_loss_t::get(double true_obj, double false_obj) const
{
    return (false_obj - true_obj) * (m_do_maximize ? 1 : -1);
}


double linear_loss_t::gradient_true(double true_obj, double false_obj) const
{
    return (m_do_maximize ? -1.0 : 1.0);
}


double linear_loss_t::gradient_false(double true_obj, double false_obj) const
{
    return (m_do_maximize ? 1.0 : -1.0);
}


void linear_loss_t::write(std::ostream *os) const
{
    (*os)
        << "<loss name=\"linear\">"
        << (m_do_maximize ? "Ef - Et" : "Et - Ef")
        << "</loss>" << std::endl;
}



squared_loss_t::squared_loss_t(bool do_maximize, double margin)
: m_do_maximize(do_maximize), m_margin(margin)
{}


double squared_loss_t::get(double true_obj, double false_obj) const
{
    double d = std::abs(false_obj - true_obj) + m_margin;
    return d * d;
}


double squared_loss_t::gradient_true(double true_obj, double false_obj) const
{
    double d = std::abs(false_obj - true_obj) + m_margin;
    return 2 * d * (m_do_maximize ? -1.0 : 1.0);
}


double squared_loss_t::gradient_false(double true_obj, double false_obj) const
{
    double d = std::abs(false_obj - true_obj) + m_margin;
    return 2 * d * (m_do_maximize ? 1.0 : -1.0);
}


void squared_loss_t::write(std::ostream *os) const
{
    (*os)
        << "<loss name=\"squared\" margin=\"" << m_margin << "\">"
        << (m_do_maximize ? "(Ef - Et + margin)^2" : "(Et - Ef + margin)^2")
        << "</loss>" << std::endl;
}


stochastic_gradient_descent_t::stochastic_gradient_descent_t(scheduler_t *eta)
: m_eta(eta)
{}


weight_t stochastic_gradient_descent_t::update(weight_t *w, gradient_t g, epoch_t e)
{
    weight_t old(*w);
    return ((*w) -= (g * (*m_eta)(e))) - old;
}


void stochastic_gradient_descent_t::write(std::ostream *os) const
{
    (*os) << "<optimizer name=\"stochastic-gradient-descent\">" << std::endl;
    m_eta->write(os);
    (*os) << "</optimizer>" << std::endl;
}



ada_grad_t::ada_grad_t(scheduler_t *eta, double s)
: m_eta(eta), m_s(s)
{}


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


void ada_grad_t::write(std::ostream *os) const
{
    (*os)
        << "<optimizer name=\"ada-grad\" stabilizer=\"" << m_s
        << "\">" << std::endl;
    m_eta->write(os);
    (*os) << "</optimizer>" << std::endl;
}




ada_delta_t::ada_delta_t(rate_t d, double s)
: m_d(d), m_s(s)
{}


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


void ada_delta_t::write(std::ostream *os) const
{
    (*os)
        << "<optimizer name=\"ada-delta"
        << "\" decay-rate=\"" << m_d
        << "\" stabilizer=\"" << m_s
        << "\"></optimizer>" << std::endl;
}



adam_t::adam_t(rate_t d1, rate_t d2, rate_t a, double s)
: m_d1(d1), m_d2(d2), m_a(a), m_s(s)
{}


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


void adam_t::write(std::ostream *os) const
{
    (*os)
        << "<optimizer name=\"adam"
        << "\" decay-rate-1=\"" << m_d1
        << "\" decay-rate-2=\"" << m_d2
        << "\" learning-rate=\"" << m_a
        << "\" stabilizer=\"" << m_s
        << "\"></optimizer>" << std::endl;
}



normalizer_t* generate_normalizer(const phillip_main_t *ph)
{
    const std::string key = ph->param("normalizer");
    std::string pred;
    std::vector<std::string> terms;

    util::parse_string_as_function_call(key, &pred, &terms);

    if (pred == "l1" and terms.size() >= 1)
    {
        double r(0.01);
        _sscanf(terms.at(0).c_str(), "%lf", &r);
        return new norm::l1_norm(r);
    }

    if (pred == "l2" and terms.size() >= 1)
    {
        double r(0.01);
        _sscanf(terms.at(0).c_str(), "%lf", &r);
        return new norm::l2_norm(r);
    }

    return NULL;
}


scheduler_t* generate_scheduler(const std::string &key)
{
    std::string pred;
    std::vector<std::string> terms;

    util::parse_string_as_function_call(key, &pred, &terms);

    if (pred == "linear")
    {
        double r(0.1), d(0.005);
        if (terms.size() >= 1) _sscanf(terms.at(0).c_str(), "%lf", &r);
        if (terms.size() >= 2) _sscanf(terms.at(1).c_str(), "%lf", &d);
        return new lr::linear(r, d);
    }

    if (pred == "exponential" or pred == "exp")
    {
        double r(0.1), d(0.95);
        if (terms.size() >= 1) _sscanf(terms.at(0).c_str(), "%lf", &r);
        if (terms.size() >= 2) _sscanf(terms.at(1).c_str(), "%lf", &d);
        return new lr::exponential(r, d);
    }

    return NULL;
}


activation_function_t* generate_activation_function(const std::string &key)
{
    std::string pred;
    std::vector<std::string> terms;

    util::parse_string_as_function_call(key, &pred, &terms);

    if (pred == "sigmoid")
    {
        double g(1.0), o(0.0), s(1.0);
        if (terms.size() >= 1) _sscanf(terms.at(0).c_str(), "%lf", &g);
        if (terms.size() >= 2) _sscanf(terms.at(1).c_str(), "%lf", &o);
        if (terms.size() >= 3) _sscanf(terms.at(2).c_str(), "%lf", &s);
        return new af::sigmoid_t(g, o, s);
    }

    if (pred == "relu")
    {
        double o(0.0);
        if (terms.size() >= 1) _sscanf(terms.at(0).c_str(), "%lf", &o);
        return new af::relu_t(o);
    }

    return NULL;
}


loss_function_t* generate_loss_function(const std::string &key, bool do_maximize)
{
    std::string pred;
    std::vector<std::string> terms;

    util::parse_string_as_function_call(key, &pred, &terms);

    if (pred == "square")
    {
        double m(0.0);
        if (terms.size() >= 1) _sscanf(terms.at(0).c_str(), "%lf", &m);
        return new squared_loss_t(do_maximize, m);
    }

    return NULL;
}


optimization_method_t* generate_optimizer(const std::string &key)
{
    std::string pred;
    std::vector<std::string> terms;
    util::parse_string_as_function_call(key, &pred, &terms);

    if (pred == "sgd" and terms.size() >= 1)
        return new stochastic_gradient_descent_t(generate_scheduler(terms.at(0)));

    if (pred == "adagrad" and terms.size() >= 1)
    {
        double s(1.0);
        if (terms.size() >= 2) _sscanf(terms.at(1).c_str(), "%lf", &s);
        return new ada_grad_t(generate_scheduler(terms.at(0)), s);
    }

    if (pred == "adadelta" and terms.size() >= 1)
    {
        double d, s(1.0);
        _sscanf(terms.at(0).c_str(), "%lf", &d);
        if (terms.size() >= 2) _sscanf(terms.at(1).c_str(), "%lf", &s);
        return new ada_delta_t(d, s);
    }

    if (pred == "adam" and terms.size() >= 3)
    {
        double d1, d2, a, s(10e-8);
        _sscanf(terms.at(0).c_str(), "%lf", &d1);
        _sscanf(terms.at(1).c_str(), "%lf", &d2);
        _sscanf(terms.at(2).c_str(), "%lf", &a);
        if (terms.size() >= 4) _sscanf(terms.at(3).c_str(), "%lf", &s);
        return new adam_t(d1, d2, a, s);
    }

    return NULL;
}



}

}