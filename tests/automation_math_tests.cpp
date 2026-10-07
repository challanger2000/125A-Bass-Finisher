#include "TestSupport.h"
#include "AutomationMath.h"
#include <cmath>
#include <iostream>
#include <limits>

using HighGainGuitarFinisher::automation::linearValueAtSample;

namespace {
void near(double a,double b,double t=1.0e-12){ BF_REQUIRE(std::abs(a-b)<=t); }
}

int main(){
    near(linearValueAtSample(0,-1,0.0,3,1.0),0.25);
    near(linearValueAtSample(1,-1,0.0,3,1.0),0.50);
    near(linearValueAtSample(2,-1,0.0,3,1.0),0.75);
    near(linearValueAtSample(3,-1,0.0,3,1.0),1.00);
    near(linearValueAtSample(4,4,0.2,5,0.8),0.2);
    near(linearValueAtSample(5,4,0.2,5,0.8),0.8);
    near(linearValueAtSample(0,-1,std::numeric_limits<double>::quiet_NaN(),3,1.0),0.25);
    std::cout<<"Bass Finisher automation math passed\n";
    return 0;
}
