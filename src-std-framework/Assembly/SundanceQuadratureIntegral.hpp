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

#ifndef SUNDANCE_QUADRATUREINTEGRAL_H
#define SUNDANCE_QUADRATUREINTEGRAL_H

#include "SundanceDefs.hpp"
#include "SundanceElementIntegral.hpp"

#ifndef DOXYGEN_DEVELOPER_ONLY

namespace SundanceStdFwk
{
  using namespace SundanceUtils;
  using namespace SundanceStdMesh;
  using namespace SundanceStdMesh::Internal;
  using namespace SundanceCore;
  using namespace SundanceCore::Internal;

  namespace Internal
  {
    using namespace Teuchos;

    /** 
     *  
     *
     */
    class QuadratureIntegral 
      : public ElementIntegral
    {
    public:
      /** Construct a zero-form to be computed by quadrature */
      QuadratureIntegral(int spatialDim,
                         int dim, 
                         const CellType& cellType,
                         const QuadratureFamily& quad);
      /** Construct a one form to be computed by quadrature */
      QuadratureIntegral(int spatialDim,
                         int dim, 
                         const CellType& cellType,
                         const BasisFamily& testBasis,
                         int alpha,
                         int testDerivOrder,
                         const QuadratureFamily& quad);

      /** Construct a two-form to be computed by quadrature */
      QuadratureIntegral(int spatialDim,
                         int dim,
                         const CellType& cellType,
                         const BasisFamily& testBasis,
                         int alpha,
                         int testDerivOrder,
                         const BasisFamily& unkBasis,
                         int beta,
                         int unkDerivOrder,
                         const QuadratureFamily& quad);

      /** virtual dtor */
      virtual ~QuadratureIntegral(){;}

      /** */
      void transform(const CellJacobianBatch& J, 
                     const double* const coeff,
                     RefCountPtr<Array<double> >& A) const 
      {
        if (order()==2) transformTwoForm(J, coeff, A);
        else if (order()==1) transformOneForm(J, coeff, A);
        else transformZeroForm(J, coeff, A);
      }
      
      /** */
      void transformZeroForm(const CellJacobianBatch& J, 
                             const double* const coeff,
                             RefCountPtr<Array<double> >& A) const ;
      
      /** */
      void transformTwoForm(const CellJacobianBatch& J, 
                            const double* const coeff,
                            RefCountPtr<Array<double> >& A) const ;
      
      /** */
      void transformOneForm(const CellJacobianBatch& J, 
                            const double* const coeff,
                            RefCountPtr<Array<double> >& A) const ;

      /** */
      void print(ostream& os) const ;
      


      /** */
      int nQuad() const {return nQuad_;}

      static double& totalFlops() {static double rtn = 0; return rtn;}

    private:

      static void addFlops(const double& flops) {totalFlops() += flops;}

      /** Do the integration by summing reference quantities over quadrature
       * points and then transforming the sum to physical quantities.  */
      void transformSummingFirst(int nCells,
                                 const double* const GPtr,
                                 const double* const coeff,
                                 RefCountPtr<Array<double> >& A) const ;

      /** Do the integration by transforming to physical coordinates 
       * at each quadrature point, and then summing */
      void transformSummingLast(int nCells,
                                const double* const GPtr,
                                const double* const coeff,
                                RefCountPtr<Array<double> >& A) const ;

      /** Determine whether to do this batch of integrals using the
       * sum-first method or the sum-last method */
      bool useSumFirstMethod() const {return useSumFirstMethod_;}
      
      /** */
      inline double& wValue(int q, int testDerivDir, int testNode,
                            int unkDerivDir, int unkNode)
      {return W_[unkNode
                 + nNodesUnk()
                 *(testNode + nNodesTest()
                   *(unkDerivDir + nRefDerivUnk()
                     *(testDerivDir + nRefDerivTest()*q)))];}

      

      /** */
      inline const double& wValue(int q, 
                                  int testDerivDir, int testNode,
                                  int unkDerivDir, int unkNode) const 
      {
        return W_[unkNode
                  + nNodesUnk()
                  *(testNode + nNodesTest()
                    *(unkDerivDir + nRefDerivUnk()
                      *(testDerivDir + nRefDerivTest()*q)))];
      }
      
      /** */
      inline double& wValue(int q, int testDerivDir, int testNode)
      {return W_[testNode + nNodesTest()*(testDerivDir + nRefDerivTest()*q)];}


      /** */
      inline const double& wValue(int q, int testDerivDir, int testNode) const 
      {return W_[testNode + nNodesTest()*(testDerivDir + nRefDerivTest()*q)];}

      /* */
      Array<double> W_;

      /* */
      int nQuad_;

      /* */
      bool useSumFirstMethod_;
      
    };
  }
}

#endif  /* DOXYGEN_DEVELOPER_ONLY */

#endif
