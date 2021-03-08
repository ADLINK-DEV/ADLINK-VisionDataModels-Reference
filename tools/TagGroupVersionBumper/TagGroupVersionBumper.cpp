
#include <iostream>
#include <map>
#include <set>
#include <utility>
#include <algorithm>
#include <functional>
#include <sstream>
#include "spdlog/spdlog.h"
#include <boost/filesystem.hpp>
#include <nlohmann/json.hpp>

enum bump_level { MAJOR, MINOR, PATCH, REVISION, LAST_BUMP_LEVEL };

struct version_info {
  std::string prefix;
  int value[LAST_BUMP_LEVEL];
  int padding[LAST_BUMP_LEVEL];
  int position[LAST_BUMP_LEVEL];
  int endOfPosition[LAST_BUMP_LEVEL];

  bump_level actual_level = MAJOR;

  version_info() { initialize(""); }
  version_info(const version_info& orig);
  version_info(const std::string& orig_version);
  void initialize(const std::string& orig_version);

  bool operator <(const version_info& other) const;
  std::string to_string() const;
};

version_info::version_info(const version_info& other) {
  prefix = other.prefix;
  for (int i = 0; i < LAST_BUMP_LEVEL; ++i) {
    value[i] = other.value[i];
    padding[i] = other.padding[i];
    position[i] = other.position[i];
    endOfPosition[i] = other.endOfPosition[i];
  }

  actual_level = other.actual_level;
}

std::ostream& operator <<(std::ostream& out, const version_info& vi) {
  out << vi.to_string();
  return out;
}

static std::string DIGITS="0123456789";

static int get_value(const std::string& value, int& padding) {
  padding = value.size();
  int result = 0;
  for (auto iter = value.begin(); iter != value.end(); iter++) {
     result *= 10;
     result += (int)((*iter)-'0');
  }

  return result;  
}

static int get_value_only(const std::string& value) {
  int dummy;
  return get_value(value,dummy);
}
static std::string TAGGROUP_NAME = "name";
static std::string TAGGROUP_CONTEXT = "context";
static std::string TAGGROUP_VERSION = "version";
static std::string TAGGROUP_QOS_PROFILE = "qosProfile";
static std::string TAGGROUP_TAGS = "tags";
static std::string TAGGROUP_TYPEDEFINITION = "typedefinition";

static std::vector<std::string> keysOfTopLevelTagGroup = {
  TAGGROUP_NAME, TAGGROUP_CONTEXT, TAGGROUP_VERSION, TAGGROUP_QOS_PROFILE
};

static std::vector<std::string> eitherOrKeysOfTopLevelTagGroup = {
  TAGGROUP_TAGS, TAGGROUP_TYPEDEFINITION
};

class TagGroupVersionBumperOptions {
  private:
    bool listVersionsOnly = false;
    std::vector<std::string> directories;
    bump_level bumpLevel = MINOR;
    bump_level desiredLevel = MINOR;
    bool provideHelp = false;
    bool explainActions = false;
    bool padding = true;
    std::vector<int> whatPadding;
    std::string versionOverride;
    bool generateSedSubstitutionScript = false;
    std::string sedSubstitionScript = "updateVersion.sed";
    bool generateSubstitutionScript = false;
    bool useWildCardForVersion = false;

    public:
       TagGroupVersionBumperOptions(int argc, const char *argv[]);

       bool shouldListVersionsOnly() const { return listVersionsOnly; }
       const std::vector< std::string > & getDirectories() const { return directories; }
       bump_level getBumpLevel() const { return bumpLevel; }
       bump_level getDesiredLevel() const { return desiredLevel; }
       bool shouldProvideHelp() const { return provideHelp; }
       bool shouldExplainActions() const { return explainActions; }
       bool shouldDoPadding() const { return padding; }
       const std::vector<int>& getPadding() const { return whatPadding; }
       int getPadding(bump_level level) { return whatPadding[level]; }
       static void displayHelp(std::ostream& out);
       const std::string& getVersionOverride() const { return versionOverride; }
       bool hasVersionOverride() const { return versionOverride.size() > 0; }
       const std::string& getSedScriptFilename() const { return sedSubstitionScript; }
       bool shouldGenerateSubstitutionScript() const { return generateSubstitutionScript; }
       bool shouldUseWildcardForVersion() const { return useWildCardForVersion; }
};

static std::string OPTION_LIST_VERSIONS_ONLY = "--list";
static std::string OPTION_DEBUG = "--debug";
static std::string OPTION_QUIET = "--quiet";
static std::string OPTION_BUMP_MAJOR = "--bump-major";
static std::string OPTION_BUMP_MINOR = "--bump-minor";
static std::string OPTION_BUMP_PATCH = "--bump-patch";
static std::string OPTION_BUMP_REVISION = "--bump-revision";
static std::string OPTION_DEPTH = "--depth";
static std::string OPTION_HELP = "--help";
static std::string OPTION_EXPLAIN = "--explain";
static std::string OPTION_PADDING = "--padding";
static std::string OPTION_NO_PADDING = "--no-padding";
static std::string OPTION_VERSION_OVERRIDE = "--version";
static std::string OPTION_GENERATE_SED_SCRIPT = "--sed";
static std::string OPTION_USE_WILDCARD_FOR_VERSION = "--wildcard";

void TagGroupVersionBumperOptions::displayHelp(std::ostream& out) {
  out << OPTION_HELP <<  " : display this helptext" << std::endl;
  out << OPTION_DEBUG << " : display verbose logging on the processing of files." << std::endl;
  out << OPTION_QUIET << " : display only critical logging." << std::endl;
  out << std::endl;
  out << OPTION_LIST_VERSIONS_ONLY << " : display a list of found contexts, with greatest version," << std::endl << 
        "  and TagGroups with their versions. Does not bump anything." << std::endl;
  out << std::endl;
  out << "Bumping options for MAJOR.MINOR.PATCH,REVISION:" << std::endl;
  out << OPTION_BUMP_MAJOR << " : Bump MAJOR: 1[.x.y.z] -> 2[.0.0.0]" << std::endl;
  out << OPTION_BUMP_MINOR << " : Bump MINOR: x.1[.y.z] -> x.2[.0.0] (default)" << std::endl;
  out << OPTION_BUMP_PATCH << " : Bump PATCH: x.y.1[.z] -> x.y.2[.0]" << std::endl;
  out << OPTION_BUMP_REVISION << " : Bump REVISION: x.y.z.1 -> x.y.z.2" << std::endl;
  out << OPTION_PADDING << " : Keep zero-padding of original version (2.001 -> 2.002) (default). " << std::endl << "    You can also specify the required padding (eg =1,3,3,0)" << std::endl;
  out << OPTION_NO_PADDING << " : Discard zero-padding of original version (2.001 -> 2.2)" << std::endl;
  out << OPTION_DEPTH << "=level : desired level to expand; normally the current level (major, minor, patch,revision) is kept" << std::endl << "     (unless explicitly asked to bump a higher level) " << std::endl;
  out << OPTION_VERSION_OVERRIDE << "=version : rather than bumping to the next available version level, use this version instead." << std::endl;
  out << OPTION_EXPLAIN << " : Rather than performing the action, explain what would have been done." << std::endl;
  out << OPTION_GENERATE_SED_SCRIPT << "[=updateVersion.sed] : Generate a substitution script suitable for use with 'sed'" << std::endl;
  out << OPTION_USE_WILDCARD_FOR_VERSION << " : Within the script, use a wildcard for version, so other versions not present" << std::endl
      << "  in the processed files will be substituted, when the script is used." << std::endl; 
  out << std::endl;
  out << "All other arguments should be directories to be processed. Every file within those" << std::endl;
  out << "directories will be checked whether it is a JSon-style TagGroup definition." << std::endl;
  out << "Note:" << std::endl;
  out << " Original files are left alone(*). Within a context, the highest version is picked, " << std::endl;
  out << " and bumped to the next available version. Therefore there is never a conflict." << std::endl;
  out << " All TagGroups within the context will be given the new calculated version." << std::endl;
  out << " Finally, new files will be written to directory/context/version/nameTagGroup.json" << std::endl;
  out << " If the directory name and the context are identical, the resulting file will be" << std::endl
      << "  directory/version/nameTagGroup.json instead. This avoids unwanted recursion." << std::endl;
  out << " (*) : if you set the version yourself, and this version is present in your current structure," << std::endl
     <<    "       files will be overwritten if they follow the naming convention above. However, as the " << std::endl
     <<    "       read and write are not done simultaneously, you can use this to pretty-print your JSon." << std::endl;

}

static int determinePadding(const std::string& paddingArg, std::vector<int>& padding) {
     
  int specifiedPadding = paddingArg.find_first_of("=");
  if (specifiedPadding < 0)
    return 0;

    std::string paddingStr = paddingArg.substr(specifiedPadding+1);

   for (int i = 0; i < LAST_BUMP_LEVEL; ++i) {
      if (paddingStr.size() == 0)
        return i;
      int separator = paddingStr.find_first_not_of(DIGITS);
      if (separator < 0)
        {
          padding[i] = get_value_only(paddingStr);
          paddingStr.clear();
        }
      else
        {
          padding[i] = get_value_only(paddingStr.substr(0,separator));
          paddingStr = paddingStr.substr(separator+1);
        }
   }

   return LAST_BUMP_LEVEL; 
}

TagGroupVersionBumperOptions::TagGroupVersionBumperOptions(int argc, const char *argv[]) {
  for (int i = 0; i < LAST_BUMP_LEVEL; ++i)
     whatPadding.push_back(0);

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (OPTION_HELP.find(arg) == 0) {
      provideHelp = true;
      directories.clear();
      return;
    }
    else
    if (OPTION_LIST_VERSIONS_ONLY.find(arg) == 0) {
       listVersionsOnly = true;
       continue;
    } else 
    if (OPTION_DEBUG.find(arg) == 0) {
      spdlog::default_logger()->set_level(spdlog::level::debug);
      continue;
    } else 
    if (OPTION_QUIET.find(arg) == 0) {
      spdlog::default_logger()->set_level(spdlog::level::critical);
      continue;
    } else
    if (OPTION_BUMP_MAJOR.find(arg) == 0) {
      bumpLevel = MAJOR;
      continue;
    } else
    if (OPTION_BUMP_MINOR.find(arg) == 0) {
      bumpLevel = MINOR;
      continue;
    } else
    if (OPTION_BUMP_PATCH.find(arg) == 0) {
      bumpLevel = PATCH;
      continue;
    } else
    if (OPTION_EXPLAIN.find(arg) == 0) {
      explainActions = true;
      continue;
    } else
    if (OPTION_PADDING.find(arg) == 0) {
      padding = true;
      determinePadding(arg,whatPadding);
      continue;
    } else
    if (OPTION_NO_PADDING.find(arg) == 0) {
      padding = false;
      continue;
    } else
    if (OPTION_DEPTH.find(arg) == 0) {
      int level_start = arg.find_first_of(DIGITS);
      if (level_start < 0)
      spdlog::default_logger()->error("{} should have been of the form {}=level (where level = 0..3, using default.", OPTION_DEPTH, OPTION_DEPTH);
      else
      {
        int level_end = arg.find_first_not_of(DIGITS, level_start);
        if (level_end < 0)
          desiredLevel = (bump_level)get_value_only(arg.substr(level_start));
        else
          desiredLevel = (bump_level)get_value_only(arg.substr(level_start, level_end - level_start));
      }
      continue;
    } else if (OPTION_VERSION_OVERRIDE.find(arg) == 0) {
      int version_start = arg.find_first_of('=');
      if (version_start < 0)
      spdlog::default_logger()->error("{} should have been of the form {}=version (where version is the desired version string).", OPTION_VERSION_OVERRIDE, OPTION_VERSION_OVERRIDE);
      else
      {    
         versionOverride = arg.substr(version_start+1);
      }
      continue;
    } else if (OPTION_GENERATE_SED_SCRIPT.find(arg) == 0) {
      int filename_start = arg.find_first_of('=');
      if (filename_start > 0)
      {    
         sedSubstitionScript = arg.substr(filename_start+1);
      }
      generateSubstitutionScript = true;
      continue;
    } else if (OPTION_USE_WILDCARD_FOR_VERSION.find(arg) == 0) {
      useWildCardForVersion = true;
      continue;
    }

      directories.push_back(arg);
  }
}

struct TagGroupInfo {
   std::string directory;
   std::string filename;
   std::string name;
   std::string context;
   std::string version;
   std::string id;
   version_info info;
   std::string qosProfile;
   //rapidjson::Document json;
   nlohmann::json jsonDoc;
   int jsonIndex;

   bool operator <(const TagGroupInfo& tgi) const {
     int cmp = filename.compare(tgi.filename);
     if (cmp != 0) return (cmp < 0);
     cmp = context.compare(tgi.context);
     if (cmp != 0) return (cmp < 0);
     cmp = name.compare(tgi.context);
     if (cmp != 0) return (cmp < 0);
     if (info < tgi.info)
       return true;
     if (tgi.info < info)
       return false;

     cmp = qosProfile.compare(tgi.qosProfile);
     if (cmp != 0) return (cmp < 0);

     cmp = jsonDoc.dump().compare(tgi.jsonDoc.dump());
     if (cmp != 0) return (cmp < 0);
     cmp = jsonIndex - tgi.jsonIndex;
     return (cmp < 0);
   }
};

class
  TagGroupVersionBumper
{
  private:
    std::set<TagGroupInfo> tagGroups;
    std::set< std::string > contexts;
    std::map< std::string, std::set< TagGroupInfo > > contextToTagGroups;
    std::map< std::string, version_info > contextToMaxVersion;
    //std::map< std::string, std::set<TagGroupInfo> > filenameToTagGroupMap; // set needed: may be multiple taggroups per file
    //std::map< std::string, std::set<std::string> > contextToFilenameMap;
    //std::map< std::string, std::set<std::string> > contextToVersion;
    //std::map< version_info, std::map< std::string, std::set<std::string> > > versionToContextAndTagGroups;

    static bool isTagGroup(nlohmann::json& doc);

    void processFile(const std::string& directory, const std::string& filename);
    void processTagGroup(const std::string& directory, const std::string& filename, nlohmann::json& object,nlohmann::json& topLevel, int index = -1);

  public:
    TagGroupVersionBumper();
    void processFiles(const std::string& directory);
    void dumpCurrentStatus(std::ostream& out);
    std::string bumpVersion(const TagGroupVersionBumperOptions& options);
    void writeTagGroup(const TagGroupInfo& tgi, const TagGroupVersionBumperOptions& options, const std::string& next_version, const nlohmann::json& json);
};

int
main (int argc, const char *argv[])
{   
  TagGroupVersionBumperOptions options(argc,argv);
  if (options.shouldProvideHelp()) {
    options.displayHelp(std::cout);
    return 0;
  }

  std::stringstream substitutionStream;

    for (const auto& dir : options.getDirectories()) {

    TagGroupVersionBumper processor;  
        
    processor.processFiles(dir);
    if (options.shouldListVersionsOnly()) {
      processor.dumpCurrentStatus(std::cout);
    }
    else
    { 
      auto&& substitutionScript = processor.bumpVersion(options);
      if (options.shouldGenerateSubstitutionScript())
        substitutionStream << substitutionScript;
    }
    
  }

  if (options.shouldGenerateSubstitutionScript()) {
    try {

      std::ofstream substitutionFile(options.getSedScriptFilename());

      substitutionFile << substitutionStream.str();
      substitutionFile.flush();
      substitutionFile.close();
    } catch (std::exception& ex) {
      spdlog::default_logger()->error("While writing substitutionfile {}, caught exception {}",options.getSedScriptFilename(),ex.what());
    }
  }
  return 0;
}

TagGroupVersionBumper::TagGroupVersionBumper() {

}

 //std::map< std::string, std::set<TagGroupInfo> > filenameToTagGroupMap; // set needed: may be multiple taggroups per file
   // std::map< std::string, std::set<std::string> > contextToFilenameMap;
   // std::map< std::string, std::set<std::string> > contextToVersion;
void TagGroupVersionBumper::processTagGroup(const std::string& directory, const std::string& filename, nlohmann::json& object, nlohmann::json& topLevel, int index) {
   TagGroupInfo tgi;

   tgi.directory = directory;
   tgi.filename = filename;
   tgi.name = object[TAGGROUP_NAME];
   tgi.context = object[TAGGROUP_CONTEXT];
   tgi.version = object[TAGGROUP_VERSION];
   tgi.id = tgi.name + ":" + tgi.context + ":" ;
   tgi.info.initialize(tgi.version);
   tgi.qosProfile = object[TAGGROUP_QOS_PROFILE];
   tgi.jsonDoc = topLevel;
   tgi.jsonIndex = index;

   tagGroups.insert(tgi);
   contexts.insert(tgi.context);

    auto contextToMaxVersionPosition = contextToMaxVersion.find(tgi.context);

    if (contextToMaxVersionPosition == contextToMaxVersion.end()) {
      contextToMaxVersion.insert(std::make_pair(tgi.context, tgi.info));
    }
    else {
      if (!(tgi.info < contextToMaxVersionPosition->second))
        contextToMaxVersionPosition->second = tgi.info;
    }

    auto contextToTagGroupsPosition = contextToTagGroups.find(tgi.context);

    if (contextToTagGroupsPosition == contextToTagGroups.end()) {
      std::set< TagGroupInfo > tagGroupSet = { tgi };
      contextToTagGroups.insert(std::make_pair(tgi.context, tagGroupSet));
    } else
      contextToTagGroupsPosition->second.insert(tgi);
}

void TagGroupVersionBumper::processFiles(const std::string& directory) {
  if (!boost::filesystem::exists(directory)) {
    spdlog::default_logger().get()->error("{} does not exist.",directory);
    return;
  }
  if (!boost::filesystem::is_directory(directory)) {
    spdlog::default_logger().get()->error("{} is not a directory.",directory);
    return;
  }

  boost::filesystem::recursive_directory_iterator fileIterator(directory);
  boost::filesystem::recursive_directory_iterator endOfIteration;

  while (fileIterator != endOfIteration) {
    if (boost::filesystem::is_regular_file(fileIterator->path())) {
      processFile(directory, fileIterator->path().string());
    }
    boost::system::error_code ec;

    fileIterator.increment(ec);

    if (ec) {
      spdlog::default_logger().get()->error("Could not access {}, because of {}",fileIterator->path().string(),ec.message());
    }

  }

}



bool TagGroupVersionBumper::isTagGroup(nlohmann::json& object) {

  if (object.size() < keysOfTopLevelTagGroup.size())
    return false;

  for (const auto& i : keysOfTopLevelTagGroup) {
  const auto& objectKey = object.find(i);
  
  if (objectKey == object.end()) {
      spdlog::default_logger().get()->debug("Checking {} to see if it is a TagGroup header, but mandatory field <{}> is not present.",object.dump(2),i);
    return false;
  }
    if (!objectKey->is_string()) {
      spdlog::default_logger().get()->debug("Checking {} to see if it is a TagGroup header, but mandatory field <{}> is not a string as expected.",object.dump(2),i);
      return false;
    }
  }

  for (const auto& i : eitherOrKeysOfTopLevelTagGroup)  {
     const auto& objectKey = object.find(i);
     if (objectKey != object.end())
       return true;
  }

   return false;
}

void TagGroupVersionBumper::dumpCurrentStatus(std::ostream& out) {
  for (const auto& context : contexts) {
    out << context << " : ";
    out << contextToMaxVersion.find(context)->second << " :";
    const auto& tagGroups = contextToTagGroups.find(context)->second;

    for (const auto tagGroup : tagGroups) {
       out << " " << tagGroup.name << " " << tagGroup.version;
    }

    out << std::endl;
  }
}



void TagGroupVersionBumper::processFile(const std::string& directory, const std::string& filename) {
  spdlog::default_logger().get()->debug("Parsing {}",filename);
  std::ifstream inputFile(filename);
  //if (inputFile.bad()) {
  //  spdlog::default_logger().get()->error("Error opening '{}'",filename,);
  //  return;
  //}
  nlohmann::json d;

  try {
  d = nlohmann::json::parse(inputFile);
  } catch (nlohmann::json::parse_error& pe) {
     spdlog::default_logger().get()->debug("Parse error parsing '{}': at offset {}: error message: {}",filename,pe.byte,pe.what());
     //spdlog::default_logger().get()->warn("{}: file not parseable.",filename);

     return;  
 }
  
  if (d.is_object()) {
    // singular TagGroup, no typedefinitions
  
    if (!isTagGroup(d)) {
      spdlog::default_logger().get()->debug("{}: JSon (Object): Not A TagGroup {}",filename,d.dump());
      //spdlog::default_logger().get()->error("{}: JSon (Object) parsed OK, but does not seem to be a TagGroup.",filename);
      return;
    }
    processTagGroup(directory,filename,d,d);

    return;
  }

  if (!d.is_array()) {
   spdlog::default_logger().get()->debug("{}: JSon, but not an array: {}",filename, d.dump());
   //spdlog::default_logger().get()->error("{}: JSon (Array) parsed OK, but does not seem to be a TagGroup.",filename);
   return;
  }

  for (int i = 0; i < d.size(); ++i) {
    if (d[i].is_object() && isTagGroup(d[i]))
      processTagGroup(directory,filename,d[i],d,i);
  }   
}


#include <iomanip>
#include <sstream>

static void add_value(std::ostream& o, int value, int padding) {
  o << std::setw(padding);
  o << std::setfill('0') ;
  o << value;
}




version_info::version_info(const std::string& orig_version){
  initialize(orig_version);
}

#define ENABLE_TESTING

void version_info::initialize(const std::string& orig_version) {
   for (int i = 0; i< LAST_BUMP_LEVEL; ++i) {
     value[i] = 0;
     padding[i] = 0;
     position[i] = 0;
     endOfPosition[i] = 0;
   }

   int current_pos = 0;

   auto version = orig_version;

   for (int i = MAJOR; i < LAST_BUMP_LEVEL; ++i) {
       if (current_pos >= version.size())
         position[i] = -1;
       else
         position[i] = version.find_first_of(DIGITS,current_pos);

     if (position[i] < 0) {
       // end of the line
       
       if (i == MAJOR) {
         actual_level = MAJOR;
         prefix = orig_version;
       }
       else {
         actual_level = (bump_level)(i-1);
         prefix = version.substr(0,position[MAJOR]);
       }
       break;
     }
     endOfPosition[i] = version.find_first_not_of(DIGITS,position[i]);
     if (endOfPosition[i] < 0)
       endOfPosition[i] = version.size();

     value[i] = get_value(version.substr(position[i],endOfPosition[i] - position[i]),padding[i]);
     actual_level = (bump_level) i;
     current_pos = endOfPosition[i];
     
   }
}

bool version_info::operator <(const version_info& other) const  {
    for (int i = MAJOR; i < LAST_BUMP_LEVEL; ++i) {
      if (value[i] < other.value[i])
        return true;

      if (value[i] > other.value[i]) 
        return false;
    }

    return (prefix.compare(other.prefix) < 0);
}

std::string version_info::to_string() const {
   std::stringstream s;

   std::string separator = prefix;

   for (int i = 0; i <= actual_level; ++i) {

      s << separator; separator = ".";
      add_value(s, value[i], padding[i]);
   }

   return s.str();
}

#undef ENABLE_TESTING


static std::string calculateNextVersion(const version_info& orig_vi, bump_level bumpLevel, bump_level desiredLevel, const std::vector<int>& padding) {
   //version_info vi(orig_version);

   version_info vi(orig_vi);

   vi.value[bumpLevel]++;
   for (int i = bumpLevel+1; i < LAST_BUMP_LEVEL; ++i) {
     vi.value[i] = 0;
   }

   if (vi.actual_level < bumpLevel)
     vi.actual_level = bumpLevel;

   if (vi.actual_level < desiredLevel)
     vi.actual_level = desiredLevel;


   for (int i = 0 ; i < padding.size(); ++i) {
      if (i >= LAST_BUMP_LEVEL)
        break;
     vi.padding[i] = padding[i];
   }

   return vi.to_string();
  }

#ifdef ENABLE_TESTING
static std::vector< std::vector<int> > testPadding = 
{
  { 0,0,0 },
  { 0,0,1 },
  { 0,1,0 },
  { 1,0,0 },
  { 1,1,1 },
  { 1,2,3 },
  { 3,6,9 },
  { 3,0,0 },
  { 0,3,0 },
  { 0,0,3 }
};

static std::vector<std::string> testVersions = {
  "version",
  "v1",
  "v1.0",
  "v1.0.0",
  "1",
  "2",
  "3",
  "9",
  "01",
  "02",
  "03",
  "09",
  "10",
  "99",
  "999"
  "1.0",
  "1.1",
  "1.2",
  "1.9",
  "2.0",
  "2.1",
  "2.2",
  "2.3",
  "2.9",
  "2.01",
  "2.03"
  "02.01",
  "02.03",
  "02.99",
  "99.99",
  "987.789",
  "1234.4321",
  "v1.0",
  "v1.99",
  "v01.99",
  "v99.99",
  "1.0.0",
  "1.0.1",
  "1.0.9",
  "1.2.3",
  "1.2.3.9",
  "1.2.4",
  "1.02.01",
  "1.02,04",
  "1.99.99",
  "99.999,999",
  "pre-99.99.99",
  "pre-98.98.98"
};
#endif 


std::string TagGroupVersionBumper::bumpVersion(const TagGroupVersionBumperOptions& options) {
  //std::map< std::string, std::string >
#ifdef ENABLE_TESTING
  for (auto& version: testVersions) {
    for (auto& padding : testPadding) {
       for (int i = 0; i < LAST_BUMP_LEVEL; ++i) {
          for (int j = 0; j < LAST_BUMP_LEVEL; ++j) {
           std::cout << version << ": " << padding[MAJOR] << "," << padding[MINOR] << "," << padding[PATCH];
           std::cout << "(BUMP:" << i << ", DESIRED:" << j << ") :" <<
                    calculateNextVersion(version,(bump_level)i,(bump_level)j,padding) << std::endl;
       }
    }
    }
  }
     
  std::cout << "Actual data:" << std::endl;
#endif
  //auto & lastVersion versionToContextAndTagGroups.

  std::stringstream substitutionRules;
  
  for (const auto& context : contexts ) {  
    const auto& version = contextToMaxVersion.find(context)->second;
    auto next_version = options.hasVersionOverride() 
                          ? options.getVersionOverride() 
                          : calculateNextVersion(version,options.getBumpLevel(),options.getDesiredLevel(), options.getPadding());                                  

    if (options.shouldExplainActions()) {
      spdlog::default_logger()->info("BumpVersion: The 'biggest' version value of {} is {}.",context,version.to_string());
      if (options.hasVersionOverride()) {
        spdlog::default_logger()->info("BumpVersion: Because you have specified a fixed version of {}, this will be used as the value.",next_version);
      } else {
        spdlog::default_logger()->info("BumpVersion: Based on the provided options, the calculated version to use for all TagGroups within this context is {}.",next_version);
      }

    }
    const auto& taggroups = contextToTagGroups.find(context)->second;

    for (const auto& taggroup : taggroups) {
      
      if (taggroup.jsonIndex > 0) // will be processed at earlier (jsonIndex == 0) stage
        continue;

        if (options.shouldExplainActions()) {
           spdlog::default_logger()->info("BumpVersion: We are working on the JSon found in the original filename {}.",taggroup.filename);
           spdlog::default_logger()->debug("BumpVersion: This is the current version: {}",taggroup.jsonDoc.dump(2));        
        }

      
      auto jsonDoc = taggroup.jsonDoc; // copy for modification
      if (jsonDoc.is_object())
        jsonDoc[TAGGROUP_VERSION] = next_version;
      else // must be array
        {
           for (int i = 0; i < jsonDoc.size(); ++i) {
              if (jsonDoc[i].is_object() && (jsonDoc[i].find(TAGGROUP_VERSION) != jsonDoc[i].end()))
                jsonDoc[i][TAGGROUP_VERSION] = next_version;
           }
        }

        if (options.shouldExplainActions()) {
          spdlog::default_logger()->debug("BumpVersion: This is the updated version: {}",jsonDoc.dump(2));
        }
        
       writeTagGroup(taggroup,options,next_version,jsonDoc);

       if (options.shouldGenerateSubstitutionScript()) {
         substitutionRules << "s/"
                           << taggroup.id << (options.shouldUseWildcardForVersion() ? "[-.0-9a-zA-Z_]*" : taggroup.version)
                           << "/"
                           << taggroup.id << next_version
                           << "/g" << std::endl;
       }

  }   

  
  
}
   return substitutionRules.str();
}


void TagGroupVersionBumper::writeTagGroup(const TagGroupInfo& tgi, const TagGroupVersionBumperOptions& options, const std::string& version, const nlohmann::json& json) {
     std::stringstream fs;
     if (tgi.directory.compare(tgi.context) != 0) { // the directory is not equal to the context
       fs << tgi.directory << "/";
     }

      fs << tgi.context << "/" << version << "/" << tgi.name << "TagGroup.json";

      std::string filename = fs.str();

      if (options.shouldExplainActions()) {
        spdlog::default_logger()->info("WriteTagGroup: from passed parameters, the filename to be written is {}",filename);
        spdlog::default_logger()->debug("WriteTagGroup: The json to be written is {}", json.dump(2));
        return;
      }

     try {
      std::ofstream outputFile(filename);

      outputFile << json.dump(2);
      outputFile.flush();
      outputFile.close();
     } catch (std::exception& ex) {
       spdlog::default_logger()->error("WriteTagGroup: could not write JSon to {}, because of {}",filename, ex.what());
     }
  }
