#pragma once

#include <string>

#include "../phillip.h"


namespace phil
{

namespace test
{


/// An virtual class of testing.
class virtual_test_t
{
public:
    bool operator()(phillip_main_t *main) const;

protected:
    virtual void test(phillip_main_t *main) const = 0;
    virtual std::string disp() const = 0;
};


class compiling_axioms_t : public virtual_test_t
{
protected:
    virtual void test(phil::phillip_main_t *main) const;
    virtual std::string disp() const;
};


}

}