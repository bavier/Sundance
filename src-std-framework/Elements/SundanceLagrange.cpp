/* @HEADER@ */
/* @HEADER@ */

#include "SundanceLagrange.hpp"
#include "SundanceADReal.hpp"
#include "SundanceExceptions.hpp"

using namespace SundanceStdFwk;
using namespace SundanceStdFwk::Internal;
using namespace SundanceCore::Internal;
using namespace Teuchos;

Lagrange::Lagrange(int order)
  : BasisFamilyBase(), order_(order)
{;}

XMLObject Lagrange::toXML() const 
{
  XMLObject rtn("Lagrange");
  rtn.addAttribute("order", Teuchos::toString(order()));
  return rtn;
}

void Lagrange::getLocalDOFs(const CellType& cellType,
                            Array<Array<Array<int> > >& dofs) const 
{
  switch(cellType)
    {
    case PointCell:
      dofs.resize(1);
      dofs[0] = tuple(tuple(0));
      return;
    case LineCell:
      dofs.resize(2);
      dofs[0] = tuple(tuple(0), tuple(1));
      dofs[1] = tuple(makeRange(2, order()));
      return;
    case TriangleCell:
      {
        int n = order()-1;
        dofs.resize(3);
        dofs[0] = tuple(tuple(0), tuple(1), tuple(2));
        dofs[1] = tuple(makeRange(3,2+n), 
                        makeRange(3+n, 2+2*n),
                        makeRange(3+2*n, 2+3*n));
                                
        dofs[2] = tuple(makeRange(3, order()));
        return;
      }
    case TetCell:
      {
        dofs.resize(4);
        dofs[0] = tuple(tuple(0), tuple(1), tuple(2), tuple(3));
        if (order() == 2)
          {
            dofs[1] = tuple(tuple(4), tuple(5), tuple(6), 
                            tuple(7), tuple(8), tuple(9));
          }
        dofs[2] = tuple(Array<int>(), Array<int>(), 
                        Array<int>(), Array<int>());
        dofs[3] = tuple(Array<int>());
        return;
      }
    default:
      TEST_FOR_EXCEPTION(true, RuntimeError, "Cell type "
                         << cellType << " not implemented in Lagrange basis");
    }
}



Array<int> Lagrange::makeRange(int low, int high)
{
  if (high < low) return Array<int>();

  Array<int> rtn(high-low+1);
  for (int i=0; i<rtn.length(); i++) rtn[i] = low+i;
  return rtn;
}

void Lagrange::refEval(const CellType& cellType,
                       const Array<Point>& pts,
                       const MultiIndex& deriv,
                       Array<Array<double> >& result) const
{
  result.resize(pts.length());

  switch(cellType)
    {
    case PointCell:
      result = tuple(tuple(1.0));
      return;
    case LineCell:
      for (int i=0; i<pts.length(); i++)
        {
          evalOnLine(pts[i], deriv, result[i]);
        }
      return;
    case TriangleCell:
      for (int i=0; i<pts.length(); i++)
        {
          evalOnTriangle(pts[i], deriv, result[i]);
        }
      return;
    default:
      SUNDANCE_ERROR("Lagrange::refEval() unimplemented for cell type ");

    }
}

/* ---------- evaluation on different cell types -------------- */

void Lagrange::evalOnLine(const Point& pt, 
													const MultiIndex& deriv,
													Array<double>& result) const
{
	ADReal x = ADReal(pt[0], 0, 1);
	ADReal one(1.0, 1);
	
	result.resize(order()+1);
	Array<ADReal> tmp(result.length());

	switch(order_)
		{
		case 0:
			tmp[0] = one;
			break;
		case 1:
			tmp[0] = 1.0 - x;
			tmp[1] = x;
			break;
		case 2:
			tmp[0] = 2.0*(1.0-x)*(0.5-x);
			tmp[1] = 2.0*x*(x-0.5);
			tmp[2] = 4.0*x*(1.0-x);
			break;
		default:
			SUNDANCE_ERROR("Lagrange::evalOnLine polynomial order > 2 has not been"
                     " implemented");
		}

	for (int i=0; i<tmp.length(); i++)
		{
			if (deriv.order()==0) result[i] = tmp[i].value();
			else result[i] = tmp[i].gradient()[0];
		}
}

void Lagrange::evalOnTriangle(const Point& pt, 
															const MultiIndex& deriv,
															Array<double>& result) const



{
	ADReal x = ADReal(pt[0], 0, 2);
	ADReal y = ADReal(pt[1], 1, 2);
	ADReal one(1.0, 2);

  Array<ADReal> tmp;
  

	switch(order_)
		{
		case 0:
      result.resize(1);
      tmp.resize(1);
			tmp[0] = one;
			break;
		case 1:
      result.resize(3);
      tmp.resize(3);
			tmp[0] = 1.0 - x - y;
			tmp[1] = x;
			tmp[2] = y;
			break;
		case 2:
      result.resize(6);
      tmp.resize(6);
			tmp[0] = (1.0-x-y)*(1.0-2.0*x-2.0*y);
			tmp[1] = 2.0*x*(x-0.5);
			tmp[2] = 2.0*y*(y-0.5);
			tmp[3] = 4.0*x*(1.0-x-y); 
			tmp[4] = 4.0*x*y;
			tmp[5] = 4.0*y*(1.0-x-y);
			break;
		default:
			SUNDANCE_ERROR("Lagrange::evalOnTriangle poly order > 2");
		}

	for (int i=0; i<tmp.length(); i++)
		{
			if (deriv.order()==0) result[i] = tmp[i].value();
			else 
				result[i] = tmp[i].gradient()[deriv.firstOrderDirection()];
		}
}

