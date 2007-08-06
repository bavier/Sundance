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

#include "SundanceSubtypeEvaluator.hpp"
#include "SundanceEvalManager.hpp"
#include "SundanceCellVectorExpr.hpp"
#include "SundanceExceptions.hpp"
#include "SundanceSet.hpp"
#include "SundanceTabs.hpp"
#include "SundanceOut.hpp"

using namespace SundanceCore;
using namespace SundanceUtils;

using namespace SundanceCore::Internal;
using namespace Teuchos;
using namespace TSFExtended;

CellVectorEvaluator::CellVectorEvaluator(const CellVectorExpr* expr, 
                                       const EvalContext& context)
  : SubtypeEvaluator<CellVectorExpr>(expr, context), 
    dim_(expr->dimension()),
    isTangentVector_(expr->isTangent()),
    basisVectorIndex_(expr->basisMemberIndex()),
    componentIndex_(expr->componentIndex()),
    stringRep_(expr->toString())
{

  Tabs tabs;
  SUNDANCE_VERB_LOW(tabs << "initializing cell vector evaluator for " 
                    << expr->toString());
  SUNDANCE_VERB_MEDIUM(tabs << "return sparsity " 
		       << endl << *(this->sparsity)());

  TEST_FOR_EXCEPTION(this->sparsity()->numDerivs() >1 , InternalError,
                     "CellVectorEvaluator ctor found a sparsity table "
                     "with more than one entry. The bad sparsity table is "
                     << *(this->sparsity)());

  /* 
   * There is only one possible entries in the nozeros table for a
   * cell vector expression: a zeroth derivative.
   */
  
  for (int i=0; i<this->sparsity()->numDerivs(); i++)
    {
      const MultipleDeriv& d = this->sparsity()->deriv(i);

      /* for a zeroth-order derivative, evaluate the coord expr */
      TEST_FOR_EXCEPTION(d.order() != 0, RuntimeError, 
			 "Derivative " << d << " is not valid for "
			 "CellVectorEvaluator");
      addVectorIndex(i, 0);
    }
  
}



void CellVectorEvaluator::internalEval(const EvalManager& mgr,
                                      Array<double>& constantResults,
                                      Array<RefCountPtr<EvalVector> >& vectorResults) const 
{
  //  TimeMonitor timer(coordEvalTimer());
  Tabs tabs;

  SUNDANCE_VERB_LOW(tabs << "CellVectorEvaluator::eval() expr=" << expr()->toString());

  if (verbosity() > VerbMedium)
    {
      cerr << tabs << "sparsity = " << endl << *(this->sparsity)() << endl;
    }

  vectorResults.resize(1);
  vectorResults[0] = mgr.popVector();
  mgr.evalCellVectorExpr(expr(), vectorResults[0]);
  mgr.stack().setVecSize(vectorResults[0]->length());
  vectorResults[0]->setString(stringRep_);

  if (verbosity() > VerbMedium)
    {
      Tabs tab1;
      cerr << tab1 << "results " << endl;
      this->sparsity()->print(cerr, vectorResults,
                            constantResults);
    }

}

