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

#ifndef SUNDANCE_CELLDIAMETEREXPR_H
#define SUNDANCE_CELLDIAMETEREXPR_H

#include "SundanceLeafExpr.hpp"
#include "SundanceEvaluatorFactory.hpp"
#include "SundanceCellDiameterEvaluator.hpp"

namespace SundanceCore
{
  using namespace SundanceUtils;
  using namespace SundanceCore::Internal;

  /**
   * Expression that returns a characteristic size for each cell on 
   * which it is evaluated. 
   */
  class CellDiameterExpr 
    : public GenericEvaluatorFactory<CellDiameterExpr, CellDiameterExprEvaluator>,
      virtual public LeafExpr
  {
  public:
    /** */
    CellDiameterExpr(const string& name="h");
    
    /** */
    virtual ~CellDiameterExpr() {;}

    /** */
    virtual XMLObject toXML() const ;

    const string& name() const {return name_;}

    /** Write a simple text description suitable 
     * for output to a terminal */
    virtual ostream& toText(ostream& os, bool paren) const ;
    
    /** Write in a form suitable for LaTeX formatting */
    virtual ostream& toLatex(ostream& os, bool paren) const ;

    /*
     * Determine which functional and spatial derivatives are nonzero in the
     * given context. We also keep track of which functional derivatives
     * are known to be constant, which can simplify evaluation. 
     */
    virtual void findNonzeros(const EvalContext& context,
                              const Set<MultiIndex>& multiIndices,
                              const Set<MultiSet<int> >& activeFuncIDs,
                              bool regardFuncsAsConstant) const ;

    /** */
    virtual RefCountPtr<ExprBase> getRcp() {return rcp(this);}
  private:
    string name_;
  };
}

#endif