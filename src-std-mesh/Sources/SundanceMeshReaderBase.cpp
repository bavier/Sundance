#include "SundanceMeshReaderBase.hpp"
#include "SundanceExceptions.hpp"
#include "SundanceOut.hpp"

using namespace SundanceStdMesh;
using namespace SundanceStdMesh::Internal;
using namespace TSFExtended;
using namespace Teuchos;
using namespace SundanceUtils;

int MeshReaderBase::atoi(const std::string& x) const 
{
#ifndef TFLOP
  return std::atoi(x.c_str());
#else
  return ::atoi(x.c_str());
#endif
}

double MeshReaderBase::atof(const std::string& x) const 
{
#ifndef TFLOP
  return std::atof(x.c_str());
#else
  return ::atof(x.c_str());
#endif
}

bool MeshReaderBase::isEmptyLine(const std::string& x) const 
{
  return x.length()==0 || StrUtils::isWhite(x);
}

void MeshReaderBase::getNextLine(istream& is, string& line,
                                         Array<string>& tokens,
                                         char comment) const 
{
  while (StrUtils::readLine(is, line))
    {
      SUNDANCE_OUT(verbosity() == VerbHigh,
                   "read line [" << line << "]");

      if (line.length() > 0) line = StrUtils::before(line,comment);
      if (isEmptyLine(line)) continue;
      if (line.length() > 0) break;
    }
  tokens = StrUtils::stringTokenizer(line);
}

RefCountPtr<ifstream> MeshReaderBase::openFile(const string& fname, 
                                               const string& description) const
{
  RefCountPtr<ifstream> rtn = rcp(new ifstream(fname.c_str()));

  SUNDANCE_OUT(verbosity() == VerbHigh,
               "trying to open " << description << " file " << fname);

  TEST_FOR_EXCEPTION(rtn.get()==0 || *rtn==0, RuntimeError, 
                     "MeshReaderBase::openFile() unable to open "
                     << description << " file " << fname);

  SUNDANCE_OUT(verbosity() > VerbSilent,
               "reading " << description << " from " << fname);

  return rtn;
}
