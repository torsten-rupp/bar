/***********************************************************************\
*
* $Revision: 3576 $
* $Date: 2015-01-28 02:59:57 +0100 (Wed, 28 Jan 2015) $
* $Author: torsten $
* Contents: Backup ARchiver crash dump utility
* Systems: Linux
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdio.h>
#include <string.h>

#include <string>
#include <iostream>
#include <fstream>

#include "global.h"

#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/minidump_processor.h"
#include "google_breakpad/processor/process_state.h"
#include "processor/logging.h"
#include "google_breakpad/processor/symbol_supplier.h"
#include "processor/stackwalk_common.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

using google_breakpad::BasicSourceLineResolver;
using google_breakpad::Minidump;
using google_breakpad::MinidumpProcessor;
using google_breakpad::ProcessState;
using google_breakpad::SymbolSupplier;
using google_breakpad::CodeModule;
using google_breakpad::PrintProcessState;

namespace google_breakpad
{
  class SimpleSymbolSupplier : public SymbolSupplier
  {
    public:
      explicit SimpleSymbolSupplier(const string &symbolFileName) :
        mSymbolFileName(symbolFileName),
        mSymbolData(NULL)
      {
      }

      virtual ~SimpleSymbolSupplier()
      {
      }

      virtual SymbolResult GetSymbolFile(const CodeModule *module,
                                         const SystemInfo *system_info,
                                         string *symbol_file);

      virtual SymbolResult GetSymbolFile(const CodeModule *module,
                                         const SystemInfo *system_info,
                                         string *symbol_file,
                                         string *symbol_data);

      virtual SymbolResult GetCStringSymbolData(const CodeModule *module,
                                                const SystemInfo *system_info,
                                                string *symbol_file,
                                                char **symbol_data,
                                                size_t *symbol_data_size);

      virtual void FreeSymbolData(const CodeModule *module);

    private:
      string mSymbolFileName;
      char   *mSymbolData;
  };

  SymbolSupplier::SymbolResult SimpleSymbolSupplier::GetSymbolFile(const CodeModule *codeModule,
                                                                const SystemInfo *systemInfo,
                                                                string *symbolFileName)
  {
    UNUSED_VARIABLE(codeModule);
    UNUSED_VARIABLE(systemInfo);
    UNUSED_VARIABLE(symbolFileName);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

    return SymbolSupplier::INTERRUPT;
  }

  SymbolSupplier::SymbolResult SimpleSymbolSupplier::GetSymbolFile(const CodeModule *codeModule,
                                                                const SystemInfo *systemInfo,
                                                                string *symbolFileName,
                                                                string *symbolData)
  {
    UNUSED_VARIABLE(codeModule);
    UNUSED_VARIABLE(systemInfo);
    UNUSED_VARIABLE(symbolFileName);
    UNUSED_VARIABLE(symbolData);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

    return SymbolSupplier::INTERRUPT;
  }

  SymbolSupplier::SymbolResult SimpleSymbolSupplier::GetCStringSymbolData(const CodeModule *codeModule,
                                                                       const SystemInfo *systemInfo,
                                                                       string *symbolFileName,
                                                                       char **symbolData,
                                                                       size_t *symbolDataSize)
  {
    size_t n;

    assert(codeModule != NULL);
    assert(systemInfo != NULL);
    assert(symbolFileName != NULL);
    assert(symbolData != NULL);
    assert(symbolDataSize != NULL);

    UNUSED_VARIABLE(codeModule);
    UNUSED_VARIABLE(systemInfo);

    (*symbolFileName) = mSymbolFileName;

    if (codeModule->debug_file().find("bar") == 0)
    {
      // read symbol file into C++-string
      string symbolDataString;
      std::ifstream input(symbolFileName->c_str());
      std::getline(input,symbolDataString,string::traits_type::to_char_type(string::traits_type::eof()));
      input.close();

      // convert to C-string
      n = symbolDataString.size();
      mSymbolData = new char[n+1];
      if (mSymbolData == NULL)
      {
        fprintf(stderr,"ERROR: insufficient memory (required %lu bytes)!\n",n+1);
        return SymbolSupplier::INTERRUPT;
      }
      memcpy(mSymbolData,symbolDataString.c_str(),n);
      mSymbolData[n] = '\0';

      (*symbolData)     = mSymbolData;
      (*symbolDataSize) = n+1;

      return SymbolSupplier::FOUND;
    }
    else
    {
      return SymbolSupplier::NOT_FOUND;
    }
  }

  void SimpleSymbolSupplier::FreeSymbolData(const CodeModule *codeModule)
  {
    UNUSED_VARIABLE(codeModule);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
    delete mSymbolData;
  }
} // namespace

using google_breakpad::SimpleSymbolSupplier;

/***********************************************************************\
* Name   : printMinidump
* Purpose: print minidump
* Input  : minidumpFileName - minidump file name
*          symbolFileName   - symbol file anme
* Output : -
* Return : true if dump printed, false otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool printMinidump(const string &minidumpFileName, const string &symbolFileName)
{
  // create symbol supplier
  SimpleSymbolSupplier simpleSymbolSupplier(symbolFileName);

  // process minidump file
  BasicSourceLineResolver basicSourceLineResolver;
  MinidumpProcessor minidumpProcessor(&simpleSymbolSupplier,&basicSourceLineResolver);

  Minidump minidump(minidumpFileName);
  if (!minidump.Read())
  {
    fprintf(stderr,"ERROR: Cannot read minidump!\n");
    return false;
  }

  ProcessState processState;
  if (minidumpProcessor.Process(&minidump,&processState) != google_breakpad::PROCESS_OK)
  {
    fprintf(stderr,"ERROR: Cannot initialize minidump processor!\n");
    return false;
  }

  // print state
  PrintProcessState(processState,true,&basicSourceLineResolver);

  return true;
}

/***********************************************************************\
* Name   : printUsage
* Purpose: print program usage
* Input  : programName - program name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printUsage(const char *programName)
{
  assert(programName != NULL);

  fprintf(stdout,"Usage: %s <minidump file> <symbol file>\n",programName);
}

/***********************************************************************\
* Name   : main
* Purpose: main programm
* Input  : argc - number of arguments
*          args - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

int main(int argc, char **argv)
{
  const char *minidumpFileName,*symbolFileName;

  if (argc < 3)
  {
    printUsage(argv[0]);
    return 1;
  }

  minidumpFileName = argv[1];

  symbolFileName   = argv[2];

  if (!printMinidump(minidumpFileName,symbolFileName))
  {
    return 1;
  }

  return 0;
}
