/* @HEADER@ */
/* @HEADER@ */

#ifndef SUNDANCE_EVALUATORFACTORY_H
#define SUNDANCE_EVALUATORFACTORY_H

#include "SundanceDefs.hpp"

#ifndef DOXYGEN_DEVELOPER_ONLY

namespace SundanceCore
{
  using namespace SundanceUtils;
  namespace Internal
    {
      class Evaluator;
      class EvaluatableExpr;
      
      /**
       *
       */
      class EvaluatorFactory
        {
        public:
          /** */
          EvaluatorFactory();

          /** */
          virtual ~EvaluatorFactory(){;}

          /** */
          virtual Evaluator* 
          createEvaluator(const EvaluatableExpr* expr) const = 0 ;

        protected:
          /** Method to create those evaluators that can be built by
           * the base evaluator factory */
          Evaluator* commonCreate(const EvaluatableExpr* expr) const ;
        private:
        };
    }
}


#endif /* DOXYGEN_DEVELOPER_ONLY */
#endif
