#include "Eigen/Eigen"
#include "cartesian_geom/cartesian_kernel.h"
#include "convex_bodies/hpolytope.h"

#include "diagnostics/diagnostics.hpp"
//todo make headers automomous
//#include "diagnostics/effective_sample_size.hpp"
//#include "diagnostics/print_diagnostics.hpp"

#include "generators/known_polytope_generators.h"

#include "random_walks/random_walks.hpp"

#include "sampling/sample_points.hpp"
#include "sampling/sampling.hpp"

#include "volume/volume_cooling_balls.hpp"

typedef Cartesian<double> Kernel;
typedef typename Kernel::Point Point;
typedef BoostRandomNumberGenerator<boost::mt19937, double> RNGType;
typedef HPolytope<Point> HPolytopeType;

using NT = double;
using MT = Eigen::Matrix<NT,Eigen::Dynamic,Eigen::Dynamic>;
using VT = Eigen::Matrix<NT,Eigen::Dynamic,1>;

template <typename Walk, typename Distribution>
void sample_points_eigen_matrix(HPolytopeType const& HP, Point const& q, Walk const& walk,
                                Distribution const& distr, RNGType rng, int walk_len, int rnum,
                                int nburns)
{
    MT samples(HP.dimension(), rnum);

    sample_points(HP, q, walk, distr, rng, walk_len, rnum, nburns, samples);

    // sample stats
    unsigned int min_ess;
    auto score = effective_sample_size<NT, VT, MT>(samples, min_ess);
    std::cout << "ess=" << min_ess << std::endl;
    //print_diagnostics<NT, VT, MT>(samples, min_ess, std::cerr);
}

struct CustomFunctor {

  // Custom density with neg log prob equal to c^T x
  template <
      typename NT,
      typename Point
  >
  struct parameters {
    unsigned int order;
    NT L; // Lipschitz constant for gradient
    NT m; // Strong convexity constant
    NT kappa; // Condition number
    Point x0;

    parameters(Point x0_) : order(2), L(1), m(1), kappa(1), x0(x0_) {};

  };

  template
  <
      typename Point
  >
  struct GradientFunctor {
    typedef typename Point::FT NT;
    typedef std::vector<Point> pts;

    parameters<NT, Point> &params;

    GradientFunctor(parameters<NT, Point> &params_) : params(params_) {};

    // The index i represents the state vector index
    Point operator() (unsigned int const& i, pts const& xs, NT const& t) const {
      if (i == params.order - 1) {
        Point y = (-1.0) * (xs[0] - params.x0);
        return y;
      } else {
        return xs[i + 1]; // returns derivative
      }
    }

  };

  template
  <
    typename Point
  >
  struct FunctionFunctor {
    typedef typename Point::FT NT;

    parameters<NT, Point> &params;

    FunctionFunctor(parameters<NT, Point> &params_) : params(params_) {};

    // The index i represents the state vector index
    NT operator() (Point const& x) const {
      Point y = x - params.x0;
      return 0.5 * y.dot(y);
    }

  };

};

int main() {
    // Generating a 3-dimensional cube centered at origin
    HPolytopeType HP = generate_cube<HPolytopeType>(10, false);
    std::cout<<"Polytope: \n";
    HP.ComputeInnerBall();
    //HP.print();
    //std::cout<<"\n";

    // Setup parameters for sampling
    Point q(HP.dimension());
    RNGType rng(HP.dimension());

    // NEW INTERFACE Sampling

    // Walks
    AcceleratedBilliardWalk abill_walk;
    AcceleratedBilliardWalk abill_walk_custom(10); //user defined walk parameters
    BallWalk ball_walk;
    BilliardWalk bill_walk;
    CDHRWalk cdhr_walk;
    DikinWalk dikin_walk;
    JohnWalk john_walk;
    RDHRWalk rdhr_walk;
    VaidyaWalk vaidya_walk;

    GaussianBallWalk gball_walk;
    GaussianCDHRWalk gcdhr_walk;
    GaussianRDHRWalk grdhr_walk;
    GaussianHamiltonianMonteCarloExactWalk ghmc_walk;

    GaussianAcceleratedBilliardWalk gbill_walk;

    ExponentialHamiltonianMonteCarloExactWalk ehmc_walk;

    HamiltonianMonteCarloWalk hmc_walk;
    NutsHamiltonianMonteCarloWalk nhmc_walk;

    // Distributions

    // 1. Uniform
    UniformDistribution udistr{};

    // 2. Spherical
    SphericalGaussianDistribution sgdistr{};

    MT A(2, 2);
    A << 0.25, 0.75,
         0.75, 3.25;
    Ellipsoid<Point> ell(A);    // origin centered ellipsoid
    GaussianDistribution gdistr(ell);

    // 3. Exponential
    NT variance = 1.0;
    auto c = GetDirection<Point>::apply(HP.dimension(), rng, false);
    ExponentialDistribution edistr(c, variance);

    // 4. LogConcave
    using NegativeGradientFunctor = CustomFunctor::GradientFunctor<Point>;
    using NegativeLogprobFunctor = CustomFunctor::FunctionFunctor<Point>;
    using Solver = LeapfrogODESolver<Point, NT, HPolytopeType, NegativeGradientFunctor>;

    std::pair<Point, NT> inner_ball = HP.ComputeInnerBall();
    Point x0 = inner_ball.first;

    CustomFunctor::parameters<NT, Point> params(x0);

    NegativeGradientFunctor g(params);
    NegativeLogprobFunctor f(params);
    LogConcaveDistribution logconcave(g, f, params.L);

/*
    NegativeGradientFunctor F(params);
    NegativeLogprobFunctor f(params);

    HamiltonianMonteCarloWalk::parameters<NT, NegativeGradientFunctor> hmc_params(F, HP.dimension());

    HamiltonianMonteCarloWalk hmc(F, f, hmc_params);

    int n_samples = 80000;
    int n_burns = 0;

    MT samples;
    samples.resize(dim, n_samples - n_burns);

    hmc.solver->eta0 = 0.5;

    for (int i = 0; i < n_samples; i++) {
      if (i % 1000 == 0) std::cerr << ".";
      hmc.apply(rng, 3);
      if (i >= n_burns) {
          samples.col(i - n_burns) = hmc.x.getCoefficients();
          std::cout << hmc.x.getCoefficients().transpose() << std::endl;
      }
    }
    std::cerr << std::endl;
*/
    // Sampling

    using NT = double;
    using MT = Eigen::Matrix<NT,Eigen::Dynamic,Eigen::Dynamic>;
    using VT = Eigen::Matrix<NT,Eigen::Dynamic,1>;

    int rnum = 20;
    int nburns = 5;
    int walk_len = 2;

    MT samples(HP.dimension(), rnum);

    // 1. the eigen matrix interface
    std::cout << "uniform" << std::endl;
    sample_points_eigen_matrix(HP, q, abill_walk, udistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, abill_walk_custom, udistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, ball_walk, udistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, cdhr_walk, udistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, dikin_walk, udistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, john_walk, udistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, rdhr_walk, udistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, vaidya_walk, udistr, rng, walk_len, rnum, nburns);

    std::cout << "shperical gaussian" << std::endl;
    sample_points_eigen_matrix(HP, q, gball_walk, sgdistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, gcdhr_walk, sgdistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, grdhr_walk, sgdistr, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, ghmc_walk, sgdistr, rng, walk_len, rnum, nburns);

    std::cout << "general gaussian" << std::endl;
    sample_points_eigen_matrix(HP, q, gbill_walk, gdistr, rng, walk_len, rnum, nburns);

    std::cout << "exponential" << std::endl;
    sample_points_eigen_matrix(HP, q, ehmc_walk, edistr, rng, walk_len, rnum, nburns);

    std::cout << "logconcave" << std::endl;
    sample_points_eigen_matrix(HP, q, hmc_walk, logconcave, rng, walk_len, rnum, nburns);
    sample_points_eigen_matrix(HP, q, nhmc_walk, logconcave, rng, walk_len, rnum, nburns);



    std::cout << "fix the following" << std::endl;
    // TODO: fix
    // Does not converge because of the starting point
    // Also ess returns rnum instead of 0
    sample_points_eigen_matrix(HP, q, bill_walk, udistr, rng, walk_len, rnum, nburns);

    // Does not compile because of walk-distribution combination
    //sample_points_eigen_matrix(HP, q, abill_walk, gdistr, rng, walk_len, rnum, nburns);

    std::cout << "std::vector interface" << std::endl;
    // 2. the std::vector interface
    std::vector<Point> points;
    sample_points(HP, q, cdhr_walk, udistr, rng, walk_len, rnum, nburns, points);
    for (auto& point : points)
    {
    //    std::cout << point.getCoefficients().transpose() << "\n";
    }

    // 3. the old interface
    // different billiard walks
    typedef BilliardWalk::template Walk<HPolytopeType, RNGType> BilliardWalkType;
    typedef AcceleratedBilliardWalk::template Walk<HPolytopeType, RNGType> AcceleratedBilliardWalkType;
    typedef RandomPointGenerator<AcceleratedBilliardWalkType> Generator;
    std::vector<Point> randPoints;
    PushBackWalkPolicy push_back_policy;
    Generator::apply(HP, q, rnum, walk_len, randPoints, push_back_policy, rng);
    for (auto& point : randPoints)
    {
    //    std::cout << point.getCoefficients().transpose() << "\n";
    }

/*
    unsigned int walkL = 10, numpoints = 10000, nburns = 0, d = HP.dimension();
    Point StartingPoint(d);
    std::list<Point> randPoints;

//    gaussian_sampling<AcceleratedBilliardWalk>(points, HP, rng, walkL, numpoints, 1.0,
    //                               StartingPoint, nburns);

    double variance = 1.0;

    Point c(HP.dimension());
    HP.set_InnerBall(std::pair<Point,double>(Point(HP.dimension()), 1.0));
    c = GetDirection<Point>::apply(HP.dimension(), rng, false);
    //ExponentialHamiltonianMonteCarloExactWalk
    exponential_sampling<NutsHamiltonianMonteCarloWalk>(points, HP, rng, walkL, numpoints, c, variance,
                                StartingPoint, nburns);

*/
    return 0;
}