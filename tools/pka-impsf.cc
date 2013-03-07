
#include <iostream>
#include <string>

#include <poloka/imagepsf.h>
#include <poloka/polokaconf.h>
#include <poloka/polokaexception.h>

static void usage(const char* progname) {
  cerr << "Usage: " << progname << " [OPTION]... DBIMAGE...\n"
       << "Make and fit a Point Spread Function for a DBIMAGE\n\n"
       << "    -f         : force overwrite\n"
       << "    -d OPTION  : use this datacard option file\n"
       << "    -c FILE    : use an external catalogue (sideral coordinates)\n\n";
  exit(EXIT_FAILURE);
}
 

int main( int nargs, char **args) {

  if (nargs <=1) usage(args[0]);

  list<string> imList;
  string externalCatalogName;
  bool force = false;

  for (int i=1; i < nargs; ++i)  {
    const char *arg = args[i];
    if (arg[0] != '-') {
      imList.push_back(args[i]);
      continue;
    }
    switch (arg[1]) {
    case 'f' : force = true; break;
    case 'd' : SetDatacardsFileName(args[++i]); break;
    case 'c' : externalCatalogName = args[++i]; break;
    default : usage(args[0]); break;
    }
  }

  bool status = true;
  try {
    for (list<string>::const_iterator it=imList.begin(); it != imList.end(); ++it)
      status &= MakePSF(*it, force, externalCatalogName);
  }
  catch(PolokaException p) {
      p.PrintMessage(cout);
      status = false;
  }
 
  return status? EXIT_SUCCESS : EXIT_FAILURE;
}
