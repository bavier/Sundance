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

#include "SundanceDerivOfSymbFunc.hpp"
#include "SundanceFunctionalDeriv.hpp"
#include "SundanceTestFuncElement.hpp"
#include "SundanceCoordDeriv.hpp"
#include "SundanceCoordExpr.hpp"
#include "SundanceTabs.hpp"
#include "SundanceOut.hpp"

using namespace SundanceCore;
using namespace SundanceUtils;

using namespace SundanceCore::Internal;
using namespace Teuchos;
using namespace TSFExtended;

DerivOfSymbFunc::DerivOfSymbFunc(const MultiIndex& op, const RefCountPtr<ScalarExpr>& arg)
  : DiffOp(op, arg), funcID_(-1)
{
  const SymbolicFuncElement* f 
    = dynamic_cast<const SymbolicFuncElement*>(evaluatableArg());
  TEST_FOR_EXCEPTION(f==0, InternalError, "argument to DerivOfSymbFunc ctor "
                     "is not a symbolic function");
  funcID_ = f->funcID();
}


FunctionalDeriv* DerivOfSymbFunc::representMeAsFunctionalDeriv() const 
{
  const FuncElementBase* f 
    = dynamic_cast<const FuncElementBase*>(evaluatableArg());
  TEST_FOR_EXCEPTION(f==0, InternalError, "DerivOfSymbFunc::"
                     "representMeAsFunctionalDeriv(), 'this' pointer "
                     "is not a symbolic function");
  return new FunctionalDeriv(f, mi());
}

Evaluator* DerivOfSymbFunc::createEvaluator(const EvaluatableExpr* expr,
                                   const EvalContext& context) const
{
  return new DerivOfSymbFuncEvaluator(dynamic_cast<const DerivOfSymbFunc*>(expr), context);
}

