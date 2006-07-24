#include "SundanceExpr.hpp"
#include "SundanceStdMathOps.hpp"
#include "SundanceDerivative.hpp"
#include "SundanceUnknownFunctionStub.hpp"
#include "SundanceTestFunctionStub.hpp"
#include "SundanceDiscreteFunctionStub.hpp"
#include "SundanceUnknownFuncElement.hpp"
#include "SundanceDiscreteFuncElement.hpp"
#include "SundanceCoordExpr.hpp"
#include "SundanceZeroExpr.hpp"
#include "SundanceSymbolicTransformation.hpp"
#include "SundanceProductTransformation.hpp"
#include "SundanceDeriv.hpp"
#include "SundanceParameter.hpp"
#include "SundanceOut.hpp"
#include "Teuchos_Time.hpp"
#include "Teuchos_MPISession.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include "SundanceDerivSet.hpp"
#include "SundanceRegionQuadCombo.hpp"
#include "SundanceEvalManager.hpp"
#include "SundanceEvalVector.hpp"
#include "SundanceSymbPreprocessor.hpp"
#include "SundanceStringEvalMediator.hpp"

using namespace SundanceUtils;
using SundanceCore::List;
using namespace SundanceCore::Internal;
using namespace Teuchos;
using namespace TSFExtended;

namespace SundanceCore
{
  /** */
  Expr slopeBarrier2D(const double& p, const Expr& x) ;

  /**
   *
   */
  class SlopeBarrierFunctor : public UnaryFunctor
  {
  public:
    /** */
    SlopeBarrierFunctor(double p);

    /** */
    virtual void eval0(const double* const x, 
                       int nx, 
                       double* f) const  ;
    
    /** */
    virtual void eval1(const double* const x, 
                       int nx, 
                       double* f, 
                       double* df_dx) const ;

    
  private:
    double p_;
  };

}

namespace SundanceCore
{
  Expr slopeBarrier2D(const double& p, const Expr& x)
  {
    RefCountPtr<ScalarExpr> arg = rcp_dynamic_cast<ScalarExpr>(x[0].ptr());
    RefCountPtr<UnaryFunctor> f = rcp(new SlopeBarrierFunctor(p));
    return new NonlinearUnaryOp(arg, f);
  }
}



SlopeBarrierFunctor::SlopeBarrierFunctor(double p)
  : UnaryFunctor("SlopeBarrier(" + Teuchos::toString(p) + ")"),
    p_(p)
{;}


void SlopeBarrierFunctor::eval0(const double* const x, 
                                int nx, 
                                double* f) const 
{
  double xp;

  for (int i=0; i<nx; i++)
    {
      if (x[i] <= 1.0)
        {
          xp = ::pow(x[i], p_);
          f[i] = xp + 1.0/xp - 2.0;
        }
      else
        {
          f[i] = 0.0;
        }
    }
}


void SlopeBarrierFunctor::eval1(const double* const x, 
                                int nx, 
                                double* f, 
                                double* df_dx) const 
{
  double xp;

  for (int i=0; i<nx; i++)
    {
      if (x[i] <= 1.0)
        {
          xp = ::pow(x[i], p_);
          f[i] = xp + 1.0/xp - 2.0;
          df_dx[i] = p_*(xp - 1.0/xp)/x[i];
        }
      else
        {
          f[i] = 0.0;
          df_dx[i] = 0.0;
        }
    }
}



static Time& totalTimer() 
{
  static RefCountPtr<Time> rtn 
    = TimeMonitor::getNewTimer("total"); 
  return *rtn;
}

static Time& doitTimer() 
{
  static RefCountPtr<Time> rtn 
    = TimeMonitor::getNewTimer("doit"); 
  return *rtn;
}



void doVariations(const Expr& e, 
                  const Expr& vars,
                  const Expr& varEvalPt,
                  const Expr& unks,
                  const Expr& unkEvalPt, 
                  const Expr& unkParams,
                  const Expr& unkParamEvalPts,
                  const Expr& fixed,
                  const Expr& fixedEvalPt, 
                  const Expr& fixedParams,
                  const Expr& fixedParamEvalPts, 
                  const EvalContext& region)
{
  TimeMonitor t0(doitTimer());
  EvalManager mgr;
  mgr.setRegion(region);

  static RefCountPtr<AbstractEvalMediator> mediator 
    = rcp(new StringEvalMediator());

  mgr.setMediator(mediator);

  const EvaluatableExpr* ev 
    = dynamic_cast<const EvaluatableExpr*>(e[0].ptr().get());

  DerivSet d = SymbPreprocessor::setupVariations(e[0], 
                                                 vars,
                                                 varEvalPt,
                                                 unks,
                                                 unkEvalPt,
                                                 unkParams,
                                                 unkParamEvalPts,
                                                 fixed,
                                                 fixedEvalPt,
                                                 fixedParams,
                                                 fixedParamEvalPts,
                                                 region);

  Tabs tab;
  //  cerr << tab << *ev->sparsitySuperset(region) << endl;
  //  ev->showSparsity(cerr, region);

  // RefCountPtr<EvalVectorArray> results;

  Array<double> constantResults;
  Array<RefCountPtr<EvalVector> > vectorResults;

  ev->evaluate(mgr, constantResults, vectorResults);

  ev->sparsitySuperset(region)->print(cerr, vectorResults, constantResults);

  
  // results->print(cerr, ev->sparsitySuperset(region).get());
}

void doGradient(const Expr& e, 
                const Expr& vars,
                const Expr& varEvalPt,
                const Expr& fixedParams,
                const Expr& fixedParamEvalPts,
                const Expr& fixed,
                const Expr& fixedEvalPts, 
                const EvalContext& region)
{
  TimeMonitor t0(doitTimer());
  EvalManager mgr;
  mgr.setRegion(region);

  static RefCountPtr<AbstractEvalMediator> mediator 
    = rcp(new StringEvalMediator());

  mgr.setMediator(mediator);

  const EvaluatableExpr* ev 
    = dynamic_cast<const EvaluatableExpr*>(e[0].ptr().get());

  DerivSet d = SymbPreprocessor::setupGradient(e[0], 
                                               vars,
                                               varEvalPt,
                                               fixedParams,
                                               fixedParamEvalPts,
                                               fixed,
                                               fixedEvalPts,
                                               region);

  Tabs tab;
  //  cerr << tab << *ev->sparsitySuperset(region) << endl;
  //  ev->showSparsity(cerr, region);

  // RefCountPtr<EvalVectorArray> results;

  Array<double> constantResults;
  Array<RefCountPtr<EvalVector> > vectorResults;

  ev->evaluate(mgr, constantResults, vectorResults);

  ev->sparsitySuperset(region)->print(cerr, vectorResults, constantResults);

  
  // results->print(cerr, ev->sparsitySuperset(region).get());
}



void doFunctional(const Expr& e, 
                  const Expr& fixedParams,
                  const Expr& fixedParamEvalPts,
                  const Expr& fixed,
                  const Expr& fixedEvalPt, 
                  const EvalContext& region)
{
  TimeMonitor t0(doitTimer());
  EvalManager mgr;
  mgr.setRegion(region);

  static RefCountPtr<AbstractEvalMediator> mediator 
    = rcp(new StringEvalMediator());

  mgr.setMediator(mediator);

  const EvaluatableExpr* ev 
    = dynamic_cast<const EvaluatableExpr*>(e[0].ptr().get());

  DerivSet d = SymbPreprocessor::setupFunctional(e[0], 
                                                 fixedParams,
                                                 fixedParamEvalPts,
                                                 fixed,
                                                 fixedEvalPt,
                                                 region);

  Tabs tab;
  //  cerr << tab << *ev->sparsitySuperset(region) << endl;
  //  ev->showSparsity(cerr, region);

  // RefCountPtr<EvalVectorArray> results;

  Array<double> constantResults;
  Array<RefCountPtr<EvalVector> > vectorResults;

  ev->evaluate(mgr, constantResults, vectorResults);

  ev->sparsitySuperset(region)->print(cerr, vectorResults, constantResults);

  
  // results->print(cerr, ev->sparsitySuperset(region).get());
}



void testVariations(const Expr& e,  
                    const Expr& vars,
                    const Expr& varEvalPt,
                    const Expr& unks,
                    const Expr& unkEvalPt, 
                    const Expr& unkParams,
                    const Expr& unkParamEvalPts,
                    const Expr& fixed,
                    const Expr& fixedEvalPt, 
                    const Expr& fixedParams,
                    const Expr& fixedParamEvalPts,  
                    const EvalContext& region)
{
  cerr << endl 
       << "------------------------------------------------------------- " << endl;
  cerr  << "-------- testing " << e.toString() << " -------- " << endl;
  cerr << endl 
       << "------------------------------------------------------------- " << endl;

  try
    {
      doVariations(e, vars, varEvalPt, 
                   unks, unkEvalPt, 
                   unkParams,
                   unkParamEvalPts,
                   fixed, fixedEvalPt, 
                   fixedParams,
                   fixedParamEvalPts,
                   region);
    }
  catch(exception& ex)
    {
      cerr << "EXCEPTION DETECTED!" << endl;
      cerr << ex.what() << endl;
      // cerr << "repeating with increased verbosity..." << endl;
      //       cerr << "-------- testing " << e.toString() << " -------- " << endl;
      //       Evaluator::verbosity() = 2;
      //       EvalVector::verbosity() = 2;
      //       EvaluatableExpr::verbosity() = 2;
      //       Expr::showAllParens() = true;
      //       doit(e, region);
      exit(1);
    }
}

void testGradient(const Expr& e,  
                  const Expr& vars,
                  const Expr& varEvalPt,
                  const Expr& fixedParams,
                  const Expr& fixedParamEvalPts,
                  const Expr& fixed,
                  const Expr& fixedEvalPt,  
                  const EvalContext& region)
{
  cerr << endl 
       << "------------------------------------------------------------- " << endl;
  cerr  << "-------- testing " << e.toString() << " -------- " << endl;
  cerr << endl 
       << "------------------------------------------------------------- " << endl;

  try
    {
      doGradient(e, vars, varEvalPt,  
                 fixedParams,
                 fixedParamEvalPts,
                 fixed, 
                 fixedEvalPt, 
                 region);
    }
  catch(exception& ex)
    {
      cerr << "EXCEPTION DETECTED!" << endl;
      cerr << ex.what() << endl;
      // cerr << "repeating with increased verbosity..." << endl;
      //       cerr << "-------- testing " << e.toString() << " -------- " << endl;
      //       Evaluator::verbosity() = 2;
      //       EvalVector::verbosity() = 2;
      //       EvaluatableExpr::verbosity() = 2;
      //       Expr::showAllParens() = true;
      //       doit(e, region);
      exit(1);
    }
}


void testFunctional(const Expr& e, 
                    const Expr& fixedParams,
                    const Expr& fixedParamEvalPts, 
                    const Expr& fixed,
                    const Expr& fixedEvalPt,  
                    const EvalContext& region)
{
  cerr << endl 
       << "------------------------------------------------------------- " << endl;
  cerr  << "-------- testing " << e.toString() << " -------- " << endl;
  cerr << endl 
       << "------------------------------------------------------------- " << endl;

  try
    {
      doFunctional(e,   
                   fixedParams,
                   fixedParamEvalPts,fixed, fixedEvalPt, region);
    }
  catch(exception& ex)
    {
      cerr << "EXCEPTION DETECTED!" << endl;
      cerr << ex.what() << endl;
      // cerr << "repeating with increased verbosity..." << endl;
      //       cerr << "-------- testing " << e.toString() << " -------- " << endl;
      //       Evaluator::verbosity() = 2;
      //       EvalVector::verbosity() = 2;
      //       EvaluatableExpr::verbosity() = 2;
      //       Expr::showAllParens() = true;
      //       doit(e, region);
      exit(1);
    }
}


int main(int argc, void** argv)
{
  
  try
		{
      MPISession::init(&argc, &argv);

      TimeMonitor t(totalTimer());

      int maxDiffOrder = 2;

      verbosity<SymbolicTransformation>() = VerbSilent;
      verbosity<Evaluator>() = VerbSilent;
      verbosity<EvalVector>() = VerbSilent;
      verbosity<EvaluatableExpr>() = VerbSilent;
      Expr::showAllParens() = true;
      ProductTransformation::optimizeFunctionDiffOps() = false;

      EvalVector::shadowOps() = true;

      Expr dx = new Derivative(0);
      Expr dy = new Derivative(1);
      Expr grad = List(dx, dy);

			Expr u = new UnknownFunctionStub("u");
			Expr lambda_u = new UnknownFunctionStub("lambda_u");
			Expr T = new UnknownFunctionStub("T");
			Expr lambda_T = new UnknownFunctionStub("lambda_T");
			Expr alpha = new UnknownFunctionStub("alpha");

			Expr unkParams;
			Expr unkParamValues;

			Expr fixedParams;
			Expr fixedParamValues;

      Expr x = new CoordExpr(0);
      Expr y = new CoordExpr(1);

      Expr u0 = new DiscreteFunctionStub("u0");
      Expr lambda_u0 = new DiscreteFunctionStub("lambda_u0");
      Expr T0 = new DiscreteFunctionStub("T0");
      Expr lambda_T0 = new DiscreteFunctionStub("lambda_T0");
      Expr zero = new ZeroExpr();
      Expr alpha0 = new DiscreteFunctionStub("alpha0");

      Array<Expr> tests;
      //Expr h = new Parameter(0.1);
      double h = 0.1;
      Expr rho = 0.5*(1.0 + tanh(alpha/h));

      //      Expr rho = tanh(alpha);
      Expr sigma =  0.1 + 0.9*rho;

      Expr zeta = slopeBarrier2D(1.5, (grad*alpha)*(grad*alpha) + alpha*alpha/h/h);
        

      //#define BLAHBLAH 1
#ifdef BLAHBLAH
      verbosity<Evaluator>() = VerbExtreme;
      verbosity<SparsitySuperset>() = VerbExtreme;
      verbosity<EvaluatableExpr>() = VerbExtreme;
#endif

      Expr q = u - x;
      tests.append( pow(T-3.14, 2.0)
                    + pow(u-2.72, 2.0)
                    + 0.5*T*T
                    + 0.5*(grad*u)*(grad*u)
                    + 0.5*q*q + zeta + 0.5*(grad*alpha)*(grad*alpha)
                    + sqrt(1.0e-16 + (grad*rho)*(grad*rho)) 
                    +  -sigma*(grad*lambda_u)*(grad*u)
                    + -sigma*sigma*lambda_T
                    + sigma*(grad*lambda_T)*(grad*T)
                    + lambda_T*sigma*(grad*u)*(grad*T)
                    + lambda_u - lambda_T);

      //      tests.append(sqrt(dx*tanh(alpha)));
      //      tests.append(sqrt(1+(dx*rho)*(dx*rho))  + rho*lambda_u );
      //      tests.append(pow(dx*(u0 - x*x ), 2.0));
      //      tests.append(-lambda_u);

#ifdef BLARF
      const EvaluatableExpr* ee 
        = dynamic_cast<const EvaluatableExpr*>(tests[0].ptr().get());
      RegionQuadCombo rr(rcp(new CellFilterStub()), 
                         rcp(new QuadratureFamilyStub(1)));
      EvalContext cc(rr, maxDiffOrder, EvalContext::nextID());
      Set<MultiIndex> miSet = makeSet<MultiIndex>(MultiIndex());

      Set<MultiSet<int> > funcs 
        = makeSet<MultiSet<int> >(makeMultiSet<int>(1),
                                  makeMultiSet<int>(1,0));


      
      const UnknownFuncElement* uPtr
        = dynamic_cast<const UnknownFuncElement*>(u[0].ptr().get());
      const UnknownFuncElement* lPtr
        = dynamic_cast<const UnknownFuncElement*>(lambda_u[0].ptr().get());
      const UnknownFuncElement* aPtr
        = dynamic_cast<const UnknownFuncElement*>(alpha[0].ptr().get());

      RefCountPtr<DiscreteFuncElement> u0Ptr
        = rcp_dynamic_cast<DiscreteFuncElement>(u0[0].ptr());
      RefCountPtr<DiscreteFuncElement> l0Ptr
        = rcp_dynamic_cast<DiscreteFuncElement>(lambda_u0[0].ptr());
      RefCountPtr<DiscreteFuncElement> a0Ptr
        = rcp_dynamic_cast<DiscreteFuncElement>(alpha0[0].ptr());
      uPtr->substituteFunction(u0Ptr);
      lPtr->substituteFunction(l0Ptr);
      aPtr->substituteFunction(a0Ptr);
      
      ee->findNonzeros(cc, miSet, funcs, false);

#endif
      //#ifdef BLARF
      cerr << endl << "============== u STATE EQUATIONS =================" << endl;

      for (int i=0; i<tests.length(); i++)
        {
          RegionQuadCombo rqc(rcp(new CellFilterStub()), 
                              rcp(new QuadratureFamilyStub(1)));
          EvalContext context(rqc, maxDiffOrder, EvalContext::nextID());
          testVariations(tests[i], 
                         List(lambda_u),
                         List(zero),
                         List(u),
                         List(u0),
                         unkParams,
                         unkParamValues,
                         List(alpha, T, lambda_T),
                         List(alpha0, T0, zero),
                         fixedParams,
                         fixedParamValues,
                         context);
        }

      cerr << endl << "============== T STATE EQUATIONS =================" << endl;

      for (int i=0; i<tests.length(); i++)
        {
          RegionQuadCombo rqc(rcp(new CellFilterStub()), 
                              rcp(new QuadratureFamilyStub(1)));
          EvalContext context(rqc, maxDiffOrder, EvalContext::nextID());
          testVariations(tests[i], 
                         List(lambda_T),
                         List(zero),
                         List(T),
                         List(T0),
                         unkParams,
                         unkParamValues,
                         List(alpha, u, lambda_u),
                         List(alpha0, u0, zero),
                         fixedParams,
                         fixedParamValues,
                         context);
        }



      cerr << endl << "=============== T ADJOINT EQUATIONS =================" << endl;
      for (int i=0; i<tests.length(); i++)
        {
          RegionQuadCombo rqc(rcp(new CellFilterStub()), 
                              rcp(new QuadratureFamilyStub(1)));
          EvalContext context(rqc, maxDiffOrder, EvalContext::nextID());
          testVariations(tests[i], 
                         List(T),
                         List(T0),
                         List(lambda_T),
                         List(lambda_T0),
                         unkParams,
                         unkParamValues,
                         List(alpha, u, lambda_u),
                         List(alpha0, u0, zero),
                         fixedParams,
                         fixedParamValues,
                         context);
        }

      cerr << endl << "=============== u ADJOINT EQUATIONS =================" << endl;
      for (int i=0; i<tests.length(); i++)
        {
          RegionQuadCombo rqc(rcp(new CellFilterStub()), 
                              rcp(new QuadratureFamilyStub(1)));
          EvalContext context(rqc, maxDiffOrder, EvalContext::nextID());
          testVariations(tests[i], 
                         List(u),
                         List(u0),
                         List(lambda_u),
                         List(lambda_u0),
                         unkParams,
                         unkParamValues,
                         List(alpha, T, lambda_T),
                         List(alpha0, T0, zero),
                         fixedParams,
                         fixedParamValues,
                         context);
        }


      cerr << endl << "================ REDUCED GRADIENT ====================" << endl;
      for (int i=0; i<tests.length(); i++)
        {
          RegionQuadCombo rqc(rcp(new CellFilterStub()), 
                              rcp(new QuadratureFamilyStub(1)));
          EvalContext context(rqc, 1, EvalContext::nextID());
          testGradient(tests[i], 
                       alpha, 
                       alpha0,
                       fixedParams,
                       fixedParamValues,
                       List(u, T, lambda_u, lambda_T),
                       List(u0, T0, lambda_u0, lambda_T0),
                       context);
        }

      cerr << endl << "=================== FUNCTIONAL ====================" << endl;
      for (int i=0; i<tests.length(); i++)
        {
          RegionQuadCombo rqc(rcp(new CellFilterStub()), 
                              rcp(new QuadratureFamilyStub(1)));
          EvalContext context(rqc, 0, EvalContext::nextID());
          testFunctional(tests[i], 
                         fixedParams,
                         fixedParamValues,
                         List(u, T, lambda_u, lambda_T, alpha),
                         List(u0, T0, zero, zero, alpha0),
                         context);
        }
      //#endif

    }
	catch(exception& e)
		{
			Out::println(e.what());
		}
  TimeMonitor::summarize();

  MPISession::finalize();
}
