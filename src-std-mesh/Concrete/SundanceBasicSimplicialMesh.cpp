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

#include "SundanceBasicSimplicialMesh.hpp"
#include "SundanceMeshType.hpp"
#include "SundanceCellJacobianBatch.hpp"
#include "SundanceMeshSource.hpp"
#include "SundanceDebug.hpp"
#include "SundanceOut.hpp"
#include "Teuchos_MPIContainerComm.hpp"
#include "Teuchos_Time.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include "TSFObjectWithVerbosity.hpp"
#include "SundanceCollectiveExceptionCheck.hpp"

#include <unistd.h>

using namespace SundanceStdMesh::Internal;
using namespace SundanceStdMesh;
using namespace Teuchos;
using namespace SundanceUtils;

using std::endl;

static Time& batchedFacetGrabTimer() 
{
  static RefCountPtr<Time> rtn 
    = TimeMonitor::getNewTimer("batched facet grabbing"); 
  return *rtn;
}




static Time& getJacobianTimer() 
{
  static RefCountPtr<Time> rtn 
    = TimeMonitor::getNewTimer("cell Jacobian grabbing"); 
  return *rtn;
}


//#define SKIP_FACES

BasicSimplicialMesh::BasicSimplicialMesh(int dim, const MPIComm& comm)
	: IncrementallyCreatableMesh(dim, comm),
    numCells_(dim+1),
    points_(),
    edgeVerts_(2),
    faceVertLIDs_(dim),
    faceVertGIDs_(dim),
    faceEdges_(dim),
    faceEdgeSigns_(dim),
    elemVerts_(dim+1),
    elemEdges_(),
    elemEdgeSigns_(),
    elemFaces_(dim+1),
    elemFaceRotations_(dim+1),
    vertexSetToFaceIndexMap_(),
    edgeFaces_(),
    edgeCofacets_(),
    faceCofacets_(),
    vertEdges_(),
    vertFaces_(),
    vertCofacets_(),
    vertEdgePartners_(),
    LIDToGIDMap_(dim+1),
    GIDToLIDMap_(dim+1),
    labels_(dim+1),
    ownerProcID_(dim+1),
    faceVertGIDBase_(1),
    hasEdgeGIDs_(false),
    hasFaceGIDs_(false),
    neighbors_(),
    neighborsAreSynchronized_(false)
{
  verbosity() = MeshSource::classVerbosity();
  //  verbosity() = VerbExtreme;
  //  Out::setLogFile("log." + Teuchos::toString(comm.getRank()));
  estimateNumVertices(1000);
  estimateNumElements(1000);

  /* Set up the pointer giving a view of the face vertex array.
   * Resize to 1 so that phony dereference will work, then resize to zero to make the
   * new array logically empty */
  faceVertGIDs_.resize(1);
  faceVertGIDBase_[0] = &(faceVertGIDs_.value(0,0));
  faceVertGIDs_.resize(0);

  /* size the element edge lists as appropriate to the mesh's dimension */
  if (spatialDim()==2) 
    {
      elemEdges_.setTupleSize(3);
      elemEdgeSigns_.setTupleSize(3);
    }
  if (spatialDim()==3) 
    {
      elemEdges_.setTupleSize(6);
      elemEdgeSigns_.setTupleSize(6);
    }

  neighbors_.put(comm.getRank());
}


void BasicSimplicialMesh::getJacobians(int cellDim, const Array<int>& cellLID,
                                       CellJacobianBatch& jBatch) const
{
  TimeMonitor timer(getJacobianTimer());

  int flops = 0 ;
  int nCells = cellLID.size();

  TEST_FOR_EXCEPTION(cellDim < 0 || cellDim > spatialDim(), InternalError,
                     "cellDim=" << cellDim 
                     << " is not in expected range [0, " << spatialDim()
                     << "]");

  jBatch.resize(cellLID.size(), spatialDim(), cellDim);

  if (cellDim < spatialDim())
    {
      switch(cellDim)
        {
        case 1:
          flops += 3*nCells;
          break;
        case 2:
          // 4 flops for two pt subtractions, 10 for a cross product
          flops += (4 + 10)*nCells;
          break;
        default:
          break;
        }

      for (int i=0; i<nCells; i++)
        {
          int lid = cellLID[i];
          double* detJ = jBatch.detJ(i);
          switch(cellDim)
            {
            case 0:
              *detJ = 1.0;
              break;
            case 1:
              {
                int a = edgeVerts_.value(lid, 0);
                int b = edgeVerts_.value(lid, 1);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                Point dx = pb-pa;
                *detJ = sqrt(dx*dx);
              }
              break;
            case 2:
              {
                int a = faceVertLIDs_.value(lid, 0);
                int b = faceVertLIDs_.value(lid, 1);
                int c = faceVertLIDs_.value(lid, 2);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                const Point& pc = points_[c];
                Point directedArea = cross(pc-pa, pb-pa);
                *detJ = sqrt(directedArea*directedArea);
              }
              break;
            default:
              TEST_FOR_EXCEPTION(true, InternalError, "impossible switch value "
                                 "cellDim=" << cellDim 
                                 << " in BasicSimplicialMesh::getJacobians()");
            }
        }
    }
  else
    {
      Array<double> J(cellDim*cellDim);
  
      flops += cellDim*cellDim*nCells;

      for (unsigned int i=0; i<cellLID.size(); i++)
        {
          int lid = cellLID[i];
          double* J = jBatch.jVals(i);
          switch(cellDim)
            {
            case 0:
              J[0] = 1.0;
              break;
            case 1:
              {
                int a = elemVerts_.value(lid, 0);
                int b = elemVerts_.value(lid, 1);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                J[0] = fabs(pa[0]-pb[0]);
              }
              break;
            case 2:
              {
                int a = elemVerts_.value(lid, 0);
                int b = elemVerts_.value(lid, 1);
                int c = elemVerts_.value(lid, 2);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                const Point& pc = points_[c];
                J[0] = pb[0] - pa[0];
                J[1] = pc[0] - pa[0];
                J[2] = pb[1] - pa[1];
                J[3] = pc[1] - pa[1];
            
              }
              break;
            case 3:
              {
                int a = elemVerts_.value(lid, 0);
                int b = elemVerts_.value(lid, 1);
                int c = elemVerts_.value(lid, 2);
                int d = elemVerts_.value(lid, 3);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                const Point& pc = points_[c];
                const Point& pd = points_[d];
                J[0] = pb[0] - pa[0];
                J[1] = pc[0] - pa[0];
                J[2] = pd[0] - pa[0];
                J[3] = pb[1] - pa[1];
                J[4] = pc[1] - pa[1];
                J[5] = pd[1] - pa[1];
                J[6] = pb[2] - pa[2];
                J[7] = pc[2] - pa[2];
                J[8] = pd[2] - pa[2];
              }
              break;
            default:
              TEST_FOR_EXCEPTION(true, InternalError, "impossible switch value "
                                 "cellDim=" << cellDim 
                                 << " in BasicSimplicialMesh::getJacobians()");
            }
        }
    }
  CellJacobianBatch::addFlops(flops);
}



void BasicSimplicialMesh::getCellDiameters(int cellDim, const Array<int>& cellLID,
                                           Array<double>& cellDiameters) const
{
  TEST_FOR_EXCEPTION(cellDim < 0 || cellDim > spatialDim(), InternalError,
                     "cellDim=" << cellDim 
                     << " is not in expected range [0, " << spatialDim()
                     << "]");

  cellDiameters.resize(cellLID.size());

  if (cellDim < spatialDim())
    {
      for (unsigned int i=0; i<cellLID.size(); i++)
        {
          int lid = cellLID[i];
          switch(cellDim)
            {
            case 0:
              cellDiameters[i] = 1.0;
              break;
            case 1:
              {
                int a = edgeVerts_.value(lid, 0);
                int b = edgeVerts_.value(lid, 1);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                Point dx = pb-pa;
                cellDiameters[i] = sqrt(dx*dx);
              }
              break;
            case 2:
              {
                int a = faceVertLIDs_.value(lid, 0);
                int b = faceVertLIDs_.value(lid, 1);
                int c = faceVertLIDs_.value(lid, 2);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                const Point& pc = points_[c];
                Point directedArea = cross(pc-pa, pb-pa);
                cellDiameters[i] = sqrt(directedArea*directedArea);
              }
              break;
            default:
              TEST_FOR_EXCEPTION(true, InternalError, "impossible switch value "
                                 "cellDim=" << cellDim 
                                 << " in BasicSimplicialMesh::getCellDiameters()");
            }
        }
    }
  else
    {
      for (unsigned int i=0; i<cellLID.size(); i++)
        {
          int lid = cellLID[i];
          switch(cellDim)
            {
            case 0:
              cellDiameters[i] = 1.0;
              break;
            case 1:
              {
                int a = elemVerts_.value(lid, 0);
                int b = elemVerts_.value(lid, 1);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                cellDiameters[i] = fabs(pa[0]-pb[0]);
              }
              break;
            case 2:
              {
                int a = elemVerts_.value(lid, 0);
                int b = elemVerts_.value(lid, 1);
                int c = elemVerts_.value(lid, 2);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                const Point& pc = points_[c];
                cellDiameters[i] 
                  = (pa.distance(pb) + pb.distance(pc) + pa.distance(pc))/3.0;
              }
              break;
            case 3:
              {
                int a = elemVerts_.value(lid, 0);
                int b = elemVerts_.value(lid, 1);
                int c = elemVerts_.value(lid, 2);
                int d = elemVerts_.value(lid, 3);
                const Point& pa = points_[a];
                const Point& pb = points_[b];
                const Point& pc = points_[c];
                const Point& pd = points_[d];
                
                cellDiameters[i] 
                  = (pa.distance(pb) + pa.distance(pc) + pa.distance(pd)
                     + pb.distance(pc) + pb.distance(pd)
                     + pc.distance(pd))/6.0;
              }
              break;
            default:
              TEST_FOR_EXCEPTION(true, InternalError, "impossible switch value "
                                 "cellDim=" << cellDim 
                                 << " in BasicSimplicialMesh::getCellDiameters()");
            }
        }
    }
}


void BasicSimplicialMesh::pushForward(int cellDim, const Array<int>& cellLID,
                                      const Array<Point>& refQuadPts,
                                      Array<Point>& physQuadPts) const
{
  TEST_FOR_EXCEPTION(cellDim < 0 || cellDim > spatialDim(), InternalError,
                     "cellDim=" << cellDim 
                     << " is not in expected range [0, " << spatialDim()
                     << "]");
  int flops = 0;
  int nQuad = refQuadPts.size();
  int nCells = cellLID.size();
  Array<double> J(cellDim*cellDim);

  if (physQuadPts.size() > 0) physQuadPts.resize(0);
  physQuadPts.reserve(cellLID.size() * refQuadPts.size());

  switch(cellDim)
    {
    case 1:
      flops += nCells * (1 + 2*nQuad);
      break;
    case 2:
      if (spatialDim()==2)
        {
          flops += nCells*(4 + 8*nQuad);
        }
      else
        {
          flops += 18*nCells*nQuad;
        }
      break;
    case 3:
      flops += 27*nCells*nQuad;
      break;
    default:
      break;
    }


  for (unsigned int i=0; i<cellLID.size(); i++)
    {
      int lid = cellLID[i];
      switch(cellDim)
        {
        case 0:
          physQuadPts.append(points_[lid]);
          break;
        case 1:
          {
            int a, b;
            if (spatialDim()==1)
              {
                a = elemVerts_.value(lid, 0);
                b = elemVerts_.value(lid, 1);
              }
            else
              {
                a = edgeVerts_.value(lid, 0);
                b = edgeVerts_.value(lid, 1);
              }
            const Point& pa = points_[a];
            const Point& pb = points_[b];
            Point dx = pb-pa;
            for (int q=0; q<nQuad; q++)
              {
                physQuadPts.append(pa + refQuadPts[q][0]*dx);
              }
          }
          break;
        case 2:
          {
            int a,b,c;
            if (spatialDim()==2)
              {
                a = elemVerts_.value(lid, 0);
                b = elemVerts_.value(lid, 1);
                c = elemVerts_.value(lid, 2);
              }
            else
              {
                a = faceVertLIDs_.value(lid, 0);
                b = faceVertLIDs_.value(lid, 1);
                c = faceVertLIDs_.value(lid, 2);
              }
            const Point& pa = points_[a];
            const Point& pb = points_[b];
            const Point& pc = points_[c];

            if (spatialDim()==2)
              {
                J[0] = pb[0] - pa[0];
                J[1] = pc[0] - pa[0];
                J[2] = pb[1] - pa[1];
                J[3] = pc[1] - pa[1];
                for (int q=0; q<nQuad; q++)
                  {
                    physQuadPts.append(pa 
                                       + Point(J[0]*refQuadPts[q][0] +J[1]*refQuadPts[q][1],
                                               J[2]*refQuadPts[q][0] +J[3]*refQuadPts[q][1]) );
                  }
              }
            else
              {
                for (int q=0; q<nQuad; q++)
                  {
                    physQuadPts.append(pa 
                                       + (pb-pa)*refQuadPts[q][0] 
                                       + (pc-pa)*refQuadPts[q][1]);
                  }
              }
            
          }
          break;
        case 3:
          {
            int a = elemVerts_.value(lid, 0);
            int b = elemVerts_.value(lid, 1);
            int c = elemVerts_.value(lid, 2);
            int d = elemVerts_.value(lid, 3);
            const Point& pa = points_[a];
            const Point& pb = points_[b];
            const Point& pc = points_[c];
            const Point& pd = points_[d];
            J[0] = pb[0] - pa[0];
            J[1] = pc[0] - pa[0];
            J[2] = pd[0] - pa[0];
            J[3] = pb[1] - pa[1];
            J[4] = pc[1] - pa[1];
            J[5] = pd[1] - pa[1];
            J[6] = pb[2] - pa[2];
            J[7] = pc[2] - pa[2];
            J[8] = pd[2] - pa[2];
            
            for (int q=0; q<nQuad; q++)
              {
                physQuadPts.append(pa 
                                   + Point(J[0]*refQuadPts[q][0] 
                                           + J[1]*refQuadPts[q][1]
                                           + J[2]*refQuadPts[q][2],
                                           J[3]*refQuadPts[q][0] 
                                           + J[4]*refQuadPts[q][1]
                                           + J[5]*refQuadPts[q][2],
                                           J[6]*refQuadPts[q][0] 
                                           + J[7]*refQuadPts[q][1]
                                           + J[8]*refQuadPts[q][2]));

              }
            
          }
          break;
        default:
          TEST_FOR_EXCEPTION(true, InternalError, "impossible switch value "
                             "in BasicSimplicialMesh::getJacobians()");
        }
    }

  CellJacobianBatch::addFlops(flops);
}

void BasicSimplicialMesh::estimateNumVertices(int nPts)
{
  points_.reserve(nPts);
  vertCofacets_.reserve(nPts);
  vertEdges_.reserve(nPts);
  vertEdgePartners_.reserve(nPts);

  ownerProcID_[0].reserve(nPts);
  LIDToGIDMap_[0].reserve(nPts);
  GIDToLIDMap_[0] = Hashtable<int,int>(nPts, 0.6);
  labels_[0].reserve(nPts);
}

void BasicSimplicialMesh::estimateNumElements(int nElems)
{
  int nEdges = 0;
  int nFaces = 0;

  if (spatialDim()==3) 
    {
      nFaces = 5*nElems;
      nEdges = 5*nElems;
      labels_[2].reserve(nFaces);
    }
  else if (spatialDim()==2)
    {
      nEdges = 3*nElems;
    }
  
  labels_[1].reserve(nEdges);
  vertexSetToFaceIndexMap_ = Hashtable<VertexView, int>(nFaces);

  edgeVerts_.reserve(nEdges);
  faceVertLIDs_.reserve(nFaces);
  faceVertGIDs_.reserve(nFaces);
  faceEdges_.reserve(nFaces);
  elemVerts_.reserve(nElems);
  elemEdges_.reserve(nElems);
  elemEdgeSigns_.reserve(nElems);
  elemFaces_.reserve(nElems);
  edgeCofacets_.reserve(nEdges);
  faceCofacets_.reserve(nFaces);

  ownerProcID_[spatialDim()].reserve(nElems);
  LIDToGIDMap_[spatialDim()].reserve(nElems);
  GIDToLIDMap_[spatialDim()] = Hashtable<int,int>(nElems, 0.6);
  labels_[spatialDim()].reserve(nElems);

  /* resize to 1 so that phony dereference will work, then resize to zero to make the
   * new array logically empty */
  faceVertGIDs_.resize(1);
  faceVertGIDBase_[0] = &(faceVertGIDs_.value(0,0));
  faceVertGIDs_.resize(0);
}



int BasicSimplicialMesh::numCells(int cellDim) const
{
  return numCells_[cellDim];
}

int BasicSimplicialMesh::ownerProcID(int cellDim, int cellLID) const
{
  return ownerProcID_[cellDim][cellLID];
}

int BasicSimplicialMesh::numFacets(int cellDim, int cellLID, 
                                   int facetDim) const
{
  if (cellDim==1)
    {
      return 2;
    }
  else if (cellDim==2)
    {
      return 3;
    }
  else 
    {
      if (facetDim==0) return 4;
      if (facetDim==1) return 6;
      return 4;
    }
}

void BasicSimplicialMesh::getFacetLIDs(int cellDim, 
                                       const Array<int>& cellLID,
                                       int facetDim,
                                       Array<int>& facetLID,
                                       Array<int>& facetSign) const 
{
  TimeMonitor timer(batchedFacetGrabTimer());

  int nf = numFacets(cellDim, cellLID[0], facetDim);
  facetLID.resize(cellLID.size() * nf);
  if (facetDim > 0) facetSign.resize(cellLID.size() * nf);

  
  if (facetDim==0)
    {
      if (cellDim == spatialDim())
        {
          int ptr=0;
          for (unsigned int c=0; c<cellLID.size(); c++)
            {
              const int* fPtr = &(elemVerts_.value(cellLID[c], 0));
              for (int f=0; f<nf; f++, ptr++)
                {
                  facetLID[ptr] = fPtr[f];
                }
            }
        }
      else if (cellDim==1) 
        {
          int ptr=0;
          for (unsigned int c=0; c<cellLID.size(); c++)
            {
              const int* fPtr = &(edgeVerts_.value(cellLID[c], 0));
              for (int f=0; f<nf; f++, ptr++)
                {
                  facetLID[ptr] = fPtr[f];
                }
            }
        }
      else if (cellDim==2) 
        {
          int ptr=0;
          for (unsigned int c=0; c<cellLID.size(); c++)
            {
              const int* fPtr = &(faceVertLIDs_.value(cellLID[c], 0));
              for (int f=0; f<nf; f++, ptr++)
                {
                  facetLID[ptr] = fPtr[f];
                }
            }
        }
    }
  else if (facetDim==1)
    {
      int ptr=0;
      if (cellDim == spatialDim())
        {
          for (unsigned int c=0; c<cellLID.size(); c++)
            {
              const int* fPtr = &(elemEdges_.value(cellLID[c], 0));
              const int* fsPtr = &(elemEdgeSigns_.value(cellLID[c], 0));
              for (int f=0; f<nf; f++, ptr++)
                {
                  facetLID[ptr] = fPtr[f];
                  facetSign[ptr] = fsPtr[f];
                }
            }
        }
      else
        {
          for (unsigned int c=0; c<cellLID.size(); c++)
            {
              const int* fPtr = &(faceEdges_.value(cellLID[c], 0));
              //    const int* fsPtr = &(faceEdgeSigns_.value(cellLID[c], 0));
              for (int f=0; f<nf; f++, ptr++)
                {
                  facetLID[ptr] = fPtr[f];
                  //  facetSign[ptr] = fsPtr[f];
                }
            }
        }
    }
  else
    {
      int ptr=0;
      for (unsigned int c=0; c<cellLID.size(); c++)
        {
          const int* fPtr = &(elemFaces_.value(cellLID[c], 0));
          const int* fsPtr = &(elemFaceRotations_.value(cellLID[c], 0));
          for (int f=0; f<nf; f++, ptr++)
            {
              facetLID[ptr] = fPtr[f];
              facetSign[ptr] = fsPtr[f];
            }
        }
    }
}

int BasicSimplicialMesh::facetLID(int cellDim, int cellLID,
                                  int facetDim, int facetIndex, int& facetSign) const 
{

  if (facetDim==0)
    {
      if (cellDim == spatialDim())
        {
          return elemVerts_.value(cellLID, facetIndex);
        }
      else if (cellDim==1) return edgeVerts_.value(cellLID, facetIndex);
      else if (cellDim==2) return faceVertLIDs_.value(cellLID, facetIndex);
    }
  if (facetDim==1)
    {
      if (cellDim==spatialDim())
        {
          facetSign = elemEdgeSigns_.value(cellLID, facetIndex);
          return elemEdges_.value(cellLID, facetIndex);
        }
      else
        {
          //facetSign = faceEdgeSigns_.value(cellLID, facetIndex);
          return faceEdges_.value(cellLID, facetIndex);
        }
    }
  else
    {
      facetSign = elemFaceRotations_.value(cellLID, facetIndex);
      return elemFaces_.value(cellLID, facetIndex);
    }
}


int BasicSimplicialMesh::numMaxCofacets(int cellDim, int cellLID) const 
{

  if (cellDim==0)
    {
      return vertCofacets_[cellLID].length();
    }
  if (cellDim==1)
    {
      return edgeCofacets_[cellLID].length();
    }
  if (cellDim==2)
    {
      return faceCofacets_[cellLID].length();
    }
  return -1; // -Wall
}



int BasicSimplicialMesh::maxCofacetLID(int cellDim, int cellLID,
                                    int cofacetIndex,
                                    int& facetIndex) const
{
  int rtn = -1;
  if (cellDim==0)
    {
      rtn = vertCofacets_[cellLID][cofacetIndex];
    }
  else if (cellDim==1)
    {
      rtn = edgeCofacets_[cellLID][cofacetIndex];
    }
  else if (cellDim==2)
    {
      rtn = faceCofacets_[cellLID][cofacetIndex];
    }
  else
    {
      TEST_FOR_EXCEPTION(true, RuntimeError, "invalid cell dimension " << cellDim
                         << " in request for cofacet");
    }

  int dummy;
  for (int f=0; f<numFacets(spatialDim(), rtn, cellDim); f++)
    {
      if (cellLID==facetLID(spatialDim(), rtn, cellDim, f, dummy)) 
        {
          facetIndex = f;
          return rtn;
        }
    }
  TEST_FOR_EXCEPTION(true, RuntimeError, "reverse pointer to facet not found"
                     " in request for cofacet");
  return -1; // -Wall
}


void BasicSimplicialMesh::getMaxCofacetLIDs(int cellDim, 
  const Array<int>& cellLIDs,
  Array<int>& cofacetLIDs, 
  Array<int>& facetIndices) const
{
  cofacetLIDs.resize(cellLIDs.size());
  facetIndices.resize(cellLIDs.size());

  for (unsigned int i=0; i<cellLIDs.size(); i++) 
  {
    cofacetLIDs[i] = maxCofacetLID(cellDim, cellLIDs[i], 0, facetIndices[i]);
  }
}

void BasicSimplicialMesh::getCofacets(int cellDim, int cellLID,
                                      int cofacetDim, Array<int>& cofacetLIDs) const 
{
  //  TimeMonitor timer(cofacetGrabTimer());
  TEST_FOR_EXCEPTION(cofacetDim > spatialDim() || cofacetDim < 0, RuntimeError,
                     "invalid cofacet dimension=" << cofacetDim);
  TEST_FOR_EXCEPTION( cofacetDim <= cellDim, RuntimeError,
                      "invalid cofacet dimension=" << cofacetDim
                      << " for cell dim=" << cellDim);
  if (cofacetDim==spatialDim())
    {
      cofacetLIDs.resize(numMaxCofacets(cellDim, cellLID));
      int dum=0;
      for (unsigned int f=0; f<cofacetLIDs.size(); f++)
        {
          cofacetLIDs[f] = maxCofacetLID(cellDim, cellLID, f, dum);
        }
    }
  else
    {
      if (cellDim==0)
        {
          if (cofacetDim==1) cofacetLIDs = vertEdges_[cellLID];
          else if (cofacetDim==2) cofacetLIDs = vertFaces_[cellLID];
          else TEST_FOR_EXCEPT(true);
        }
      else if (cellDim==1)
        { 
          if (cofacetDim==2) cofacetLIDs = edgeFaces_[cellLID];
          else TEST_FOR_EXCEPT(true);
        }
      else if (cellDim==2)
        {
          /* this should never happen, because the only possibility is a maximal cofacet,
           * which would have been handled above */
          TEST_FOR_EXCEPT(true);
        }
      else
        {
          TEST_FOR_EXCEPT(true);
        }
    }
}

int BasicSimplicialMesh::mapGIDToLID(int cellDim, int globalIndex) const
{
  return GIDToLIDMap_[cellDim].get(globalIndex);
}

bool BasicSimplicialMesh::hasGID(int cellDim, int globalIndex) const
{
  return GIDToLIDMap_[cellDim].containsKey(globalIndex);
}

int BasicSimplicialMesh::mapLIDToGID(int cellDim, int localIndex) const
{
  return LIDToGIDMap_[cellDim][localIndex];
}

CellType BasicSimplicialMesh::cellType(int cellDim) const
{

  switch(cellDim)
    {
    case 0:
      return PointCell;
    case 1:
      return LineCell;
    case 2:
      return TriangleCell;
    case 3:
      return TetCell;
    default:
      return NullCell; // -Wall
    }
}

int BasicSimplicialMesh::label(int cellDim, 
                               int cellLID) const
{
  return labels_[cellDim][cellLID];
}

void BasicSimplicialMesh::getLabels(int cellDim, const Array<int>& cellLID, 
                                    Array<int>& labels) const
{
  labels.resize(cellLID.size());
  const Array<int>& ld = labels_[cellDim];
  for (unsigned int i=0; i<cellLID.size(); i++)
    {
      labels[i] = ld[cellLID[i]];
    }
}


Set<int> BasicSimplicialMesh::getAllLabelsForDimension(int cellDim) const
{
  Set<int> rtn;

  const Array<int>& ld = labels_[cellDim];
  for (unsigned int i=0; i<ld.size(); i++)
    {
      rtn.put(ld[i]);
    }
  
  return rtn;
}

void BasicSimplicialMesh::getLIDsForLabel(int cellDim, int label, Array<int>& cellLIDs) const
{
  cellLIDs.resize(0);
  const Array<int>& ld = labels_[cellDim];
  for (unsigned int i=0; i<ld.size(); i++)
    {
      if (ld[i] == label) cellLIDs.append(i);
    }
}




int BasicSimplicialMesh::addVertex(int globalIndex, const Point& x,
                                   int ownerProcID, int label)
{

  int lid = points_.length();
  points_.append(x);

  SUNDANCE_OUT(this->verbosity() > VerbLow,
               "BSM added point " << x << " lid = " << lid);

  numCells_[0]++;

  LIDToGIDMap_[0].append(globalIndex);
  GIDToLIDMap_[0].put(globalIndex, lid);

  if (comm().getRank() != ownerProcID) neighbors_.put(ownerProcID);
  ownerProcID_[0].append(ownerProcID);
  labels_[0].append(label);

  vertCofacets_.resize(points_.length());
  vertEdges_.resize(points_.length());
  if (spatialDim() > 2) vertFaces_.resize(points_.length());
  vertEdgePartners_.resize(points_.length());
  
  return lid;
}

int BasicSimplicialMesh::addElement(int globalIndex, 
                                    const Array<int>& vertGID,
                                    int ownerProcID, int label)
{
  SUNDANCE_VERB_HIGH("p=" << comm().getRank() << "adding element " << globalIndex 
                     << " with vertices:" << vertGID);

  /* 
   * do basic administrative steps for the new element: 
   * set LID, label, and procID; update element count. 
   */
  int lid = elemVerts_.length();
  elemEdgeSigns_.resize(lid+1);

  numCells_[spatialDim()]++;

  LIDToGIDMap_[spatialDim()].append(globalIndex);
  GIDToLIDMap_[spatialDim()].put(globalIndex, lid);
  labels_[spatialDim()].append(label);
  ownerProcID_[spatialDim()].append(ownerProcID);
  if (comm().getRank() != ownerProcID) neighbors_.put(ownerProcID);

  /* these little arrays will get used repeatedly, so make them static
   * to save a few cycles. */
  static Array<int> edges;
  static Array<int> faces;
  static Array<int> faceRotations;
  static Array<int> vertLID;

  /* find the vertex LIDs given the input GIDs */
  vertLID.resize(vertGID.size());
  for (unsigned int i=0; i<vertGID.size(); i++) 
    {
      vertLID[i] = GIDToLIDMap_[0].get(vertGID[i]);
    }
  
  /* 
   * Now comes the fun part: creating edges and faces for the 
   * new element, and registering it as a cofacet of its
   * lower-dimensional facets. 
   */
  

  if (spatialDim()==1)  
    {
      /* In 1D, there are neither edges nor faces. */
      edges.resize(0);
      faces.resize(0);
      /* register the new element as a cofacet of its vertices. */
      vertCofacets_[vertLID[0]].append(lid);
      vertCofacets_[vertLID[1]].append(lid);
    }
  if (spatialDim()==2)
    {
      /* In 2D, we need to define edges but not faces for the new element. */
      edges.resize(3);
      faces.resize(0);

      /* add the edges and define the relative orientations of the edges */
      edges[0] = addEdge(vertLID[0], vertLID[1], lid, globalIndex, 0);
      edges[1] = addEdge(vertLID[1], vertLID[2], lid, globalIndex,  1);
      edges[2] = addEdge(vertLID[2], vertLID[0], lid, globalIndex,  2);

      /* register the new element as a cofacet of its vertices. */
      vertCofacets_[vertLID[0]].append(lid);
      vertCofacets_[vertLID[1]].append(lid);
      vertCofacets_[vertLID[2]].append(lid);

    }
  else if (spatialDim()==3)
    {
      /* In 3D, we need to define edges and faces for the new element. */
      edges.resize(6);
      faces.resize(4);
      faceRotations.resize(4);

      /* add the edges and define the relative orientations of the edges */
      edges[0] = addEdge(vertLID[0], vertLID[1], lid, globalIndex,  0);
      edges[1] = addEdge(vertLID[1], vertLID[2], lid, globalIndex,  1);
      edges[2] = addEdge(vertLID[2], vertLID[0], lid, globalIndex,  2);
      edges[3] = addEdge(vertLID[0], vertLID[3], lid, globalIndex,  3);
      edges[4] = addEdge(vertLID[1], vertLID[3], lid, globalIndex,  4);
      edges[5] = addEdge(vertLID[2], vertLID[3], lid, globalIndex,  5);

      /* register the new element as a cofacet of its vertices. */
      vertCofacets_[vertLID[0]].append(lid);
      vertCofacets_[vertLID[1]].append(lid);
      vertCofacets_[vertLID[2]].append(lid);
      vertCofacets_[vertLID[3]].append(lid);

      /* add the faces and define the relative orientations of the faces */
      faces[0] = addFace(vertLID[0], vertLID[1], vertLID[3], 
                         edges[0], edges[4], edges[3], lid, globalIndex, 
                         faceRotations[0]);


      faces[1] = addFace(vertLID[1], vertLID[2], vertLID[3], 
                         edges[1], edges[5], edges[4], lid, globalIndex, 
                         faceRotations[1]);

      faces[2] = addFace(vertLID[0], vertLID[3], vertLID[2], 
                         edges[3], edges[5], edges[2], lid, globalIndex, 
                         faceRotations[2]);

      faces[3] = addFace(vertLID[2], vertLID[1], vertLID[0], 
                         edges[1], edges[0], edges[2], lid, globalIndex, 
                         faceRotations[3]);
    }

  elemVerts_.append(vertLID);
  if (edges.length() > 0) elemEdges_.append(edges);

  if (faces.length() > 0) 
    {
      elemFaces_.append(faces);
      elemFaceRotations_.append(faceRotations);
    }
  for (int i=0; i<edges.length(); i++) edgeCofacets_[edges[i]].append(lid);


  for (int i=0; i<faces.length(); i++) faceCofacets_[faces[i]].append(lid);

  return lid;
}

int BasicSimplicialMesh::addFace(int v1, int v2, int v3, 
                                 int e1, int e2, int e3,
                                 int elemLID,
                                 int elemGID,
                                 int& rotation)
{
  /* create static data for workspace arrays and pointers that will be
   * used repeatedly */
  static Array<int> sortedVertGIDs(3);
  static Array<int> reorderedVertLIDs(3);
  static Array<int> reorderedEdgeLIDs(3);
  //  static Array<int> edgeSigns(3);
  static int* sortedGIDs = &(sortedVertGIDs[0]);
  static int* reorderedLIDs = &(reorderedVertLIDs[0]);
  static int* reorderedEdges = &(reorderedEdgeLIDs[0]);

  //static int* edgeSigns = &(edgeSigns[0]);

  /* First we check whether the face already exists, and
   * along the way determine the orientation of the new element's 
   * face relative to the absolute orientation of the face. */
  int lid = checkForExistingFace(v1, v2, v3, e1, e2, e3,
                                 sortedGIDs, reorderedLIDs, reorderedEdges,
                                 rotation);

  if (lid >= 0) /* if the face already exists, we're done */
    {
      return lid;
    }
  else /* we need to register the face */
    {
      /* get a LID for the new face */
      lid = faceVertLIDs_.length();
      
      /* record the new face's vertex sets (both GID and LID, ordered by GID)
       * and its edges (reordered to the the sorted-GID orientation) */
      faceVertGIDs_.append(sortedGIDs, 3);
      faceVertLIDs_.append(reorderedLIDs, 3);
      faceEdges_.append(reorderedEdges, 3);

      SUNDANCE_VERB_EXTREME("p=" << comm().getRank() << " adding face "
                            << cellStr(3, sortedGIDs));


      /* If we own all vertices, we own the face. 
       * Otherwise, mark it for later assignment of ownership */
      int vert1Owner = ownerProcID_[0][v1];
      int vert2Owner = ownerProcID_[0][v2];
      int vert3Owner = ownerProcID_[0][v3];
      int myRank = comm().getRank();
      if (vert1Owner==myRank && vert2Owner==myRank && vert3Owner==myRank) 
        {
          ownerProcID_[2].append(myRank);
        }
      else ownerProcID_[2].append(-1);

      /* the face doesn't yet have a label, so set it to zero */
      labels_[2].append(0);
      
      /* update the view pointer to stay in synch with the resized
       * face vertex GID array */
      faceVertGIDBase_[0] = &(faceVertGIDs_.value(0,0));
      
      /* create a vertex set view that points to the new face's 
       * vertex GIDs. The first argument gives a pointer to the base of the
       * vertex GID array. The view is offset from the base by the face's
       * LID, and is of length 3. */
      VertexView face(&(faceVertGIDBase_[0]), lid, 3);
      
      /* record this vertex set in the hashtable of 
       * existing face vertex sets */
      vertexSetToFaceIndexMap_.put(face, lid);
      
      /* create space for the new face's cofacets */
      faceCofacets_.resize(lid+1);
      
      /* update the cell count */
      numCells_[spatialDim()-1]++;

      /* Register the face as a cofacet of its edges */
      edgeFaces_[e1].append(lid);
      edgeFaces_[e2].append(lid);
      edgeFaces_[e3].append(lid);

      /* Register the face as a cofacet of its vertices */
      vertFaces_[v1].append(lid);
      vertFaces_[v2].append(lid);
      vertFaces_[v3].append(lid);

      /* return the LID of the new face */
      return lid;
    }

}


void BasicSimplicialMesh::getSortedFaceVertices(int a, int b, int c,
                                                int la, int lb, int lc,
                                                int ea, int eb, int ec, 
                                                int* sortedVertGIDs,
                                                int* reorderedVertLIDs,
                                                int* reorderedEdgeLIDs,
                                                int& rotation) const
{
  /* 
   * Do a hand-coded sort of a 3-tuple of vertex GIDs, reordering tuples of vertex LIDs
   * and edge LIDs accordingly.
   */
  if (a < b)
    {
      if (c < a) 
        {
          fillSortedArray(c,a,b,sortedVertGIDs); 
          fillSortedArray(lc,la,lb,reorderedVertLIDs); 
          fillSortedArray(ec,ea,eb,reorderedEdgeLIDs); 
          rotation=3;
        }
      else if (c > b) 
        {
          fillSortedArray(a,b,c,sortedVertGIDs); 
          fillSortedArray(la,lb,lc,reorderedVertLIDs); 
          fillSortedArray(ea,eb,ec,reorderedEdgeLIDs); 
          rotation=1;
        }
      else 
        {
          fillSortedArray(a,c,b,sortedVertGIDs); 
          fillSortedArray(la,lc,lb,reorderedVertLIDs); 
          fillSortedArray(ea,ec,eb,reorderedEdgeLIDs); 
          rotation=-1;
        }
    }
  else /* b < a */
    {
      if (c < b) 
        {
          fillSortedArray(c,b,a,sortedVertGIDs); 
          fillSortedArray(lc,lb,la,reorderedVertLIDs); 
          fillSortedArray(ec,eb,ea,reorderedEdgeLIDs); 
          rotation=-3;
        }
      else if (c > a) 
        {
          fillSortedArray(b,a,c,sortedVertGIDs); 
          fillSortedArray(lb,la,lc,reorderedVertLIDs); 
          fillSortedArray(eb,ea,ec,reorderedEdgeLIDs); 
          rotation=-2;
        }
      else
        {
          fillSortedArray(b,c,a,sortedVertGIDs); 
          fillSortedArray(lb,lc,la,reorderedVertLIDs); 
          fillSortedArray(eb,ec,ea,reorderedEdgeLIDs); 
          rotation=2;
        }
    }
}


void BasicSimplicialMesh::getSortedFaceVertices(int a, int b, int c,
                                                int* sortedVertGIDs) const
{
  /* 
   * Do a hand-coded sort of a 3-tuple of vertex GIDs
   */
  if (a < b)
    {
      if (c < a) 
        {
          fillSortedArray(c,a,b,sortedVertGIDs); 
        }
      else if (c > b) 
        {
          fillSortedArray(a,b,c,sortedVertGIDs); 
        }
      else 
        {
          fillSortedArray(a,c,b,sortedVertGIDs); 
        }
    }
  else /* b < a */
    {
      if (c < b) 
        {
          fillSortedArray(c,b,a,sortedVertGIDs); 
        }
      else if (c > a) 
        {
          fillSortedArray(b,a,c,sortedVertGIDs); 
        }
      else
        {
          fillSortedArray(b,c,a,sortedVertGIDs); 
        }
    }
}


int BasicSimplicialMesh::checkForExistingEdge(int vertLID1, int vertLID2)
{
  const Array<int>& edgePartners = vertEdgePartners_[vertLID1];
  for (int i=0; i<edgePartners.length(); i++)
    {
      if (edgePartners[i] == vertLID2)
        {
          return vertEdges_[vertLID1][i];
        }
    }
  return -1;
}

int BasicSimplicialMesh::checkForExistingFace(int v1, int v2, int v3,
                                              int e1, int e2, int e3,
                                              int* sortedVertGIDs,
                                              int* reorderedVertLIDs,
                                              int* reorderedEdgeLIDs,
                                              int& rotation) 
{
  /* sort the face's vertices by global ID, reordering the vertex LIDs and
   * edge LIDs to follow. */
  int g1 = LIDToGIDMap_[0][v1];
  int g2 = LIDToGIDMap_[0][v2];
  int g3 = LIDToGIDMap_[0][v3];

  getSortedFaceVertices(g1, g2, g3, v1, v2, v3, e1, e2, e3, 
                        sortedVertGIDs, reorderedVertLIDs, reorderedEdgeLIDs,
                        rotation);

  /* see if the face is already in existence by checking for the
   * presence of the same ordered set of vertices. */
  VertexView inputFaceView(&(sortedVertGIDs), 0, 3);

  if (vertexSetToFaceIndexMap_.containsKey(inputFaceView)) 
    {
      /* return the existing face's LID */
      return vertexSetToFaceIndexMap_.get(inputFaceView);
    }
  else 
    {
      /* return -1 as an indication that the face does not yet exist */
      return -1;
    }
}

int BasicSimplicialMesh::lookupFace(int g1, int g2, int g3) 
{
  static Array<int> sortedVertGIDs(3);
  static int* sortedVertGIDPtr = &(sortedVertGIDs[0]);

  /* sort the face's vertices by global ID */
  getSortedFaceVertices(g1, g2, g3, sortedVertGIDPtr);

  /* see if the face is already in existence by checking for the
   * presence of the same ordered set of vertices. */
  VertexView inputFaceView(&(sortedVertGIDPtr), 0, 3);

  if (vertexSetToFaceIndexMap_.containsKey(inputFaceView)) 
    {
      /* return the existing face's LID */
      return vertexSetToFaceIndexMap_.get(inputFaceView);
    }
  else 
    {
      /* return -1 as an indication that the face does not yet exist */
      return -1;
    }
}
                                                
int BasicSimplicialMesh::addEdge(int v1, int v2, 
                                 int elemLID, int elemGID,
                                 int myFacetNumber)
{
  /* 
   * First we ask if this edge already exists in the mesh. If so,
   * we're done. 
   */
  int lid = checkForExistingEdge(v1, v2);

  if (lid >= 0) 
    {
      /* determine the sign of the existing edge */
      int g1 = LIDToGIDMap_[0][v1];
      int g2 = LIDToGIDMap_[0][v2];
      int edgeSign;
      if (g2 > g1) 
        {
          edgeSign = 1;
        }
      else 
        {
          edgeSign = -1;
        }
      elemEdgeSigns_.value(elemLID, myFacetNumber) = edgeSign;

      /* return the LID of the pre-existing edge */
      return lid;
    }
  else
    {
      /* get the LID of the new edge */
      lid = edgeVerts_.length();

      /* create the new edge, oriented from lower to higher vertex GID */
      int g1 = LIDToGIDMap_[0][v1];
      int g2 = LIDToGIDMap_[0][v2];
      int edgeSign;
      if (g2 > g1) 
        {
          edgeVerts_.append(tuple(v1,v2));
          edgeSign = 1;
        }
      else 
        {
          edgeVerts_.append(tuple(v2,v1));
          edgeSign = -1;
        }

      /* if we own all vertices, we own the edge. Otherwise, mark it
       * for later assignment. */
      int vert1Owner = ownerProcID_[0][v1];
      int vert2Owner = ownerProcID_[0][v2];
      if (vert2Owner==comm().getRank() && vert1Owner==comm().getRank()) 
        ownerProcID_[1].append(vert1Owner);
      else ownerProcID_[1].append(-1);

      elemEdgeSigns_.value(elemLID, myFacetNumber) = edgeSign;

      /* register the new edge with its vertices */
      vertEdges_[v1].append(lid);
      vertEdgePartners_[v1].append(v2);
      vertEdges_[v2].append(lid);
      vertEdgePartners_[v2].append(v1);
      /* create storage for the cofacets of the new edge */
      edgeCofacets_.resize(lid+1);
      if (spatialDim() > 2) edgeFaces_.resize(lid+1);
      
      
      /* the new edge is so far unlabeled, so set its label to zero */
      labels_[1].append(0);
      
      /* update the edge count */
      numCells_[1]++;
    }
  /* return the LID of the edge */
  return lid;
}


void BasicSimplicialMesh::synchronizeGIDNumbering(int dim, int localCount) 
{
  
  SUNDANCE_OUT(this->verbosity() > VerbMedium, 
               "sharing offsets for GID numbering for dim=" << dim);
  SUNDANCE_OUT(this->verbosity() > VerbMedium, 
               "I have " << localCount << " cells");
  Array<int> gidOffsets;
  int total;
  MPIContainerComm<int>::accumulate(localCount, gidOffsets, total, comm());
  int myOffset = gidOffsets[comm().getRank()];

  SUNDANCE_OUT(this->verbosity() > VerbMedium, 
               "back from MPI accumulate: offset for d=" << dim
               << " on p=" << comm().getRank() << " is " << myOffset);

  for (int i=0; i<numCells(dim); i++)
    {
      if (LIDToGIDMap_[dim][i] == -1) continue;
      LIDToGIDMap_[dim][i] += myOffset;
      GIDToLIDMap_[dim].put(LIDToGIDMap_[dim][i], i);
    }
  SUNDANCE_OUT(this->verbosity() > VerbMedium, 
               "done sharing offsets for GID numbering for dim=" << dim);
}


void BasicSimplicialMesh::assignIntermediateCellGIDs(int cellDim)
{
  int myRank = comm().getRank();
  SUNDANCE_VERB_MEDIUM("p=" << myRank << " assigning " << cellDim 
                       << "-cell owners");

  /* If we are debugging parallel code, we may want to stagger the output
   * from the different processors. This is done by looping over processors,
   * letting one do its work while the others are idle; a synchronization
   * point after each iteration forces this procedure to occur such
   * that the processors' output is staggered. 
   *
   * If we don't want staggering (as will usually be the case in production
   * runs) numWaits should be set to 1. 
   */  
  int numWaits = 1;
  if (staggerOutput())
    {
      SUNDANCE_VERB_LOW("p=" << myRank << " doing staggered output in "
                        "assignIntermediateCellOwners()");
      numWaits = comm().getNProc();
    }
  

  /* list of vertex GIDs which serve to identify the cells whose
   * GIDs are needed */
  Array<Array<int> > outgoingRequests(comm().getNProc());
  Array<Array<int> > incomingRequests(comm().getNProc());

  /* lists of cells for which GIDs are needed from each processor */
  Array<Array<int> > outgoingRequestLIDs(comm().getNProc());

  /* lists of GIDs to be communicated. */
  Array<Array<int> > outgoingGIDs(comm().getNProc());
  Array<Array<int> > incomingGIDs(comm().getNProc());

  /* Define these so we're not constantly dereferencing the owners arrays */
  Array<int>& cellOwners = ownerProcID_[cellDim];

  /* run through the cells, assigning GID numbers to the locally owned ones
   * and compiling a list of the remotely owned ones. */
  int localCount = 0;
  ArrayOfTuples<int>* cellVerts;
  if (cellDim==2) cellVerts = &(faceVertLIDs_);
  else cellVerts = &(edgeVerts_);

  LIDToGIDMap_[cellDim].resize(numCells(cellDim));
  
  SUNDANCE_OUT(this->verbosity() > VerbMedium,  
               "starting loop over cells");


  /*
   * We assign edge owners as follows: 
   *
   * (1) Any edge whose vertices are owned by the same proc is owned
   * by that proc.  This can be done at the time of construction of
   * the edge.
   *
   * (2) For edges not resolved by step 1, we send a specification of
   * that edge to all processors neighboring this one. Each processor
   * then returns either its processor rank or -1 for each edge
   * specification it receives, according to whether or not both
   * vertex GIDs exist on that processor.
   *
   * (3) The ranks of all processors seeing each disputed edge having
   * been collected in step 2, the edge is assigned to the processor
   * with the largest rank.
   */
  
  try
    {
      resolveEdgeOwnership(cellDim);
    }
  catch(std::exception& e0)
    {
      SUNDANCE_TRACE_MSG(e0, "while resolving edge ownership");
    }

  
  for (int q=0; q<numWaits; q++)
    {
      if (numWaits > 1 && q != myRank) 
        {
          TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                             "off-proc error detected on proc=" << myRank
                             << " while computing GID resolution requests");
          continue;
        }
      try
        {
          for (int i=0; i<numCells(cellDim); i++)
            {  
              SUNDANCE_OUT(this->verbosity() > VerbHigh, 
                           "p=" << myRank 
                           <<" cell " << i);
              
              /* If the cell is locally owned, give it a temporary
               * GID. This GID is temporary because it's one of a
               * sequence numbered from zero rather than the lowest
               * GID on this processor. We'll offset all of these GIDs
               * once we know how many cells this processor owns. */
              int owner = cellOwners[i];
              if (owner==myRank)
                {
                  SUNDANCE_VERB_EXTREME("p=" << myRank 
                                        << " assigning GID=" << localCount
                                        << " to cell " 
                                        << cellToStr(cellDim, i));
                  LIDToGIDMap_[cellDim][i] = localCount++;
                }
              else /* If the cell is remotely owned, we can't give it
                    * a GID now. Give it a GID of -1 to mark it as
                    * unresolved, and then add it to a list of cells for
                    * which we'll need remote GIDs. To get a GID, we have
                    * to look up this cell on another processor.  We can't
                    * use the LID to identify the cell, since other procs
                    * won't know our LID numbering. Thus we send the GIDs
                    * of the cell's vertices, which are sufficient to
                    * identify the cell on the other processor. */
                {
                  LIDToGIDMap_[cellDim][i] = -1;
                  SUNDANCE_OUT(this->verbosity() > VerbHigh, 
                               "p=" << myRank << " adding cell LID=" << i 
                               << " to req list");
                  outgoingRequestLIDs[owner].append(i);
                  for (int v=0; v<=cellDim; v++)
                    {
                      int ptLID = cellVerts->value(i, v);
                      int ptGID = LIDToGIDMap_[0][ptLID];
                      SUNDANCE_OUT(this->verbosity() > VerbHigh, 
                                   "p=" << myRank << " adding pt LID=" << ptLID
                                   << " GID=" << ptGID
                                   << " to req list");
                      outgoingRequests[owner].append(ptGID);
                    }
                  SUNDANCE_VERB_EXTREME("p=" << myRank 
                                        << " deferring GID assignment for cell "
                                        << cellToStr(cellDim, i));
                }
            }
        }
      catch(std::exception& e0)
        {
          reportFailure(comm());
          SUNDANCE_TRACE_MSG(e0, "while computing GID resolution requests");
        }
      TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                         "off-proc error detected on proc=" << myRank
                         << " while computing GID resolution requests");
    }

  /* Now that we know how many cells are locally owned, we can 
   * set up a global GID numbering */
  try
    {
      synchronizeGIDNumbering(cellDim, localCount);
    }
  catch(std::exception& e0)
    {
      reportFailure(comm());
      SUNDANCE_TRACE_MSG(e0, "while computing GID offsets");
    }
  TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                     "off-proc error detected on proc=" << myRank
                     << " while computing GID offsets");
  
  /* We now share requests for cells for which remote GIDs are needed */
  for (int q=0; q<numWaits; q++)
    {
      if (numWaits > 1) comm().synchronize();
      if (numWaits > 1 && q!=myRank) continue;
      SUNDANCE_OUT(this->verbosity() > VerbHigh,
                   "p=" << myRank << "sending requests: " 
                   << outgoingRequests);
    }

  try
    {
      MPIContainerComm<int>::allToAll(outgoingRequests,
                                      incomingRequests,
                                      comm()); 
    }
  catch(std::exception& e0)
    {
      reportFailure(comm());
      SUNDANCE_TRACE_MSG(e0, "while distributing GID resolution requests");
    }
  TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                     "off-proc error detected on proc=" << myRank
                     << " while distributing GID resolution requests");


  for (int q=0; q<numWaits; q++)
    {
      if (numWaits > 1) comm().synchronize();
      if (numWaits > 1 && q!=myRank) continue;
      SUNDANCE_OUT(this->verbosity() > VerbHigh,
                   "p=" << myRank << "recv'd requests: " << incomingRequests);
    }


  for (int q=0; q<numWaits; q++)
    {
      if (numWaits>1 && q != myRank) 
        {
          TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                             "off-proc error detected on proc=" << myRank
                             << " while computing edge/face GID responses");
          continue;
        }
      try
        {
          /* Answer the requests incoming to this processor */
          for (int p=0; p<comm().getNProc(); p++)
            {
              const Array<int>& requestsFromProc = incomingRequests[p];
              
              for (unsigned int c=0; c<requestsFromProc.size(); c+=(cellDim+1))
                {
                  SUNDANCE_OUT(this->verbosity() > VerbHigh,
                               "p=" << myRank << "processing request c=" 
                               << c/(cellDim+1));
                  /* The request for each cell is a tuple of vertex GIDs. 
                   * Find the LID of the local cell that contains them */
                  int cellLID;
                  if (cellDim==1) 
                    {
                      int vertLID1 = mapGIDToLID(0, requestsFromProc[c]);
                      int vertLID2 = mapGIDToLID(0, requestsFromProc[c+1]);
                      SUNDANCE_VERB_HIGH("p=" << myRank 
                                         << " edge vertex GIDs are "
                                         <<  requestsFromProc[c+1]
                                         << ", " << requestsFromProc[c]);
                      cellLID = checkForExistingEdge(vertLID1, vertLID2);
                    }
                  else
                    {
                      
                      SUNDANCE_VERB_HIGH("p=" << myRank 
                                         << " face vertex GIDs are "
                                         <<  requestsFromProc[c]
                                         << ", " << requestsFromProc[c+1]
                                         << ", " << requestsFromProc[c+2]);
                      cellLID = lookupFace(requestsFromProc[c],
                                           requestsFromProc[c+1],
                                           requestsFromProc[c+2]);
                    }
                  SUNDANCE_VERB_HIGH("p=" << myRank << "cell LID is " 
                                     << cellLID);
                  
                  TEST_FOR_EXCEPTION(cellLID < 0, RuntimeError,
                                     "unable to find requested cell "
                                     << cellStr(cellDim+1, &(requestsFromProc[c])));
                  
                  /* Finally, we get the cell's GID and append to the list 
                   * of GIDs to send back as answers. */
                  int cellGID = mapLIDToGID(cellDim, cellLID);
                  TEST_FOR_EXCEPTION(cellGID < 0, RuntimeError,
                                     "proc " << myRank 
                                     << " was asked by proc " << p
                                     << " to provide a GID for cell "
                                     << cellStr(cellDim+1, &(requestsFromProc[c]))
                                     << " with LID=" << cellLID
                                     << " but its GID is undefined");
                  
                  outgoingGIDs[p].append(mapLIDToGID(cellDim, cellLID));
                }
            }
        }
      catch(std::exception& e0)
        {
          reportFailure(comm());
          SUNDANCE_TRACE_MSG(e0, "while computing edge/face GID responses");
        }
      TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                         "off-proc error detected "
                         "on proc=" << myRank
                         << " while computing edge/face GID responses");
    }


  /* We now share the GIDs for the requested cells */
  for (int q=0; q<numWaits; q++)
    {
      if (numWaits>1) comm().synchronize();
      if (numWaits>1 && q!=myRank) continue;
      SUNDANCE_OUT(this->verbosity() > VerbHigh,
                   "p=" << myRank << "sending GIDs: " << outgoingGIDs);
    }

  
  try
    {
      MPIContainerComm<int>::allToAll(outgoingGIDs,
                                      incomingGIDs,
                                      comm());
    }
  catch(std::exception& e0)
    {
      reportFailure(comm());
      SUNDANCE_TRACE_MSG(e0, "while distributing edge/face GIDs");
    }
  TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                     "off-proc error detected on proc=" << myRank
                     << " while distributing edge/face GIDs");
  



  for (int q=0; q<numWaits; q++)
    {
      if (numWaits > 1 && q != myRank) 
        {
          TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                             "off-proc error detected on proc=" << myRank
                             << " while setting remote edge/face GIDs");
          continue;
        }
      try
        {
          SUNDANCE_OUT(this->verbosity() > VerbHigh,
                       "p=" << myRank << "recv'ing GIDs: " << incomingGIDs);
          
          /* Now assign the cell GIDs we've recv'd from from the other procs */
          for (int p=0; p<comm().getNProc(); p++)
            {
              const Array<int>& gidsFromProc = incomingGIDs[p];
              for (unsigned int c=0; c<gidsFromProc.size(); c++)
                {
                  int cellLID = outgoingRequestLIDs[p][c];
                  int cellGID = incomingGIDs[p][c];
                  SUNDANCE_OUT(this->verbosity() > VerbHigh,
                               "p=" << myRank << 
                               " assigning GID=" << cellGID << " to LID=" 
                               << cellLID);
                  LIDToGIDMap_[cellDim][cellLID] = cellGID;
                  GIDToLIDMap_[cellDim].put(cellGID, cellLID);
                }
            }
          

          SUNDANCE_OUT(this->verbosity() > VerbMedium,  
                       "p=" << myRank 
                       << "done synchronizing cells of dimension " 
                       << cellDim);
        }
      catch(std::exception& e0)
        {
          reportFailure(comm());
          SUNDANCE_TRACE_MSG(e0, " while setting remote edge/face GIDs");
        }
      TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                         "off-proc error detected on proc=" << myRank
                         << " while setting remote edge/face GIDs");
    }
  if (cellDim==1) hasEdgeGIDs_ = true;
  else hasFaceGIDs_ = true;
}







void BasicSimplicialMesh::synchronizeNeighborLists()
{
  if (neighborsAreSynchronized_) return ;

  Array<Array<int> > outgoingNeighborFlags(comm().getNProc());
  Array<Array<int> > incomingNeighborFlags(comm().getNProc());

  int myRank = comm().getRank();

  for (int p=0; p<comm().getNProc(); p++)
    {
      if (neighbors_.contains(p)) outgoingNeighborFlags[p].append(myRank);
    }

  try
    {
      MPIContainerComm<int>::allToAll(outgoingNeighborFlags,
                                      incomingNeighborFlags,
                                      comm()); 
    }
  catch(std::exception& e0)
    {
      reportFailure(comm());
      SUNDANCE_TRACE_MSG(e0, "while distributing neighbor information");
    }
  TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                     "off-proc error detected on proc=" << myRank
                     << " while distributing neighbor information");

  

  for (int p=0; p<comm().getNProc(); p++)
    {
      if (incomingNeighborFlags[p].size() > 0) neighbors_.put(p);
    }

  neighborsAreSynchronized_ = true;
}




void BasicSimplicialMesh::resolveEdgeOwnership(int cellDim) 
{
  int myRank = comm().getRank();
  SUNDANCE_VERB_LOW("p=" << myRank << " finding " 
                    << cellDim << "-cell owners");


  /* If we are debugging parallel code, we may want to stagger the output
   * from the different processors. This is done by looping over processors,
   * letting one do its work while the others are idle; a synchronization
   * point after each iteration forces this procedure to occur such
   * that the processors' output is staggered. 
   *
   * If we don't want staggering (as will usually be the case in production
   * runs) numWaits should be set to 1. 
   */  
  int numWaits = 1;
  if (staggerOutput())
    {
      SUNDANCE_VERB_LOW("p=" << myRank << " doing staggered output in "
                        "assignIntermediateCellOwners()");
      numWaits = comm().getNProc();
    }

  synchronizeNeighborLists();

  /* Define some helper variables to save us from lots of dereferencing */
  Array<int>& cellOwners = ownerProcID_[cellDim];
  ArrayOfTuples<int>* cellVerts;
  if (cellDim==2) cellVerts = &(faceVertLIDs_);
  else cellVerts = &(edgeVerts_);

  /* Dump neighbor set into an array to simplify looping */
  Array<int> neighbors = neighbors_.elements();
  SUNDANCE_VERB_LOW("p=" << myRank << " neighbors are " << neighbors);
  


  /*
   * In 3D it is not possible to assign all edge owners without communication. 
   * We assign edge owners as follows:
   * (1) Any edge whose vertices are owned by the same proc is owned by 
   *     that proc. This can be done at the time of construction of the edge. 
   * (2) For edges not resolved by step 1, we send a specification of that edge
   *     to all processors neighboring this one. Each processor then returns 
   *     either its processor rank or -1 for each edge specification it 
   *     receives, according to whether or not both vertex GIDs exist 
   *     on that processor. 
   * (3) The ranks of all processors seeing each disputed edge having been 
   *     collected in step 2, the edge is assigned to the processor 
   *     with the largest rank.
   */


      
  /* the index to LID map will let us remember the LIDs of edges
   * we've sent off for owner information */
  Array<int> reqIndexToEdgeLIDMap;

  /* compile lists of specifiers to be communicated to neighbors */
  Array<Array<int> > outgoingEdgeSpecifiers(comm().getNProc());
  Array<int> vertLID(cellDim+1);
  Array<int> vertGID(cellDim+1);

  for (int q=0; q<numWaits; q++)
    {
      if (numWaits > 1 && q != myRank) 
        {
          TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                             "off-proc error detected on proc=" << myRank
                             << " while determining edges to be resolved");
          continue;
        }
      try
        {
          for (int i=0; i<numCells(cellDim); i++)
            {  
              int owner = cellOwners[i];
              /* if we don't know the owner of the edge, send
               * information about the edge to all neighbors so we can
               * come to a consensus on the ownership of the edge
               */
              if (owner==-1)
                {
                  /* The specification of the edge is the set of 
                   * vertices defining the edge. */
                  for (int v=0; v<=cellDim; v++) 
                    {
                      vertLID[v] = cellVerts->value(i, v);
                      vertGID[v] = LIDToGIDMap_[0][vertLID[v]];
                    }
                  /* make a request to all neighbors */
                  for (unsigned int n=0; n<neighbors.size(); n++)
                    {
                      for (int v=0; v<=cellDim; v++)
                        {
                          outgoingEdgeSpecifiers[neighbors[n]].append(vertGID[v]);
                        }
                    }
                  SUNDANCE_VERB_HIGH("p=" << myRank 
                                     << " is asking all neighbors to resolve edge "
                                     << cellToStr(cellDim, i));

                  /* Remember the LIDs of the edges about whom we're requesting
                   * information. */
                  reqIndexToEdgeLIDMap.append(i);
                }
            }

          SUNDANCE_VERB_MEDIUM("p=" << myRank 
                               << " initially unresolved edge LIDs are "
                               << reqIndexToEdgeLIDMap);

          SUNDANCE_VERB_HIGH("p=" << myRank 
                             << " sending outgoing edge specs:") ;
          for (int p=0; p<comm().getNProc(); p++)
            {
              SUNDANCE_VERB_HIGH(outgoingEdgeSpecifiers[p].size()/(cellDim+1) 
                                 << " edge reqs to proc "<< p) ;
            }
          SUNDANCE_VERB_HIGH("p=" << myRank << " outgoing edge specs are " 
                             << outgoingEdgeSpecifiers);
        }
      catch(std::exception& e0)
        {
          reportFailure(comm());
          SUNDANCE_TRACE_MSG(e0, "while determining edges to be resolved");
        }
      TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                         "off-proc error detected on proc=" << myRank
                         << " while determining edges to be resolved");
    }
  


  /* share the unresolved edges with my neighbors */
  Array<Array<int> > incomingEdgeSpecifiers(comm().getNProc());


  try
    {
      MPIContainerComm<int>::allToAll(outgoingEdgeSpecifiers,
                                      incomingEdgeSpecifiers,
                                      comm()); 
    }
  catch(std::exception& e0)
    {
      reportFailure(comm());
      SUNDANCE_TRACE_MSG(e0, "while distributing edge resolution reqs");
    }
  TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                     "off-proc error detected on proc=" << myRank
                     << " while distributing edge resolution reqs");
    

  Array<Array<int> > outgoingEdgeContainers(comm().getNProc());    
  for (int q=0; q<numWaits; q++)
    {

      if (numWaits > 1 && q != myRank) 
        {
          TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                             "off-proc error detected on proc=" << myRank
                             << " while resolving edge ownership results");
          continue;
        }
      
      try
        {
          SUNDANCE_VERB_HIGH("p=" << myRank << " incoming edge specs are " 
                             << incomingEdgeSpecifiers);
          /* Now, put on my receiver hat. I have received from each
           * other processor  an array of vertex GID pairs, each of
           * which may or may not exist on this processor. I need to
           * send back to my neighbors an indicator of whether or not I
           * know about both vertices defining each edge.
           */
          for (int p=0; p<comm().getNProc(); p++)
            {
              const Array<int>& edgesFromProc = incomingEdgeSpecifiers[p];
              
              int numVerts = cellDim+1;
              for (unsigned int c=0; c<edgesFromProc.size(); c+=numVerts)
                {
                  SUNDANCE_VERB_LOW("p=" << myRank << " doing c=" << c/numVerts
                                    << " of " << edgesFromProc.size()/numVerts
                                    << " reqs from proc=" << p);
                  /* The request for each cell is a tuple of vertex
                   * GIDs. Find out whether this processor knows about
                   * all of them. If this processor contains knows
                   * both vertices of the edge and the vertex pair
                   * forms an edge, add my rank to the pool of
                   * potential edge owners to be returned. Otherwise,
                   * mark with -1. */

                  int cellLID;
                  SUNDANCE_VERB_LOW("p=" << myRank << " looking at "
                                    << cellStr(numVerts, &(edgesFromProc[c])));
                  /* The response will be:
                   * (*) -1 if the proc doesn't know the edge
                   * (*) 0  if the proc knows the edge, but doesn't know who owns it
                   * (*) 1  if the proc owns it
                   */
                  int response = 0;
                  /* See if all verts are present on this proc. Respond with -1 of not. */
                  for (int v=0; v<numVerts; v++)
                    {
                      vertGID[v] = edgesFromProc[c+v];
                      if (!GIDToLIDMap_[0].containsKey(vertGID[v])) 
                        {
                          response = -1;
                          break;
                        }
                      else
                        {
                          vertLID[v] = GIDToLIDMap_[0].get(vertGID[v]);
                        }
                    }
                  if (response != -1)
                    {
                      /* Make sure the vertices are actually organized into an edge
                       * on this processor */
                      if (cellDim==1)
                        {
                          cellLID = checkForExistingEdge(vertLID[0], vertLID[1]);
                        }
                      else
                        {
                          cellLID = lookupFace(vertGID[0], vertGID[1], vertGID[2]);
                        }
                      /* Respond with -1 if the edge/face doesn't exist on this proc */
                      if (cellLID==-1)
                        {
                          response = -1;
                        }
                    }

                  if (response == -1)
                    {
                      SUNDANCE_VERB_HIGH("p=" << myRank << " is unaware of cell "
                                         << cellStr(numVerts, &(vertGID[0])));
                    } 
                  else
                    {
                      SUNDANCE_VERB_HIGH("p=" << myRank << " knows about cell "
                                         << cellStr(numVerts, &(vertGID[0])));
                      if (ownerProcID_[cellDim][cellLID] != -1) response = 1;
                    }
                  /* add the response to the data stream to be returned */
                  outgoingEdgeContainers[p].append(response);
                  SUNDANCE_VERB_LOW("p=" << myRank << " did c=" << c/numVerts
                                    << " of " << edgesFromProc.size()/numVerts 
                                    << " reqs from proc=" << p);
                }
              SUNDANCE_VERB_LOW("p=" << myRank << " done all reqs from proc "
                                << p);
          
            }
          SUNDANCE_VERB_LOW("p=" << myRank << " done processing edge reqs");
          SUNDANCE_VERB_HIGH("p=" << myRank 
                             << " outgoing edge containers are " 
                             << outgoingEdgeContainers);
        }
      catch(std::exception& e0)
        {
          reportFailure(comm());
          SUNDANCE_TRACE_MSG(e0, "while resolving edge ownership results");
        }
      TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                         "off-proc error detected on proc=" << myRank
                         << " while resolving edge ownership results");
    }
  

  
  /* share the information about which edges I am aware of */
  
  Array<Array<int> > incomingEdgeContainers(comm().getNProc());
  try
    {
      MPIContainerComm<int>::allToAll(outgoingEdgeContainers,
                                      incomingEdgeContainers,
                                      comm()); 
    }
  catch(std::exception& e0)
    {
      reportFailure(comm());
      SUNDANCE_TRACE_MSG(e0, "while distributing edge resolution results");
    }
  TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                     "off-proc error detected on proc=" << myRank
                     << " while distributing edge ownership results");

  



  /* Process the edge data incoming to this processor */
  for (int q=0; q<numWaits; q++)
    {
      if (numWaits > 1 && q != myRank)
        {
          TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                             "off-proc error detected on proc=" << myRank
                             << " while finalizing edge ownership");
          continue;
        }

      try
        {
          SUNDANCE_VERB_HIGH("p=" << myRank 
                             << " incoming edge containers are " 
                             << incomingEdgeContainers);
          Set<int> resolved;
          for (int p=0; p<comm().getNProc(); p++)
            {
              const Array<int>& edgeContainers = incomingEdgeContainers[p];
              
              for (unsigned int c=0; c<edgeContainers.size(); c++)
                {
                  int edgeLID = reqIndexToEdgeLIDMap[c];
                  /* If the state of the edge is unknown, 
                   * update the owner to the largest rank of all procs 
                   * containing this edge */
                  if (edgeContainers[c] == 0 && !resolved.contains(edgeLID)) 
                    {
                      /* because p is increasing in the loop, p is
                       * equivalent to max(cellOwners[edgeLID], p) */
                      cellOwners[edgeLID] = p;
                    }
                  /* If one of the processors reports it owns the edge, set
                   * the owner and mark it as resolved so that it won't be modified 
                   * further */
                  else if (edgeContainers[c]==1)
                    {
                      cellOwners[edgeLID] = p;
                      resolved.put(edgeLID);
                    }
                }
            }
          /* Any remaining unresolved edges appear only on this processor, so 
           * I own them. */
          for (unsigned int c=0; c<reqIndexToEdgeLIDMap.size(); c++)
            {
              int edgeLID = reqIndexToEdgeLIDMap[c];
              if (cellOwners[edgeLID] < 0) cellOwners[edgeLID] = myRank;
              SUNDANCE_VERB_EXTREME("p=" << myRank 
                                    << " has decided cell "
                                    << cellToStr(cellDim, edgeLID) 
                                    << " is owned by proc=" 
                                    << cellOwners[edgeLID]);
            }
          SUNDANCE_VERB_LOW("p=" << myRank << " done finalizing ownership"); 
        }
      catch(std::exception& e0)
        {
          reportFailure(comm());
          SUNDANCE_TRACE_MSG(e0, "while finalizing edge ownership");
        }
      TEST_FOR_EXCEPTION(checkForFailures(comm()), RuntimeError, 
                         "off-proc error detected on proc=" << myRank
                         << " while finalizing edge ownership");
    }
}

string BasicSimplicialMesh::cellStr(int dim, const int* verts) const
{
  string rtn="(";
  for (int i=0; i<dim; i++)
    {
      if (i!=0) rtn += ", ";
      rtn += Teuchos::toString(verts[i]);
    }
  rtn += ")";
  return rtn;
}

string BasicSimplicialMesh::cellToStr(int cellDim, int cellLID) const
{
  TeuchosOStringStream os;

  if (cellDim > 0)
    {
      const ArrayOfTuples<int>* cellVerts;
      if (cellDim==spatialDim()) 
        {
          cellVerts = &(elemVerts_);
        }
      else
        {
          if (cellDim==2) cellVerts = &(faceVertLIDs_);
          else cellVerts = &(edgeVerts_);
        }
      os << "(";
      for (int j=0; j<cellVerts->tupleSize(); j++)
        {
          if (j!=0) os << ", ";
          os << mapLIDToGID(0, cellVerts->value(cellLID,j));
        }
      os << ")" << std::endl;
    }
  else
    {
      os << LIDToGIDMap_[0][cellLID] << std::endl;
    }
  return os.str();
}

string BasicSimplicialMesh::printCells(int cellDim) const
{
  TeuchosOStringStream os;

  if (cellDim > 0)
    {
      const ArrayOfTuples<int>* cellVerts;
      if (cellDim==spatialDim()) 
        {
          cellVerts = &(elemVerts_);
        }
      else
        {
          if (cellDim==2) cellVerts = &(faceVertLIDs_);
          else cellVerts = &(edgeVerts_);
        }

      os << "printing cells for processor " << comm().getRank() << std::endl;
      for (int i=0; i<cellVerts->length(); i++)
        {
          os << i << " (";
          for (int j=0; j<cellVerts->tupleSize(); j++)
            {
              if (j!=0) os << ", ";
              os << mapLIDToGID(0, cellVerts->value(i,j));
            }
          os << ")" << std::endl;
        }
    }
  else
    {
      os << LIDToGIDMap_[0] << std::endl;
    }
  
  return os.str();
}



