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

#include "Sundance.hpp"


#include "NOX.H"
#include "NOX_Common.H"
#include "NOX_Utils.H"
#include "NOX_TSF_Group.H"


/*
 * Solve the equation u^2 - alpha = 0, where alpha is a stochastic
 * variable.
 */

int main(int argc, void** argv)
{
  
  try
		{
      Sundance::init(&argc, &argv);
      int np = MPIComm::world().getNProc();

      EquationSet::classVerbosity() = VerbExtreme;

      /* We will do our linear algebra using Epetra */
      VectorType<double> vecType = new EpetraVectorType();

      /* Create a mesh. It will be of type BasisSimplicialMesh, and will
       * be built using a PartitionedLineMesher. */
      MeshType meshType = new BasicSimplicialMeshType();
      MeshSource mesher = new PartitionedLineMesher(0.0, 1.0, 1*np, meshType);
      Mesh mesh = mesher.getMesh();

      /* Create a cell filter that will identify the maximal cells
       * in the interior of the domain */
      CellFilter interior = new MaximalCellFilter();

      /* Create the Spectral Basis */
      int ndim = 1;
      int order = 2;
      SpectralBasis sbasis(ndim, order); 
      
      /* Create unknown and test functions, discretized using first-order
       * Lagrange interpolants */
      Expr u = new UnknownFunction(new Lagrange(1), sbasis, "u");
      Expr v = new TestFunction(new Lagrange(1), sbasis, "v");

      /* Create the stochastic input function. */
      Expr a0 = new Parameter(1.0);
      Expr a1 = new Parameter(0.1);
      Expr a2 = new Parameter(0.0);
      Expr alpha = new SpectralExpr(sbasis, tuple(a0, a1, a2));

      /* Create a discrete space, and discretize the function 1.0 on it */
      DiscreteSpace discSpace(mesh, new Lagrange(1), sbasis, vecType);
      Expr u0 = new DiscreteFunction(discSpace, 0.5, "u0");

      /* We need a quadrature rule for doing the integrations */
      QuadratureFamily quad = new GaussianQuadrature(2);

      /* Now we set up the weak form of our equation. */
      Expr eqn = Integral(interior, v*(u*u-alpha), quad);

      cout << "equation = " << eqn << endl;

      /* There are no boundary conditions for this problem, so the
       * BC expression is empty */
      Expr bc;

      /* We can now set up the nonlinear problem! */
      GrouperBase::classVerbosity() = VerbExtreme;
      NonlinearOperator<double> F 
        = new NonlinearProblem(mesh, eqn, bc, v, u, u0, vecType);

      ParameterXMLFileReader reader("../../../tests-std-framework/Problem/nox.xml");
      ParameterList noxParams = reader.getParameters();

      cerr << "solver params = " << noxParams << endl;

      NOXSolver solver(noxParams, F);

      solver.solve();

      /* Inspect solution values. The solution is constant in space,
       * so we can simply take the first NTerms entries in the vector */
      Vector<double> vec = DiscreteFunction::discFunc(u0)->getVector();

      for (int i=0; i<sbasis.nterms(); i++)
        {
          cout << "u[" << i << "] = " << vec[i] << endl;
        }
      
      double tol = 1.0e-12;
      double errorSq = 0.0;
      Sundance::passFailTest(errorSq, tol);
    }
	catch(exception& e)
		{
      cerr << e.what() << endl;
		}
  Sundance::finalize();
}