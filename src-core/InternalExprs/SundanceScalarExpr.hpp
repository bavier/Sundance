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

#ifndef SUNDANCE_SCALAREXPR_H
#define SUNDANCE_SCALAREXPR_H


#include "SundanceDefs.hpp"
#include "SundanceExceptions.hpp"
#include "Teuchos_XMLObject.hpp"
#include "Teuchos_RefCountPtrDecl.hpp"
#include "SundanceExprBase.hpp"

#ifndef DOXYGEN_DEVELOPER_ONLY

namespace SundanceCore
{
  using namespace SundanceUtils;
  using namespace Teuchos;

  using std::string;
  using std::ostream;

  namespace Internal
    {
      /** */
      class ScalarExpr : public ExprBase
        {
        public:
          /** empty ctor */
          ScalarExpr();

          /** virtual destructor */
          virtual ~ScalarExpr() {;}


          /** Indicate whether this expression is constant in space */
          virtual bool isConstant() const {return false;}


          /** Indicate whether this expression is an immutable constant */
          virtual bool isImmutable() const {return false;}

          /** Indicate whether this expression is a "hungry"
           * differential operator that is awaiting an argument. */
          virtual bool isHungryDiffOp() const {return false;}

          /** Ordering operator for use in transforming exprs to standard form */
          virtual bool lessThan(const ScalarExpr* other) const = 0 ;

        protected:
        };
    }
}

#endif /* DOXYGEN_DEVELOPER_ONLY */
#endif
