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

#include "SundanceQuadratureIntegral.hpp"
#include "SundanceGaussianQuadrature.hpp"
#include "SundanceOut.hpp"
#include "SundanceTabs.hpp"

using namespace SundanceStdFwk;
using namespace SundanceStdFwk::Internal;
using namespace SundanceCore;
using namespace SundanceCore::Internal;
using namespace SundanceStdMesh;
using namespace SundanceStdMesh::Internal;
using namespace SundanceUtils;
using namespace Teuchos;
using namespace TSFExtended;

extern "C" 
{
  int dgemm_(const char* transA, const char* transB,
             const int* M, const int *N, const int* K,
             const double* alpha, 
             const double* A, const int* ldA,
             const double* B, const int* ldB,
             const double* beta,
             double* C, const int* ldC);
}

static Time& quadratureTimer() 
{
  static RefCountPtr<Time> rtn 
    = TimeMonitor::getNewTimer("quadrature"); 
  return *rtn;
}


QuadratureIntegral::QuadratureIntegral(int dim, 
                                       const CellType& cellType,
                                       const QuadratureFamily& quad)
  : ElementIntegral(dim, cellType),
    W_(),
    nQuad_(0),
    useSumFirstMethod_(true)
{
  Tabs tab0;
  verbosity() = classVerbosity();

  /* create the quad points and weights */
  Array<double> quadWeights;
  Array<Point> quadPts;
  quad.getPoints(cellType, quadPts, quadWeights);
  nQuad_ = quadPts.size();
  
  W_.resize(nQuad());

  SUNDANCE_OUT(this->verbosity() > VerbLow, 
               tab0 << "num quad pts" << nQuad());

  SUNDANCE_OUT(this->verbosity() > VerbHigh, 
               tab0 << "quad weights" << quadWeights);

  for (int q=0; q<nQuad(); q++)
    {
      W_[q] = quadWeights[q];
    }    
}


QuadratureIntegral::QuadratureIntegral(int dim, 
                                       const CellType& cellType,
                                       const BasisFamily& testBasis,
                                       int alpha,
                                       int testDerivOrder,
                                       const QuadratureFamily& quad)
  : ElementIntegral(dim, cellType, testBasis, alpha, testDerivOrder),
    W_(),
    nQuad_(0),
    useSumFirstMethod_(true)
{
  Tabs tab0;
  verbosity() = classVerbosity();
  SUNDANCE_OUT(this->verbosity() > VerbSilent, 
               tab0 
               << "******** computing basis functions on quad pts *******"
               << endl << tab0 << "test basis=" 
               << testBasis 
               << endl << tab0 << "cell type=" << cellType
               << endl << tab0 << "differentiation order="
               << testDerivOrder);
  SUNDANCE_OUT(this->verbosity() > VerbMedium, 
               tab0 << "num test derivative components=" 
               << nRefDerivTest());

  TEST_FOR_EXCEPTION(testDerivOrder < 0 || testDerivOrder > 1,
                     InternalError,
                     "Test function derivative order=" << testDerivOrder
                     << " must be 0 or 1");
  

  /* create the quad points and weights */
  Array<double> quadWeights;
  Array<Point> quadPts;
  quad.getPoints(cellType, quadPts, quadWeights);
  nQuad_ = quadPts.size();
  
  W_.resize(nQuad() * nRefDerivTest() * nNodesTest());

  SUNDANCE_OUT(this->verbosity() > VerbLow, 
               tab0 << "num quad pts" << nQuad());

  SUNDANCE_OUT(this->verbosity() > VerbHigh, 
               tab0 << "quad pts" << quadPts);

  SUNDANCE_OUT(this->verbosity() > VerbHigh, 
               tab0 << "quad weights" << quadWeights);

  Array<Array<Array<double> > > testBasisVals(nRefDerivTest());

  for (int r=0; r<nRefDerivTest(); r++)
    {
      MultiIndex mi;
      if (testDerivOrder==1) mi[r] = 1;
      testBasis.ptr()->refEval(cellType, quadPts, mi, 
                               testBasisVals[r]);
    }

  SUNDANCE_OUT(this->verbosity() > VerbHigh, 
               tab0 << "basis values" << testBasisVals);


  for (int q=0; q<nQuad(); q++)
    {
      for (int t=0; t<nRefDerivTest(); t++)
        {
          for (int nt=0; nt<nNodesTest(); nt++)
            {
              wValue(q, t, nt) 
                = chop(quadWeights[q] * testBasisVals[t][q][nt]) ;
            }
        }
    }    
  
  addFlops(2*nQuad()*nRefDerivTest()*nNodesTest());

  if (verbosity() > VerbMedium)
    {
      print(cerr);
    }
}




QuadratureIntegral::QuadratureIntegral(int dim,
                                       const CellType& cellType,
                                       const BasisFamily& testBasis,
                                       int alpha,
                                       int testDerivOrder,
                                       const BasisFamily& unkBasis,
                                       int beta,
                                       int unkDerivOrder,
                                       const QuadratureFamily& quad)
  : ElementIntegral(dim, cellType, 
                    testBasis, alpha, testDerivOrder, 
                    unkBasis, beta, unkDerivOrder), 
    W_(),
    nQuad_(0),
    useSumFirstMethod_(true)
{
  Tabs tab0;
  verbosity() = classVerbosity();
  SUNDANCE_OUT(this->verbosity() > VerbSilent, 
               tab0 << " ************* computing basis func products on quad pts ***************" 
               << endl << tab0 << "test basis=" 
               << testBasis 
               << endl << tab0 << "unk basis=" << unkBasis
               << endl << tab0 << "cell type=" << cellType
               << endl << tab0 <<"differentiation order t=" 
               << testDerivOrder << ", u=" << unkDerivOrder);
  SUNDANCE_OUT(this->verbosity() > VerbMedium, 
               tab0 << "num test derivative components=" 
               << nRefDerivTest());

  TEST_FOR_EXCEPTION(testDerivOrder < 0 || testDerivOrder > 1,
                     InternalError,
                     "Test function derivative order=" << testDerivOrder
                     << " must be 0 or 1");
  
  TEST_FOR_EXCEPTION(unkDerivOrder < 0 || unkDerivOrder > 1,
                     InternalError,
                     "Unknown function derivative order=" << unkDerivOrder
                     << " must be 0 or 1");

  /* get the quad pts and weights */
  Array<double> quadWeights;
  Array<Point> quadPts;
  quad.getPoints(cellType, quadPts, quadWeights);
  nQuad_ = quadPts.size();

  W_.resize(nQuad() * nRefDerivTest() * nNodesTest()  
            * nRefDerivUnk() * nNodesUnk());

  SUNDANCE_OUT(this->verbosity() > VerbLow, 
               tab0 << "num quad pts" << nQuad());

  SUNDANCE_OUT(this->verbosity() > VerbHigh, 
               tab0 << "quad pts" << quadPts);

  SUNDANCE_OUT(this->verbosity() > VerbHigh, 
               tab0 << "quad weights" << quadWeights);



  /* compute the basis functions */
  Array<Array<Array<double> > > testBasisVals(nRefDerivTest());
  Array<Array<Array<double> > > unkBasisVals(nRefDerivUnk());

  for (int r=0; r<nRefDerivTest(); r++)
    {
      MultiIndex mi;
      if (testDerivOrder==1) mi[r] = 1;
      testBasis.ptr()->refEval(cellType, quadPts, mi, 
                               testBasisVals[r]);
    }

  SUNDANCE_OUT(this->verbosity() > VerbHigh, 
               tab0 << "test basis values" << testBasisVals);

  for (int r=0; r<nRefDerivUnk(); r++)
    {
      MultiIndex mi;
      if (unkDerivOrder==1) mi[r] = 1;
      unkBasis.ptr()->refEval(cellType, quadPts, mi, unkBasisVals[r]);
    }


  SUNDANCE_OUT(this->verbosity() > VerbHigh, 
               tab0 << "unk basis values" << unkBasisVals);


  /* form the products of basis functions at each quad pt */
  for (int q=0; q<nQuad(); q++)
    {
      for (int t=0; t<nRefDerivTest(); t++)
        {
          for (int nt=0; nt<nNodesTest(); nt++)
            {
              for (int u=0; u<nRefDerivUnk(); u++)
                {
                  for (int nu=0; nu<nNodesUnk(); nu++)
                    {
                      wValue(q, t, nt, u, nu)
                        = chop(quadWeights[q] * testBasisVals[t][q][nt] 
                               * unkBasisVals[u][q][nu]);
                    }
                }
            }
        }
    }

  addFlops(3*nQuad()*nRefDerivTest()*nNodesTest()*nRefDerivUnk()*nNodesUnk()
           + W_.size());
  for (int i=0; i<W_.size(); i++) W_[i] = chop(W_[i]);

  if (verbosity() > VerbMedium)
    {
      print(cerr);
    }

}


void QuadratureIntegral::print(ostream& os) const 
{
  
}

void QuadratureIntegral::transformZeroForm(const CellJacobianBatch& J,  
                                           const double* const coeff,
                                           RefCountPtr<Array<double> >& A) const
{
  TimeMonitor timer(quadratureTimer());
  TEST_FOR_EXCEPTION(order() != 0, InternalError,
                     "QuadratureIntegral::transformZeroForm() called "
                     "for form of order " << order());

  int flops = 0;

  //  A->resize(1);
  double& a = (*A)[0];
  //  a = 0.0;
  double* coeffPtr = (double*) coeff;
  for (int c=0; c<J.numCells(); c++)
    {
      double detJ = fabs(J.detJ()[c]);
      for (int q=0; q<nQuad(); q++, coeffPtr++)
        {
          a += W_[q]*(*coeffPtr)*detJ;
        }
    }

  addFlops(J.numCells()*(1 + 2*nQuad()));
}



void QuadratureIntegral::transformOneForm(const CellJacobianBatch& J,  
                                          const double* const coeff,
                                          RefCountPtr<Array<double> >& A) const
{
  TimeMonitor timer(quadratureTimer());
  TEST_FOR_EXCEPTION(order() != 1, InternalError,
                     "QuadratureIntegral::transformOneForm() called for form "
                     "of order " << order());

  int flops = 0;

  /* If the derivative order is zero, the only thing to be done 
   * is to multiply by the cell's Jacobian determinant and sum over the
   * quad points */
  if (testDerivOrder() == 0)
    {
      double* aPtr = &((*A)[0]);
      double* coeffPtr = (double*) coeff;
      int offset = 0 ;

      for (int c=0; c<J.numCells(); c++, offset+=nNodes())
        {
          double detJ = fabs(J.detJ()[c]);
          for (int q=0; q<nQuad(); q++, coeffPtr++)
            {
              double f = (*coeffPtr)*detJ;
              for (int n=0; n<nNodes(); n++) 
                {
                  aPtr[offset+n] += f*W_[n + nNodes()*q];
                }
            }
        }
      addFlops( J.numCells() * (1 + nQuad() * (1 + 2*nNodes())) );
    }
  else
    {
      int nCells = J.numCells();
      //      A->resize(nCells * nNodes()); 

      createOneFormTransformationMatrix(J);

      SUNDANCE_OUT(this->verbosity() > VerbMedium, 
                   Tabs() << "transformation matrix=" << G(alpha()));

      double* GPtr = &(G(alpha())[0]);      

      if (useSumFirstMethod())
        {
          transformSummingFirst(J.numCells(), GPtr, coeff, A);
        }
      else
        {
          transformSummingLast(J.numCells(), GPtr, coeff, A);
        }
    }
  addFlops(flops);
}


void QuadratureIntegral::transformTwoForm(const CellJacobianBatch& J,  
                                          const double* const coeff,
                                          RefCountPtr<Array<double> >& A) const
{
  TimeMonitor timer(quadratureTimer());
  TEST_FOR_EXCEPTION(order() != 2, InternalError,
                     "QuadratureIntegral::transformTwoForm() called for form "
                     "of order " << order());

  /* If the derivative orders are zero, the only thing to be done 
   * is to multiply by the cell's Jacobian determinant and sum over the
   * quad points */
  if (testDerivOrder() == 0 && unkDerivOrder() == 0)
    {
      double* aPtr = &((*A)[0]);
      double* coeffPtr = (double*) coeff;
      int offset = 0 ;

      for (int c=0; c<J.numCells(); c++, offset+=nNodes())
        {
          double detJ = fabs(J.detJ()[c]);
          for (int q=0; q<nQuad(); q++, coeffPtr++)
            {
              double f = (*coeffPtr)*detJ;
              for (int n=0; n<nNodes(); n++) 
                {
                  aPtr[offset+n] += f*W_[n + nNodes()*q];
                }
            }
        }

      addFlops( J.numCells() * (1 + nQuad() * (1 + 2*nNodes())) );
    }
  else
    {
      int nCells = J.numCells();

      createTwoFormTransformationMatrix(J);
      double* GPtr;

      if (testDerivOrder() == 0)
        {
          GPtr = &(G(beta())[0]);
          SUNDANCE_OUT(this->verbosity() > VerbMedium, 
                       Tabs() << "transformation matrix=" << G(beta()));
        }
      else if (unkDerivOrder() == 0)
        {
          GPtr = &(G(alpha())[0]);
          SUNDANCE_OUT(this->verbosity() > VerbMedium, 
                       Tabs() << "transformation matrix=" << G(alpha()));
        }
      else
        {
          GPtr = &(G(alpha(), beta())[0]);
          SUNDANCE_OUT(this->verbosity() > VerbMedium, 
                       Tabs() << "transformation matrix=" 
                       << G(alpha(),beta()));
        }
        
      
      if (useSumFirstMethod())
        {
          transformSummingFirst(J.numCells(), GPtr, coeff, A);
        }
      else
        {
          transformSummingLast(J.numCells(), GPtr, coeff, A);
        }
    }
}

void QuadratureIntegral
::transformSummingFirst(int nCells,
                        const double* const GPtr,
                        const double* const coeff,
                        RefCountPtr<Array<double> >& A) const
{
  double* aPtr = &((*A)[0]);
  double* coeffPtr = (double*) coeff;
  
  int transSize = 0; 
  if (order()==2)
    {
      transSize = nRefDerivTest() * nRefDerivUnk();
    }
  else
    {
      transSize = nRefDerivTest();
    }

  if (false)
    {
      cerr << "nCells = " << nCells << endl;
      cerr << "nNodes = " << nNodes() << endl;
      cerr << "nQuad = " << nQuad() << endl;
      cerr << "transSize = " << transSize << endl;
    }
  /* The sum workspace is used to store the sum of untransformed quantities */
  static Array<double> sumWorkspace;

  int swSize = transSize * nNodes();
  sumWorkspace.resize(swSize);
  
  
  /*
   * The number of operations for the sum-first method is 
   * 
   * Adds: (N_c * nNodes * transSize) * (N_q + 1) 
   * Multiplies: same as number of adds
   * Total: 2*(N_c * nNodes * transSize) * (N_q + 1) 
   */
  
  for (int c=0; c<nCells; c++)
    {
      /* sum untransformed basis combinations over quad points */
      for (int i=0; i<swSize; i++) sumWorkspace[i]=0.0;

      for (int q=0; q<nQuad(); q++, coeffPtr++)
        {
          double f = (*coeffPtr);
          for (int n=0; n<swSize; n++) 
            {
              sumWorkspace[n] += f*W_[n + q*swSize];
            }
        }
      /* transform the sum */
      const double * const gCell = &(GPtr[transSize*c]);
      double* aCell = aPtr + nNodes()*c;
      for (int i=0; i<nNodes(); i++)
        {
          for (int j=0; j<transSize; j++)
            {
              aCell[i] += sumWorkspace[nNodes()*j + i] * gCell[j];
            }
        }
    }
  
  int flops = 2*(nCells * nNodes() * transSize) * (nQuad() + 1) ;
  addFlops(flops);
}

void QuadratureIntegral
::transformSummingLast(int nCells,
                       const double* const GPtr,
                       const double* const coeff,
                       RefCountPtr<Array<double> >& A) const
{
  double* aPtr = &((*A)[0]);
  int transSize = 0; 
  
  int nAdds = 0;
  int nMults = 0;

  if (order()==2)
    {
      transSize = nRefDerivTest() * nRefDerivUnk();
    }
  else
    {
      transSize = nRefDerivTest();
    }

  /* This workspace is used to store the jacobian values scaled by the coeff
   * at that quad point */
  static Array<double> jWorkspace;
  jWorkspace.resize(transSize);


  /*
   * The number of operations for the sum-last method is 
   * 
   * Adds: N_c * N_q * transSize * nNodes
   * Multiplies: numAdds + N_c * N_q * transSize
   * Total: N_c * N_q * transSize * (1 +  2*nNodes)
   */

  for (int c=0; c<nCells; c++)
    {
      const double* const gCell = &(GPtr[transSize*c]);
      double* aCell = aPtr + nNodes()*c;

      for (int q=0; q<nQuad(); q++)
        {
          double f = coeff[c*nQuad() + q];
          for (int t=0; t<transSize; t++, nMults++) jWorkspace[t]=f*gCell[t];

          for (int n=0; n<nNodes(); n++)
            {
              for (int t=0; t<transSize; t++)
                {
                  aCell[n] += jWorkspace[t]*W_[n + nNodes()*(t + transSize*q)];
                }
            }
        }
    }
  int flops = nCells * nQuad() * transSize * (1 + 2*nNodes()) ;
  addFlops(flops);
}

