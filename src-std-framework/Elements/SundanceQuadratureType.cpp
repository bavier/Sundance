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

#include "SundanceQuadratureType.hpp"
#include "SundanceExceptions.hpp"

using namespace SundanceStdFwk;
using namespace SundanceUtils;
using namespace SundanceStdFwk::Internal;
using namespace SundanceCore;
using namespace SundanceCore::Internal;
using namespace Teuchos;

int QuadratureType::findValidOrder(const CellType& cellType, int requestedOrder) const 
{
  TEST_FOR_EXCEPTION(hasLimitedOrder(cellType) 
                     && requestedOrder > maxOrder(cellType),
                     RuntimeError,
                     "order=" << requestedOrder << " not available on cell type "
                     << cellType);

  
  int big = 100;
  if (hasLimitedOrder(cellType)) big = maxOrder(cellType);

  for (int r=requestedOrder; r<=big; r++) 
    {
      if (supports(cellType, r)) return r; 
    }
  TEST_FOR_EXCEPTION(true, RuntimeError, "Could not find valid quadrature order "
                     "greater than " << requestedOrder << " for cell type "
                     << cellType);
  return -1; // -Wall
}
