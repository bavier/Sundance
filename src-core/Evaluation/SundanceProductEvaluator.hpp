/* @HEADER@ */
// ************************************************************************
// 
//                             Sundance
//                 Copyright 2011 Sandia Corporation
// 
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Kevin Long (kevin.long@ttu.edu)
// 

/* @HEADER@ */


#ifndef SUNDANCE_PRODUCTEVALUATOR_H
#define SUNDANCE_PRODUCTEVALUATOR_H

#include "SundanceDefs.hpp"
#include "SundanceBinaryEvaluator.hpp"
#include "SundanceProductExpr.hpp"
#include "Teuchos_TimeMonitor.hpp"


namespace Sundance 
{
/**
 *
 */
class ProductEvaluator : public BinaryEvaluator<ProductExpr>
{
public:
  /** */
  ProductEvaluator(const ProductExpr* expr,
    const EvalContext& context);


  /** */
  virtual ~ProductEvaluator(){;}

  /** */
  virtual void internalEval(const EvalManager& mgr,
    Array<double>& constantResults,
    Array<RCP<EvalVector> >& vectorResults) const ;


  /** */
  TEUCHOS_TIMER(evalTimer, "product evaluation");

private:
  /** */
  enum ProductParity {VecVec, VecConst, ConstVec};
      
  int maxOrder_;
  Array<Array<int> > resultIndex_;
  Array<Array<int> > resultIsConstant_;

  Array<Array<int> > hasWorkspace_;
  Array<Array<int> > workspaceIsLeft_;
  Array<Array<int> > workspaceIndex_;
  Array<Array<int> > workspaceCoeffIndex_;
  Array<Array<int> > workspaceCoeffIsConstant_;

  Array<Array<Array<Array<int> > > > ccTerms_;
  Array<Array<Array<Array<int> > > > cvTerms_;
  Array<Array<Array<Array<int> > > > vcTerms_;
  Array<Array<Array<Array<int> > > > vvTerms_;

  Array<Array<Array<int> > > startingVectors_;
  Array<Array<ProductParity> > startingParities_;
      
      

      
};

}

#endif
