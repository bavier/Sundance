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

#ifndef SUNDANCE_QUADRATUREEVALMEDIATOR_H
#define SUNDANCE_QUADRATUREEVALMEDIATOR_H

#include "SundanceDefs.hpp"
#include "SundanceMap.hpp"
#include "SundanceStdFwkEvalMediator.hpp"
#include "SundanceQuadratureFamily.hpp"
#include "SundanceBasisFamily.hpp"
#include "SundanceOrderedTuple.hpp"

#ifndef DOXYGEN_DEVELOPER_ONLY

namespace SundanceStdFwk
{
  using namespace SundanceUtils;
  using namespace SundanceStdMesh;
  using namespace SundanceStdMesh::Internal;
  using namespace SundanceCore;
  using namespace SundanceCore;

  namespace Internal
  {
    using namespace Teuchos;

    /**
     * 
     */
    class QuadratureEvalMediator : public StdFwkEvalMediator
    {
    public:
      /** 
       * 
       */
      QuadratureEvalMediator(const Mesh& mesh, 
        int cellDim,
        const QuadratureFamily& quad);

      /** */
      virtual ~QuadratureEvalMediator(){;}

      
      /** Evaluate the given coordinate expression, putting
       * its numerical values in the given EvalVector. */
      virtual void evalCoordExpr(const CoordExpr* expr,
                                 RefCountPtr<EvalVector>& vec) const ;
      
      /** Evaluate the given discrete function, putting
       * its numerical values in the given EvalVector. */
      virtual void evalDiscreteFuncElement(const DiscreteFuncElement* expr,
                                           const Array<MultiIndex>& mi,
                                           Array<RefCountPtr<EvalVector> >& vec) const ;

      /** Evaluate the given cell diameter expression, putting
       * its numerical values in the given EvalVector. */
      virtual void evalCellDiameterExpr(const CellDiameterExpr* expr,
                                        RefCountPtr<EvalVector>& vec) const ;


      /** Evaluate the given cell vector expression, putting
       * its numerical values in the given EvalVector. */
      virtual void evalCellVectorExpr(const CellVectorExpr* expr,
				      RefCountPtr<EvalVector>& vec) const ;
            
      /** */
      virtual void setCellType(const CellType& cellType,
        const CellType& maxCellType,
        bool isInternalBdry) ;

      /** */
      virtual void print(ostream& os) const ;

      /** */
      Array<Array<double> >* getRefBasisVals(const BasisFamily& basis, 
                                                   int diffOrder) const ;

      /** */
      RefCountPtr<Array<Array<Array<double> > > > getFacetRefBasisVals(const BasisFamily& basis, int diffOrder) const ;

      /** */
      int numQuadPts(const CellType& cellType) const ;

      static double& totalFlops() {static double rtn = 0; return rtn;}

      

      static void addFlops(const double& flops) {totalFlops() += flops;}

      /**
       * Return the number of different cases for which reference
       * basis functions must be evaluated. If we're on maximal cells,
       * this will be one. If we're on lower-dimensional cells, this will
       * be equal to the number of cellDim-dimensional facets of the maximal
       * cells.
       */
      int numEvaluationCases() const {return numEvaluationCases_;}

    private:


      /** */
      void fillFunctionCache(const DiscreteFunctionData* f,
                             const MultiIndex& mi) const ;

     
      /** */
      void computePhysQuadPts() const ;

      /** */
      int numEvaluationCases_;

      /** */
      QuadratureFamily quad_;

      /** */
      Map<CellType, int> numQuadPtsForCellType_;

      /** quadPtsForReferenceCell_ stores quadrature points using the 
       * cell's coordinate system, i.e., points on a line are
       * stored in 1D coordinates regardless of the dimension of the 
       * maximal cell. This tabulation of quad pts is used in push
       * forward operations because it saves some pointer chasing 
       * within the mesh's pushForward() function.  */
      Map<CellType, RefCountPtr<Array<Point> > > quadPtsForReferenceCell_;

      /** quadPtsReferredToMaxCell_ stores quadrature points using the maximal
       * cell's coordinate system, i.e., points on a line are stored in 2D
       * coordinates if the line is a facet of a triangle, and so on. These
       * quadrature points are used in basis function evaluations. */
      Map<CellType, RefCountPtr<Array<Array<Point> > > > quadPtsReferredToMaxCell_;

      /** */
      mutable Array<Point> physQuadPts_;

      /** */
      mutable Array<Map<OrderedPair<BasisFamily, CellType>, RefCountPtr<Array<Array<Array<double> > > > > > refFacetBasisVals_;
      
    };
  }
}

#endif  /* DOXYGEN_DEVELOPER_ONLY */

#endif
