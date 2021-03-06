/* @HEADER@ */
// ************************************************************************
// 
//                             Sundance
//                 Copyright 2011 Sandia Corporation
// 
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Kevin Long (kevin.long@ttu.edu)
// 

/* @HEADER@ */

#ifndef SUNDANCE_CELLFILTERBASE_H
#define SUNDANCE_CELLFILTERBASE_H

#include "SundanceDefs.hpp"
#include "SundanceCellFilterStub.hpp"
#include "SundanceCellFilter.hpp"
#include "SundanceCellSet.hpp"
#include "SundanceMap.hpp"
#include "PlayaHandleable.hpp"
#include "PlayaPrintable.hpp"
#include "Teuchos_Describable.hpp"

namespace Sundance
{
using namespace Teuchos;
  
/** 
 * Base class for CellFilter objects.
 *
 * <h4> Notes for subclass implementors </h4>
 * 
 * Derived classes must implement the methods
 * <ul>
 * <li> internalGetCells() -- returns the set of cells that 
 * pass through this filter
 * <li> dimension() -- returns the dimension of the cells that
 * will pass through this filter
 * <li> toXML() -- writes an XML description of the filter
 * <li> lessThan() -- compares to another cell filter. Used to store
 * cell filters in STL containers. 
 * <li> typeName() -- returns the name of the subclass. Used in ordering.
 * </ul>
 */
class CellFilterBase : public CellFilterStub
{
public:
  /** Empty ctor */
  CellFilterBase();

  /** virtual dtor */
  virtual ~CellFilterBase();

  /** Find the cells passing this filter on the given mesh. This
   * method will cache the cell sets it computes for each mesh  */
  CellSet getCells(const Mesh& mesh) const ;

  /** Return the dimension of the cells that will be identified
   * by this filter when acting on the given mesh */
  virtual int dimension(const Mesh& mesh) const = 0 ;

  /** */
  void registerSubset(const CellFilter& sub) const ;

  /** */
  void registerLabeledSubset(int label, const CellFilter& sub) const ;

  /** */
  void registerDisjoint(const CellFilter& sub) const ;

  /** */
  const Set<CellFilter>& knownSubsets() const ;

  /** */
  const Set<CellFilter>& knownDisjoints() const ;

  /** */
  virtual std::string toString() const {return name_;}

  /** Print to a stream */
  virtual std::string description() const 
    {return toString();}

  /** */
  void setName(const std::string& name) {name_ = name;}

  /** empties the cache of the filter */
  void flushCache() const;

protected:

  /** */
  virtual CellSet internalGetCells(const Mesh& mesh) const = 0 ;

private:
  /** cache of previously computed cell sets */
  mutable Sundance::Map<int, CellSet> cellSetCache_;

  /** */
  std::string name_;

};
}


#endif
