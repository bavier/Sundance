/* @HEADER@ */
// ************************************************************************
// 
//                              Sundance
//                 Copyright (2005) Sandia Corporation
// 
// Copyright (year first published) Sandia Corporation.  Under the terms 
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government 
// retains certain rights in this software.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//                                                                                 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA                                                                                
// Questions? Contact Kevin Long (krlong@sandia.gov), 
// Sandia National Laboratories, Livermore, California, USA
// 
// ************************************************************************
/* @HEADER@ */

#ifndef SUNDANCE_SUBTYPEEVALUATOR_H
#define SUNDANCE_SUBTYPEEVALUATOR_H

#include "SundanceDefs.hpp"
#include "SundanceEvaluator.hpp"
#include "SundanceEvaluatableExpr.hpp"

namespace SundanceCore 
{
class EvalContext;


/**
 * 
 */
template <class ExprType> class SubtypeEvaluator : public Evaluator
{
public:
  /** */
  SubtypeEvaluator(const ExprType* expr,
    const EvalContext& context)
    : Evaluator(), 
      expr_(expr), 
      sparsity_(expr_->sparsitySuperset(context))
    {
      this->setVerbosity(Evaluator::classVerbosity());
    }

  /** */
  virtual ~SubtypeEvaluator(){;}


      
  /** */
  const RefCountPtr<SparsitySuperset>& sparsity() const {return sparsity_;}

protected:
  /** */
  const ExprType* expr() const {return expr_;}

  /** */
  const MultipleDeriv& vectorResultDeriv(int iVec) const 
    {
      return this->sparsity()->deriv(vectorIndices()[iVec]);
    }

  /** */
  const MultipleDeriv& constantResultDeriv(int iConst) const 
    {
      return this->sparsity()->deriv(constantIndices()[iConst]);
    }

private:
  const ExprType* expr_;

  RefCountPtr<SparsitySuperset> sparsity_;
};
   
}




#endif
