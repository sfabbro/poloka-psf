#include <iostream>
#include <fstream>
#include <string>


#include "stringlist.h"
#include "swarpstack.h"
#include "sestar.h"
#include "fitsimage.h"
#include "fitsslice.h"
#include "wcsutils.h"
#include "sextractor_box.h"

#ifdef TODO


- average AIRMASS ?? (what for ?)
- integrate EXPTIME
- write contituents in the output header in the form 712865o[0-2,4-35]

#endif



#define SWARP_HEADER_EXT ".headforswarp"

#define SWARP_COMMAND " swarp "
//#define SWARP_COMMAND " /afs/in2p3.fr/throng/snovae/softsnls/swarp-2.17.1-with-fixes/bin/swarp "


#define REPROCESS_WEIGHT_ONLINE_SCRIPT " $THRONG_DIR/softsnls/scripts-dh/reprocess_weight_image.csh " 



/*************** SwarpCards   **************/

#include <list>
#include <map> /* only use pair, but no pair.h in modern stdlib */


/* we do not use here datacards a la Peida (datacards.cc)
   but a simplified version of datacards a la swarp, so that
   standard swarp datacards can be used */

using namespace std;

struct SwarpCards :  public list<pair<string,string> > 
{

  typedef std::list<std::pair<std::string,std::string> >::iterator KeyIterator;

  typedef list<pair<string,string> >::const_iterator KeyCIterator;
  

  KeyCIterator Where(const std::string &Key)  const
    {
      KeyCIterator i = begin();
      for ( ; i!=end(); ++i) if (i->first == Key) break;
      return i;
    }

  KeyIterator Where(const std::string &Key)
    {
      KeyIterator i = begin();
      for ( ; i!=end(); ++i) if (i->first == Key) break;
      return i;
    }

  bool HasKey(const std::string &Key) const { return (Where(Key) != end());}


  std::string KeyLine(const std::string &KeyName) const
  {
    KeyCIterator i = Where(KeyName);
    if (i != end())
      {
	return i->first+" "+i->second;
      }
    return "";
  }

  bool RmKey(const std::string &Key)
  {
    KeyIterator i = Where(Key);
    if (i != end()) { erase(i); return true;}
    return false;
  }

  void AddKey(const std::string &Key, const std::string &Val)
  { push_back(pair<string,string>(Key,Val));}
    

  bool write(const std::string &FileName);

};

static std::string SwarpCardsDefaultName;

bool SetSwarpCardsName(const std::string &Name)
{
  SwarpCardsDefaultName = Name;
  if (!FileExists(SwarpCardsDefaultName))
    {
      std::cerr << " cannot find " << SwarpCardsDefaultName << std::endl;
      return false;
    }
  // try them right now
  SwarpCards trial;
  return trial.write("/dev/null");
} 


static SwarpCards *DefaultSwarpCardsKeys = NULL;

void AddSwarpDefaultKey(const std::string &Key, const std::string &Value)
{
  if (!DefaultSwarpCardsKeys) DefaultSwarpCardsKeys = new SwarpCards;
  // last call wins:
  DefaultSwarpCardsKeys->RmKey(Key);
  DefaultSwarpCardsKeys->AddKey(Key,Value);
}


bool SwarpCards::write(const std::string &FileName)
{
  std::string comment;
  std::string removed;
  bool ok = true;
  
  if (SwarpCardsDefaultName  != "" && !FileExists(SwarpCardsDefaultName))
    {
      std::cout << " cannot find " << SwarpCardsDefaultName << ", resorting to swarp internal defaults" << std::endl;
      comment = "# could not open "+SwarpCardsDefaultName;
    }
  else if (FileExists(SwarpCardsDefaultName))
    {
      FILE *f = fopen(SwarpCardsDefaultName.c_str(),"r");
      char line[1024];
      while (fgets(line,1024,f))
	{
	  char *p = line;
	  while (*p == ' ' || *p == '\t' ) p++;
	  if (*p == '\0') continue;
	  if (*p == '#') continue;
	  char first_word[256];
	  char second_word[256];
	  if (sscanf(p,"%s %s",first_word,second_word)!=2) 
	    {
	      std::cout << " don't understand line " << std::endl 
			<< line << std::endl;
	      std::cout << " non standard file for swarp " 
			<<  SwarpCardsDefaultName << std::endl;
	      ok = false;
	      continue;
	    }
	  std::string key = string(first_word);
	  if (HasKey(key))
	    {
	      removed += "#"+KeyLine(key)+"\n";
	      RmKey(key);
	    }
	  AddKey(key, second_word);
	}
      fclose(f);
      comment = "# read default keys from "+SwarpCardsDefaultName;
    }
  // now consider eventual default keys set by program
  if (DefaultSwarpCardsKeys)
    for (KeyIterator i = DefaultSwarpCardsKeys->begin(); 
	 i != DefaultSwarpCardsKeys->end(); ++i)
      {
	KeyIterator j = Where(i->first);
	if (j!= end()) removed+= "#"+KeyLine(j->first)+"\n";
	AddKey(i->first, i->second);
      }
  // write survivers and comments
  FILE *out = fopen(FileName.c_str(),"w");
  fprintf(out,"%s\n",comment.c_str());
  // keep track of overwritten keys
  if (removed != "")
    fprintf(out,"# toads default superseeded :\n%s#end of superseeded keys\n",
	    removed.c_str());

  for (KeyIterator i= begin(); i != end(); ++i)
      fprintf(out,"%s    %s\n",i->first.c_str(),i->second.c_str());
  fclose(out);
  return ok;
}



   
	  

/************* class AsciiHead *********************************/


typedef enum {WCSKEYS, NAXIS} FitsGroup;

//! a class intended to assemble and print ascii-fits headers for swarp.
/*! we use the capability of cfitsio to handle "files in memory", and
  add a few convenience routines. We could as well use actual (disk) files
  with dummy names; this scheme saves the burden of generating a unique
  temporary name for the actual fits file. */
class AsciiHead : public FitsHeader
{
private :
  std::string asciiName;

public :
  AsciiHead(const std::string &FileName) : FitsHeader("mem://",RW)
  { 
    asciiName = FileName;
    // by default we put mandatory fits keys into a newly created header
    // here we don't care for those keys, so me remove them now, and let
    // the user put them back if she wishes to do so:
    RmKey("SIMPLE");
    RmKey("BITPIX");
    RmKey("NAXIS");
    RmKey("NAXIS1");
    RmKey("NAXIS2");
    RmKey("ORIGIN");
  }


  int CopyKeys(const FitsHeader &From, const StringList &KeyNames)
  { // this routine could be in FitsHeader (with the same signature as CopyKey)
    int count = 0;
    for (StringCIterator i = KeyNames.begin(); i != KeyNames.end(); ++i)
      if (From.CopyKey(*i, *this)) count ++;
    return count;
  }

  void CopyKeyGroup(const FitsHeader &From, const FitsGroup Group)
  {
    if (Group == WCSKEYS)
      {
	StringList l; WCSKeyList(From, l); CopyKeys(From, l);
      }
    if (Group == NAXIS)
      {
	/* don't use FitsHeader::CopyKey here: translations
	   (NAXIS->ZNAXIS) are not handled */
	this->AddOrModKey("NAXIS",int(From.KeyVal("NAXIS")));
	this->AddOrModKey("NAXIS1",int(From.KeyVal("NAXIS1")));
	this->AddOrModKey("NAXIS2",int(From.KeyVal("NAXIS2")));
      }
  }
	

  ~AsciiHead()
  {
    ofstream s(asciiName.c_str());
    FitsHeader &head = *this;
    s << head;
    s<< "END     "<< std::endl;
    s.close();
    /* because cfitsio complains when a file gets closed without
       SIMPLE, BITPIX, NAXIS ,..., but does not check when deleting,
       although the fits file used here resides in memory, and will
       anyway be "deleted". */
    DeleteFile(); 

  }
};

/**********************  class Ccd, Shoot and Collection *******************/

//! a ccd in an image
struct Ccd
{
  std::string name;
  int chip;
  double gain;
  double sigmaBack;
  double seeing;
  double fluxScale;
  std::string filter;
  std::string refcat;
  //  std::string acqFileName;
  

  Ccd(const ReducedImage &R, const double FluxScale)
  {
    name = R.Name();
    FitsHeader head(R.FitsName());
    gain = head.KeyVal("TOADGAIN");
    seeing = R.Seeing();
    chip = head.KeyVal("TOADCHIP");
    filter = string(head.KeyVal("TOADFILT"));
    refcat = string(head.KeyVal("REFCAT"));
    sigmaBack = R.SigmaBack();
    fluxScale = FluxScale;
  }

  //double Weight() { return 1/(sigmaBack*sigmaBack);}
  // more interesting for a true weighted average
  double Weight() { return 1/(sigmaBack*sigmaBack*fluxScale*fluxScale*seeing*seeing);}

};


bool CcdCompareChip(const Ccd&C1, const Ccd&C2)
{
  return (C1.chip < C2.chip);
}

//! the ensemble of ccd's of a given shoot
struct Shoot : public std::list<Ccd> // use list for sort
{
  int odo;

  Shoot(int Odo) { odo = Odo;}
  void AddCcd(const ReducedImage &R, const double &FluxScale) 
  { push_back(Ccd(R, FluxScale));}


  // assumes that all ccds from a shoot have the same filter!
  std::string Filter() const
  {
    return front().filter;
  }


  double AverageGain()
  {
    double average = 0;
    for (iterator i= begin(); i != end(); ++i) average += i->gain;
    return average/size();
  }
  
  double AverageWeight()
  {
    double average = 0;
    for (iterator i= begin(); i != end(); ++i) average += i->Weight();
    return average/size();
  }

  
  double AverageFluxScale()
  {
    double average = 0;
    for (iterator i= begin(); i != end(); ++i) average += i->fluxScale;
    return average/size();
  }

  void CollectAllNames(StringList &List)
  {
    sort(CcdCompareChip);
    for (iterator i = begin(); i !=end(); ++i) 
      List.push_back(i->name);
  }

  
  // to assemble names like 123456o[00-02,04-35] : does not work (yet?)
  std::string CompactName()
  {
    sort(CcdCompareChip);
    std::string result;
    if (size() == 1) return front().name;
    iterator i= begin(); ++i;
    int startchip = i->chip;
    int endchip = startchip;
    char buf[16];
    for (; i != end(); ++i)
      {
	int chip = i->chip;
	if (chip == endchip+1) { endchip = chip; continue;}
	if (endchip != startchip)
	    sprintf(buf,"%02d-%02d",startchip,endchip);
	else
	    sprintf(buf,"%02d",startchip);
	if (result[result.size()-1] != '[') result += ",";
	result += buf;
	startchip = endchip = chip;
      }
    return result;
  }


};


bool ShootCompareOdo(const Shoot &S1, const Shoot &S2)
{
  return (S1.odo < S2.odo);
}

//! a set of Shoot's
struct Collection : public list<Shoot>
{

  Shoot *Locate(const int Odo)
  {
    for (iterator i = begin(); i!= end(); ++i)
      if (i->odo == Odo) return &(*i);
    return NULL;
  }

  void AddCcd(const ReducedImage &R, const double FluxScale)
  {
    FitsHeader head(R.FitsName());
    // need a TOADKEY for that
    int odo = head.HasKey("EXPNUM") ? head.KeyVal("EXPNUM") : 0; 
    Shoot *s = Locate(odo);
    if (!s) 
      {
	push_back(Shoot(odo));
	s = &(back());
      }
    s->AddCcd(R, FluxScale);
  }

  double AverageGain()
  {
    double num = 0;
    double deno = 0;
    int count = 0;
    for (iterator i = begin(); i!= end(); ++i)
      {
	Shoot &s = (*i);
	double averageWeight = s.AverageWeight();
	num += averageWeight;
	double averageGain = s.AverageGain();
	double averageFluxScale = s.AverageFluxScale();
	deno += averageWeight*averageFluxScale/ averageGain;
	count ++;
      }
    return count*num/deno;
  }
    double AverageGain_SimpleSum()
  {
    int count = 0;
    double sum_fs_over_g = 0. ;
    for (iterator i = begin(); i!= end(); ++i)
      {
	Shoot &s = (*i);
	double averageWeight = s.AverageWeight();
	double averageGain = s.AverageGain();
	double averageFluxScale = s.AverageFluxScale();
	sum_fs_over_g += averageFluxScale/ averageGain;
	count ++;
      }
    return count*count/sum_fs_over_g;
  }
  
  void CollectAllNames(StringList &List)
  {
    sort(ShootCompareOdo);
    List.clear();
    for (iterator i=begin(); i!= end(); ++i) 
      i->CollectAllNames(List);
  }

  // don't use it : Shoot::CompactName is not finished
  void CollectAllCompactNames(StringList &List)
  {
    sort(ShootCompareOdo);
    List.clear();
    for (iterator i=begin(); i!= end(); ++i) 
      List.push_back(i->CompactName());
  }
  
  StringList FilterList() const
  {
    StringList l;
    for (const_iterator i= begin(); i != end(); ++i) 
      l.push_back(i->Filter());
    l.sort(); l.unique(); // remove multiple entries
    return l;
  }


  StringList RefcatList() const
  {
    StringList l;
    for (const_iterator i= begin(); i != end(); ++i) // loop on Shoots
      for (Shoot::const_iterator j = i->begin(); j!= i->end(); ++j) // CCd's
	l.push_back(j->refcat);
    l.sort(); l.unique(); // remove multiple entries
    return l;
  }
};

/*********************** class SwarpStack ***********************/

const std::string SwarpStack::SwarpTmpDir()
{
  if (tmpDir == "")
    {
      char *dir = getenv("IMAGE_TMP_DIR");
      if (dir) tmpDir = AddSlash(string(dir));
      else tmpDir = FullFileName(StandardPath(SwarpPermDir()));
      cout << "SwarpPermDir() " << SwarpPermDir() << endl;
      cout << "StandardPath(SwarpPermDir()) " << StandardPath(SwarpPermDir()) 
	   << endl;
      cout << " for image " << Name() << ", using " << tmpDir
	   << " to write temporary images " << endl;
    }
  return tmpDir;
}




/* in a future version, we may align "Images" with the ref catalog
that may be stored in PhotomAstromReference. For the time beeing, 
we just prepare things for swarp, run it and cleanup afterwards */
SwarpStack::SwarpStack(const string &Name, const ReducedImageList &Images,
		       const ReducedImage *PhotomAstromReference,
		       const Frame &SubFrame): ReducedImage(Name)
{
  if (!Create("here")) return;
  // create the directory for swarp files
  if (!FileExists(SwarpPermDir()) && !MKDir(SwarpPermDir().c_str()) ) return;
  images = Images;
  photomAstromReference = PhotomAstromReference;
  photomAstromReferenceFrame = SubFrame;
}


SwarpStack::SwarpStack(const string &Name) : ReducedImage(Name) {}

static string build_file_name(const std::string &Format, const std::string &AName)
{
  char link_name[1024];
  sprintf(link_name,Format.c_str(),AName.c_str());
  return link_name;
}

// substitute_extension("toto.fits.gz","xa")="toto.fits.xa"
// this is different from SubstituteExtension.
static string substitute_extension(const std::string &Original, 
			   const std::string &NewExtension)
{
  const char *dot = strrchr(Original.c_str(),'.');
  unsigned int dotpos = (dot)? dot-Original.c_str(): Original.size();
  if (dotpos > Original.size()) return Original;
  return Original.substr(0,dotpos)+NewExtension;  
}

bool extract_ascii_head(const std::string &InputHeadName,
		       const Frame &SubFrame,
		       const std::string AsciiFileName)
{
  /*
  FitsHeader modified(InputHeadName+frame_to_string(SubFrame));
  modified.AsciiDump(AsciiFileName);
  */
  /* This approach does not work for all files:
     sone files are longer than needed (i.e.) the EOF is *NOT*
     just after the last pixel. In cfitsio, this causes an error,
     when addressing a subimage using the [a:b,c:d] syntax.
     A more serious drawback of this syntax is that cfitsio actually
     reads the pixels (of the requested patch) in memory. 
     Here, we only want the header, so we go by hand
     1) copy the header
     2) edit NAXIS1 NAXIS2, CRPIX1 CRPIX2
     3) ascii write
  */
  AsciiHead ah(AsciiFileName);
  FitsHeader head(InputHeadName);
  ah.CopyKeyGroup(head, WCSKEYS); ah.CopyKeyGroup(head, NAXIS);
  int imin = int(SubFrame.xMin+0.5);
  int imax = int(SubFrame.xMax+0.5);
  int jmin = int(SubFrame.yMin+0.5);
  int jmax = int(SubFrame.yMax+0.5);
  ah.AddOrModKey("NAXIS1",imax-imin+1);
  ah.AddOrModKey("NAXIS2",jmax-jmin+1);
  double crpix1 = ah.KeyVal("CRPIX1");
  double crpix2 = ah.KeyVal("CRPIX2");
  ah.AddOrModKey("CRPIX1",crpix1-imin);
  ah.AddOrModKey("CRPIX2",crpix2-jmin);
  return true;
}

static void extract_ascii_wcs(const std::string &FitsName, 
			      const std::string &AsciiFileName)
{
  AsciiHead ah(AsciiFileName);
  FitsHeader head(FitsName);
  ah.CopyKeyGroup(head, NAXIS);
  ah.CopyKeyGroup(head, WCSKEYS);
}


/* on makiki/kiholo, the openfile limit was enlarged only 
   for tcsh*/
static int my_tcsh_system(const std::string &Command)
{
  std::cout << " spawning command:" << std::endl << Command << std::endl;
  // do not hard code path and tcsh should not be forced here but on the shell
  //return system(("/bin/tcsh -f -c \'"+Command+"\'").c_str());
  return system(Command.c_str());
}



void SwarpStack::Success()
{
  string success = "touch " + FullFileName(Dir()) + "/.success" ;
  my_tcsh_system(success);
  return ;
}

void SwarpStack::SetExternalHeader(const std::string &ext_header)
{
  string command = "cp " + ext_header + "  " + FullFileName(Dir()) + "/calibrated" + SWARP_HEADER_EXT;
  my_tcsh_system(command);
  return ;
}
  

bool SwarpStack::MakeHeaderFromRef()
{
  bool res = false ;
  if (photomAstromReference != NULL)
    {
      res = extract_ascii_head(photomAstromReference->FitsName(),
			 photomAstromReferenceFrame, 
			 substitute_extension(FitsName(),SWARP_HEADER_EXT)
			 );
    }
  return(res);
}

bool SwarpStack::MakeFits()
{
  if (HasImage()) return true;
  std ::cout << " SwarpStack::MakeFits : making " << FitsName() << std::endl;
  /* link files in the "working directory ", and collect info about
     input images */
  string toRemove;
  string inputFiles;
  double saturation = 1e30;
  // collect info on input stuff to assemble output header
  Collection collection; 
  // zero point : if there is a reference, align on it
  double stackZp = 30;
  if (photomAstromReference)
    {
      FitsHeader head(photomAstromReference->FitsName());
      if (head.HasKey("ZP")) stackZp = head.KeyVal("ZP");
      else
	std::cerr << " cannot align photometrically on " << photomAstromReference->Name() 
		  << " because we don't find it's zero point " << std::endl;
    }

  // setup files for swarp input
  for (ReducedImageIterator i=images.begin(); i!= images.end(); ++i)
    {
      ReducedImage &ri = **i;
      // check catalog size (to remove e.g. non working CCD03 images)
      SEStarList seList(ri.CatalogName());
      if (seList.size() < 10)
	{
	  std::cout << " SwarpStack::MakeFits : ignoring input image " << ri.Name() 
		    << " because it only has " << seList.size() << " objects in catalog" 
		    << std::endl;
	  continue;
	}
      // build file names for swarp input files.
      std::string imageSwarpName =  build_file_name(SwarpTmpDir()+"%s.image.fits", ri.Name());
      std::string weightSwarpName = 
	build_file_name(SwarpTmpDir()+"%s.image.weight.fits", ri.Name());
      
      std::string tmpimage;
      remove(imageSwarpName.c_str());
      std::string decompImage = DecompressImageIfNeeded(ri.FitsName(), imageSwarpName, tmpimage);
      if (decompImage.empty())
	{
	  std::cerr << " could not prepare " << imageSwarpName 
		    << " for swarp " << std::endl;
	  return false;
	}
      if (decompImage == ri.FitsName() && ri.BackSub() && 
	  !MakeRelativeLink(ri.FitsName(), imageSwarpName))
	{
	  std::cerr << " could not prepare " << imageSwarpName 
		    << " for swarp " << std::endl;
	  return false;
	}
      else if (decompImage == ri.FitsName())
	{
	  ImageCopy(ri.FitsName(), imageSwarpName, true);
	}
      if (!ri.BackSub())
	{
	  cout << " Subtracting background for " << ri.Name() << endl;
	  FitsImage im(imageSwarpName,RW);
	  ri.MakeBack();
	  SubtractMiniBack(im, ri.FitsMiniBackName());
	}

      // HACK for weight contaminated with satur mask
      if (getenv("REPROCESS_WEIGHT_ONLINE"))
	{
	  string command_rep  = REPROCESS_WEIGHT_ONLINE_SCRIPT ;
	  string command = command_rep + " " + ri.Dir() + " " + SwarpTmpDir()  ;
	  cerr << "Running " << command << endl ;
	  system(command.c_str());
	  command = "cp " + SwarpTmpDir() + "/" + ri.Name() + "/weight.fits " + weightSwarpName ;
	  cerr << "Running " << command << endl ;
	  system(command.c_str());
	  command = "rm -rf " + SwarpTmpDir() + "/" + ri.Name() ;
	  cerr << "Running " << command << endl ;
	  system(command.c_str());
	}
      else
	if (!DecompressOrLinkImage(ri.FitsWeightName(), weightSwarpName))
	  {
	    std::cerr << " could not prepare " << weightSwarpName 
		      << " for swarp " << std::endl;
	    return false;
	    continue;
	  }




      inputFiles += (" "+BaseName(imageSwarpName));
      toRemove += " "+imageSwarpName;
      toRemove += " "+weightSwarpName;

      // compute flux scale factor from ZP
      FitsHeader head(ri.FitsName());
      double fluxScale = 1;
      if (head.HasKey("ZP"))
	{
	  double thisZp = head.KeyVal("ZP");
	  double ZP_phot  =  -1 ;
	  if (head.HasKey("ZP_PHOT"))
	    {
	      thisZp  = head.KeyVal("ZP_PHOT");
	    }
	  else
	    cout << "SwarpStack::MakeFits : NO ZP_PHOT key in " << ri.Name() << " using ZP key" << endl ;
	  cout << "SwarpStack::MakeFits : original ZP_used_in_swarp " << ri.Name() << " :  " << head.KeyVal("EXTVER") << " " << thisZp << endl ; 
	  fluxScale = pow(10.,0.4*(stackZp - thisZp));
	}
      else
	{
	  std::cerr << " ERROR : SwarpStack::MakeFits : image " << ri.Name() 
		    << " misses a photometric zero point " << std::endl;
	}
      // write fluxScale in a ascii header
      string header_name = substitute_extension(imageSwarpName,SWARP_HEADER_EXT);
      AsciiHead ah(header_name);
      ah.AddKey("FLXSCALE", fluxScale);
      // compute the satur level for this image 
      double this_satur = ri.Saturation();
      saturation = min(saturation,this_satur*fluxScale);
      collection.AddCcd(ri, fluxScale);
    }
  // provide to swarp the header we want:
  if (photomAstromReference != NULL)
    {
      extract_ascii_head(photomAstromReference->FitsName(),
			 photomAstromReferenceFrame, 
			 substitute_extension(FitsName(),SWARP_HEADER_EXT)
			 );
    }
  // setup datacards
  SwarpCards cards;
  // directly write image and weight in the right place
  cards.AddKey("IMAGEOUT_NAME",FullFileName(FitsName()));
  cards.AddKey("WEIGHTOUT_NAME",FullFileName(FitsWeightName()));
  cards.AddKey("HEADER_SUFFIX",SWARP_HEADER_EXT);
  /* 
     There are issues in how Swarp makes the interpolation from
     input pixels to output output projection (projapp.c):
     the interpolator from input to output pixels
     has a bug, that we did not find. However, we modified the
     projapp.c file of swarp in order to abort when the 
     "bug condition" occurs, and a trick that seems to avoid
     this deadly condition. If this trick does not work for any 
     reason, you can use the non-approximation mode of SWarp,
     by setting PROJECTION_ERR to 0. This makes the code about 
     3 times slower. If you change here, change also in MakeSatur().

     E.Bertin has been informed of the problem.
  */
  //   cards.AddKey("PROJECTION_ERR","0.0");

  // tell swarp that weights are weights
  cards.AddKey("WEIGHT_TYPE","MAP_WEIGHT");
  // weighted average stack
  
  // --nrl 02/2006 in order to deal w/ the satellites...
  //cards.AddKey("COMBINE_TYPE","MEDIAN");
  //  cards.AddKey("COMBINE_TYPE","WEIGHTED");
  bool weighted_sum = false ;
  if (getenv("WEIGHTED_AVERAGE"))
    {
      cards.AddKey("COMBINE_TYPE","WEIGHTED");
      weighted_sum = true ;
    }
  else
    cards.AddKey("COMBINE_TYPE","MEDIAN");

  // no astrometric flux rescaling 
  // (this is because Elixir does in principle the right job)
  /* I even tried without this card (i.e. with FSCALASTRO = FIXED
     and the results are a total disaster (why??) */
  cards.AddKey("FSCALASTRO_TYPE","NONE");
  // background alredy subtracted
  cards.AddKey("SUBTRACT_BACK","N");
  // cleanup?
  cards.AddKey("DELETE_TMPFILES","N");

  // write cards
  string cardsName = "default.images.swarp";
  // write them in perm dir
  cards.write(SwarpPermDir()+cardsName);
  //link them in Tmp Directory
  MakeRelativeLink(SwarpPermDir()+cardsName, SwarpTmpDir()+cardsName);
  
  

  string command = "cd "+SwarpTmpDir()+"; " + SWARP_COMMAND + " -c " + cardsName 
    + inputFiles;
  if (my_tcsh_system(command)!=0)
    {
      cerr <<" swarpstack: something went wrong ... " << std::endl;
      return false;
    }
  // STILL have to add a few things in the header
  {
    FitsHeader head(FitsName(), RW);
    StringList filtList = collection.FilterList();
    if (filtList.size() > 1)
      {
	std::cout << " Image " << Name() << " is composed of different filters" 
		  << " hope you know what you are doing" << std::endl;
	head.AddOrModKey("FILTERS", filtList.AllEntries());
      }
    else head.AddOrModKey("FILTER",filtList.front());
    StringList refcatList = collection.RefcatList();
    if (filtList.size() > 1)
      {
	std::cout << " Image " << Name() << " is made from components aligned "
		  << "\n on different astro-photometric catalogs (REFCAT key)"
		  << "\n hope you know what you are doing" << std::endl;
	head.AddOrModKey("REFCATS", filtList.AllEntries());
      }
    else head.AddOrModKey("REFCAT",refcatList.front());
    double average_gain;
    if (weighted_sum) 
      average_gain = collection.AverageGain();
    else
       average_gain = collection.AverageGain_SimpleSum();
    
    head.AddOrModKey("GAIN", average_gain," averaged over the whole area, rather rough");
    head.AddOrModKey("BACK_SUB",true);
    head.AddOrModKey("ZP",stackZp);
    
    
  }

  // write the component list
  ofstream s((Dir()+"/components.list").c_str());
  StringList components;
  collection.CollectAllNames(components);
  for (StringIterator i = components.begin(); i != components.end(); ++i)
    s << *i << std::endl;
  s.close();
  // write out the saturation
  SetSaturation(saturation);

  // since we got here, swarp succeeded, so cleanup resamp files

  // and cleanup input files (links or actual files depending if they were compressed)
  RemoveFiles(toRemove);
  RemoveFiles(SubstitutePattern(toRemove,".image.",".image.resamp."));
  return true;
}


bool SwarpStack::MakeFits_OnlyAdd()
{
  if (HasImage()) return true;
  std ::cout << " SwarpStack::MakeFits_OnlyAdd : making " << FitsName() << std::endl;
  /* link files in the "working directory ", and collect info about
     input images */
  string toRemove;
  string inputFiles;
  double saturation = 1e30;
  // collect info on input stuff to assemble output header
  Collection collection; 
  // zero point : if there is a reference, align on it
  double stackZp = 30;
  // setup files for swarp input
  for (ReducedImageIterator i=images.begin(); i!= images.end(); ++i)
    {
      ReducedImage &ri = **i;
      
      // build file names for swarp input files.
      std::string imageSwarpName =  build_file_name(SwarpTmpDir()+"%s.image.fits", ri.Name());
      if (!DecompressOrLinkImage(ri.FitsName(), imageSwarpName))
	{
	  std::cerr << " could not prepare " << imageSwarpName 
		    << " for swarp " << std::endl;
	  //return false;
	  continue;
	}
      inputFiles += (" "+BaseName(imageSwarpName));
      toRemove += " "+imageSwarpName;

      std::string weightSwarpName = 
	build_file_name(SwarpTmpDir()+"%s.image.weight.fits", ri.Name());
      if (!DecompressOrLinkImage(ri.FitsWeightName(), weightSwarpName))
	{
	  std::cerr << " could not prepare " << weightSwarpName 
		    << " for swarp " << std::endl;
	  //return false;
	  continue;
	}
      toRemove += " "+weightSwarpName;
      // we do not compute flux scale factor from ZP
      // we compute the satur level for this image 
      double fluxScale = 1;
      double this_satur = ri.Saturation();
      saturation = min(saturation,this_satur*fluxScale);
      collection.AddCcd(ri, fluxScale);
    }
  // setup datacards
  SwarpCards cards;
  // directly write image and weight in the right place
  cards.AddKey("IMAGEOUT_NAME",FullFileName(FitsName()));
  cards.AddKey("WEIGHTOUT_NAME",FullFileName(FitsWeightName()));
  cards.AddKey("HEADER_SUFFIX",SWARP_HEADER_EXT);
  // tell swarp that weights are weights
  cards.AddKey("WEIGHT_TYPE","MAP_WEIGHT");
  // no resampling
  cards.AddKey("RESAMPLE", "N");
  // weighted average stack
  cards.AddKey("COMBINE_TYPE","WEIGHTED");
  cards.AddKey("FSCALASTRO_TYPE","NONE");
  // background alredy subtracted
  cards.AddKey("SUBTRACT_BACK","N");
  // cleanup?
  cards.AddKey("DELETE_TMPFILES","N");

  // write cards
  string cardsName = "default.images.swarp";
  // write them in perm dir
  cards.write(SwarpPermDir()+cardsName);
  // copy them in Tmp Directory
  cards.write(SwarpTmpDir()+cardsName);
  
  

  string command = "cd "+SwarpTmpDir()+";" + SWARP_COMMAND + " -c " + cardsName 
    + inputFiles;
  if (my_tcsh_system(command.c_str())!=0)
    {
      cerr <<" swarpstack: something went wrong ... " << std::endl;
      return false;
    }
  // STILL have to add a few things in the header
  {
    FitsHeader head(FitsName(), RW);
    StringList filtList = collection.FilterList();
    if (filtList.size() > 1)
      {
	std::cout << " Image " << Name() << " is composed of different filters" 
		  << " hope you know what you are doing" << std::endl;
	head.AddOrModKey("FILTERS", filtList.AllEntries());
      }
    else head.AddOrModKey("FILTER",filtList.front());
    StringList refcatList = collection.RefcatList();
    if (filtList.size() > 1)
      {
	std::cout << " Image " << Name() << " is made from components aligned "
		  << "\n on different astro-photometric catalogs (REFCAT key)"
		  << "\n hope you know what you are doing" << std::endl;
	head.AddOrModKey("REFCATS", filtList.AllEntries());
      }
    else head.AddOrModKey("REFCAT",refcatList.front());
    
    head.AddOrModKey("GAIN", collection.AverageGain()," averaged over the whole are, rather rough");
    head.AddOrModKey("BACK_SUB",true);
    head.AddOrModKey("ZP",stackZp);
    
    
  }

  // write the component list
  ofstream s((Dir()+"/components.list").c_str());
  StringList components;
  collection.CollectAllNames(components);
  for (StringIterator i = components.begin(); i != components.end(); ++i)
    s << *i << std::endl;
  s.close();
  // write out the saturation
  SetSaturation(saturation);

  // since we got here, swarp succeeded, so cleanup resamp files

  // and cleanup input files (links or actual files depending if they were compressed)
    RemoveFiles(toRemove);
    RemoveFiles(SubstitutePattern(toRemove,".image.",".image.resamp."));
  return true;
}



bool SwarpStack::MakeCatalog()
{
  MakeSatur();
  return ReducedImage::MakeCatalog_ImageBizarre();
}


bool SwarpStack::MakeDead()
{
  cerr << " SwarpStack::MakeDead() still to be implemented " << std::endl; 
  return false;
  
}

bool SwarpStack::MakeWeight()
{
  if (!HasWeight()) return MakeFits();
  else return true;
}


bool SwarpStack::MakeSatur()
{
  bool only_add = false ;
  return(MakeSatur(only_add));
}
bool SwarpStack::MakeSatur(bool only_add)
{
  if (HasSatur()) return true;
  // link files in the "working directory "
  std::cout << " Making satur image for " << Name() << std::endl;

  /* we need the header to align our satur frame on it */
  if (!HasImage()) MakeFits(); 


  std::string toRemove;
  std::string inputFiles;
  for (ReducedImageIterator i=images.begin(); i!= images.end(); ++i)
    {
      ReducedImage &ri = **i;
      std::string inputSatur = ri.FitsSaturName();
      std::string swarpInput = build_file_name(SwarpTmpDir()+"%s.satur.fits",
					       ri.Name());
      std::string swarpHead = substitute_extension(swarpInput,SWARP_HEADER_EXT);
      if (!DecompressOrLinkImage(inputSatur,swarpInput))
	{
	  std::cerr << " could not prepare " << inputSatur << " for swarp " 
		    << std::endl;
	  return false;
	}
      inputFiles += " "+BaseName(swarpInput);
      // extract header of calibrated image
      toRemove += " "+swarpInput;
      extract_ascii_wcs(ri.FitsName(), swarpHead);
    }

  std::string swarpOutName =  SwarpTmpDir()+"satur32.fits";
  // extract the header of the calibrated.fits
  extract_ascii_wcs(FitsName(), substitute_extension(swarpOutName,SWARP_HEADER_EXT));

  // setup datacards
  SwarpCards cards;
  cards.AddKey("IMAGEOUT_NAME", BaseName(swarpOutName));
  // I don't want a weight out of here. but swarp insists on doing one:
  cards.AddKey("WEIGHTOUT_NAME","/dev/null");
  cards.AddKey("HEADER_SUFFIX",SWARP_HEADER_EXT);
  cards.AddKey("WEIGHT_TYPE","NONE");
  /* Do not approximate output projection (there are issues in the approximation)
     E.B has been informed */
  // cards.AddKey("PROJECTION_ERR","0.0");
  cards.AddKey("COMBINE_TYPE","MAX");
  cards.AddKey("FSCALASTRO_TYPE","NONE");
  cards.AddKey("SUBTRACT_BACK","N");
  cards.AddKey("DELETE_TMPFILES","N");
  if (only_add)
    cards.AddKey("RESAMPLE","N");
  else
    cards.AddKey("RESAMPLING_TYPE","BILINEAR"); // enough for binary masks
  string cardsName = "default.satur.swarp";
  // write them in perm dir
  cards.write(SwarpPermDir()+cardsName);
  // and make a pseudo-copy in temp dir
  //MakeRelativeLink(SwarpPermDir()+cardsName, SwarpTmpDir()+cardsName);
  cards.write(SwarpTmpDir()+cardsName);

  string command = "cd "+SwarpTmpDir()+ ";" + SWARP_COMMAND + " -c " + cardsName + inputFiles;
  if (my_tcsh_system(command.c_str())!=0)
    {
      cerr <<" swarpstack: something went wrong ... " << std::endl;
      return false;
    }
  /* we have to process the output, first to quantize it (0 or 1) to
     write it in BITPIX =8, and gzip the output (gzip is done by
     cfitsio if FitsSaturName() return <something>.gz) */
  FitsInOutParallelSlices inOut(swarpOutName,
				FitsSaturName());
  // BITPIX, BSCALE, BZERO are copied by default from in to out
  // So overwrite them here:
  inOut.out.AddOrModKey("BITPIX",8);
  inOut.out.AddOrModKey("BSCALE",1); 
  inOut.out.AddOrModKey("BZERO",0);
  do
    {
      Image &out = inOut.out; // handler
      out = inOut.in; // copy
      out.Simplify(0.1); // simplify
    } while (inOut.LoadNextSlice()); // read/write
  // remove temporary files
  remove(swarpOutName.c_str());
  RemoveFiles(toRemove);
  if (!only_add)
    {
      RemoveFiles(SubstitutePattern(toRemove,".satur.",".satur.resamp."));
      RemoveFiles(SubstitutePattern(toRemove,".satur.",".satur.resamp.weight."));
    }
  RemoveFiles(SubstitutePattern(toRemove,".satur.fits",".satur.headforswarp"));
  RemoveFiles(SubstitutePattern(toRemove,".satur.fits",".image.headforswarp"));

  // we are done !
  return true;

}



  
