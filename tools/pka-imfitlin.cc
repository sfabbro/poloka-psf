#include <iostream>
#include <string>

#include <poloka/imagepsf.h>

static void usage(const string &progname) {
  cerr << "Usage: " << progname << " DBIMAGE...\n"
       << "Fit non-linearities for PSF\n\n";
  exit(EXIT_FAILURE);
}
 

int main( int nargs, char **args) {

  if (nargs <=1) usage(args[0]);

  bool status = true;

  for (int i=1; i < nargs; ++i) {
    ReducedImage *ri = new ReducedImage(args[i]);
    if (!ri->IsValid())	{
      cout << args[0] << " cannot find dbimage " << args[i] << endl;
      continue;
    }
    ImagePSF imagePSF(*ri);
    status &=imagePSF.FitNonLinearity();
  }
 
  return status? EXIT_SUCCESS : EXIT_FAILURE;
}
