/* @HEADER@ */
/* @HEADER@ */

#ifndef SUNDANCE_EVALMEDIATOR_H
#define SUNDANCE_EVALMEDIATOR_H

#ifndef DOXYGEN_DEVELOPER_ONLY

#include "SundanceDefs.hpp"
#include "SundanceLoadableVector.hpp"


namespace SundanceCore
{
  using namespace SundanceUtils;
  class CoordExpr;

  
  namespace Internal 
  {
    class MultiIndex; 
    class DiscreteFuncElement;
  }

  using namespace Internal;

  namespace FrameworkInterface
    {
      /**
       * Base class for evaluation mediator objects. 
       * Evaluation mediators are responsible
       * for evaluating those expressions whose
       * calculation must be delegated to the framework.
       */
      class EvalMediator
        {
        public:
          /** */
          EvalMediator();

          /** */
          virtual ~EvalMediator(){;}

          /** Return the size of the vectors used by this evaluator */
          int vecSize() const {return vecSize_;}

          /** Change the size of the vectors used by this evaluator */
          void setVecSize(int newSize) {vecSize_ = newSize;}

          /** Evaluate the given coordinate expression, putting
           * its numerical values in the given LoadableVector. */
          virtual void evalCoordExpr(const CoordExpr* expr,
                                     LoadableVector* const vec) const {;}

          /** Evaluate the given discrete function, putting
           * its numerical values in the given LoadableVector. */
          virtual void evalDiscreteFuncElement(const DiscreteFuncElement* expr,
                                               const MultiIndex& mi,
                                               LoadableVector* const vec) const 
          {;}

        private:
          int vecSize_;
        };
    }
}


#endif /* DOXYGEN_DEVELOPER_ONLY */
#endif
