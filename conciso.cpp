/*
* Conciso - Lightweight Markdown blogin
* @Author: Pedro Henrique
* @Date:   2016-06-22 17:53:40
* @Last Modified by:   Pedro Henrique
* @Last Modified time: 2016-07-11 10:26:19
*/


/*

- Generate Aspects
TODO(Pedro): improve the parse and metadata expand functionality
make something more modular

OpenMDFiles()
parseHeaders() and UpdateMDFilesWithConstantData() like {{ date }}

insertTemplateOnFiles()
expandMetadaDataOnFiles() now that we have parsed the header of the files we can have generateCategoryList(), postFromCategory() and printJSON("categories")
or something like printJSONCategories() printJSONURL() printJSON() or something like that
writeFiles()

- Server
Create a simple server

- WatchForChangesOnMDFiles

- Deploy

write configuration file on create

use regex for parse
i guess it's a better way to handle this

improve the configuration file
maybe something like
title: PedroHenrique.ninja

Would be nice to be able to do:
{{ title | "Replacement title" }}
{{ include "file.md" }}
{{ include "header.html" }}

{{ include "footer.html" }} 
*/

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstdint>
#include <cerrno>
#include <cassert>
#include <sys/stat.h>
#include <dirent.h>
#ifdef __WIN32
    #include <direct.h> // char    *_getcwd(char *, size_t);
    #define _GetCurrentDir _getcwd
#else
    #include <unistd.h> // char    *getcwd(char *, size_t);
    #define _GetCurrentDir getcwd 
#endif

#include "DiscountCpp.hpp" // markdown



/** DEFINES **/
#define DEFAULT_CONFIG_FILE "project.conciso"
#define DEFAULT_HEADER_METADATA "[conciso]: <> ("
#define DEFAULT_DATE_FORMAT "%X %d/%m/%Y"


namespace Conciso {


const std::string RESET("\033[0m");
const std::string RED("\033[0;31m");
const std::string GREEN("\033[1;32m");
const std::string YELLOW("\033[1;33m");
const std::string BLUE("\033[1;34m");
const std::string MAGENTA("\033[0;35m");
const std::string CYAN("\033[0;36m");
const std::string BOLD("\x1B[1m");
const std::string UNDERLINE("\x1B[4m");


// TODO(Pedro): Windows version?
#define TEXT_RED(x) RED << x << RESET
#define TEXT_GREEN(x) GREEN << x << RESET
#define TEXT_YELLOW(x) YELLOW << x << RESET
#define TEXT_BLUE(x) BLUE << x << RESET
#define TEXT_MAGENTA(x) MAGENTA << x << RESET
#define TEXT_CYAN(x) CYAN << x << RESET

#define TEXT_BOLD(x) BOLD << x << RESET
#define TEXT_UNDERLINE(x) UNDERLINE << x << RESET


/** Typedefs **/
typedef std::uint8_t uint8;
typedef std::int8_t int8;

typedef std::uint16_t uint16;
typedef std::int16_t int16;

typedef std::uint32_t uint32;
typedef std::int32_t int32;

typedef std::uint64_t uint64;
typedef std::int64_t int64;


struct Metadata {
    std::string name;
    std::string value;
};

struct ConcisoTemplate {
    std::string name; // this is the name without the file extension
    std::string code;
};

struct ConcisoProject {
    std::string name;
    std::string url;
    std::string basePath;
    std::string contentPath;
    std::string outputPath;
    ConcisoTemplate defaultTemplate; // we only keep the defaultTemplate on the memory, all the others templates we load on the fly
    std::vector<Metadata> metadata;
};

struct ConcisoFile {
    std::string name;
    std::string content;
    std::string category;
    std::string templateHTML;
    std::string date;
    std::string author;
    std::string title;
    std::string description;
    std::string keywords;
    bool visible;    
};

// Regex to macth field's 
// {{\s*([a-zA-Z0-9_-]+)\s*}}  g

class Utils {
public:
    static std::string regexPattern;
    static std::regex regexTest;

    static void replacePlaceHolder(const std::string &placeHolder, const std::string &replace, std::string &content) {
        if (Utils::hasPlaceHolder(content)) {
            std::string customRegex = "\\{\\{\\s*(" + placeHolder + ")\\s*\\}\\}";
            std::regex customRegexTest(customRegex);

            content = std::regex_replace(content, customRegexTest, replace);
        }
    }

    static bool hasPlaceHolder(const std::string &content) {
        return std::regex_search(content, Utils::regexTest);
    }

    static std::vector<std::string> getPlaceHolderMatches(const std::string &content) {
        std::string checkString = content;

        std::vector<std::string> matches;

        std::smatch match;

        while (std::regex_search(checkString, match, Utils::regexTest)) {
            matches.push_back(match[1].str());
            checkString = match.suffix();
        }

        return matches;
    }

    static std::vector<std::string> convertFileContentsToLines(const std::string &content) {
        std::vector<std::string> lines;

        std::size_t pos  = 0;
        std::size_t prev = 0;
        while ((pos = content.find('\n', prev)) != std::string::npos) {
            lines.push_back(content.substr(prev, pos - prev));
            prev = pos + 1;
        }

        // for the last line
        lines.push_back(content.substr(prev));

        return lines;
    }

    static std::string getAbsolutePathFromString(const std::string &string) {
        std::string result;
        std::size_t posLastSlash;

        posLastSlash = string.find_last_of("/");

        if (posLastSlash != std::string::npos) {
            result = string.substr(0, posLastSlash);
        } else {
            std::cout << "Coul'd not get the absolute path from the string: " << string << std::endl;
        }

        return result;
    }

    static std::vector<std::string> getFilesFromType(const std::vector<std::string> &files, const std::string &type) {
        std::vector<std::string> filesFromType;

        std::size_t posDotType = type.find_last_of(".");
        std::string typeSafe   = type.substr(posDotType + 1);

        for (uint32 index = 0; index < files.size(); index++) {
            std::size_t posDot   = files[index].find_last_of(".");
            std::string fileType = files[index].substr(posDot + 1);

            if (fileType == typeSafe) {
                filesFromType.push_back(files[index]);
            }
        }

        return filesFromType;
    }

    static std::string getCurrentTime() {
        std::time_t now = std::time(0);
        struct tm tdata;
        char buffer[100];
        tdata = *localtime(&now);

        std::strftime(buffer, sizeof(buffer), DEFAULT_DATE_FORMAT, &tdata);

        return std::string(buffer);
    }

    static std::string getDirNameFromString(const std::string &string) {
        std::string result;
        std::size_t posLastSlash;

        posLastSlash = string.find_last_of("/");

        if (posLastSlash != std::string::npos) {
            result = string.substr(posLastSlash + 1);
        } else {
            std::cout << "Coul'd not get the directory name from the string: " << string << std::endl;
        }

        return result;
    }

    static std::string getCurrentWorkingDir() {
        char buffer[FILENAME_MAX + 1];
        _GetCurrentDir(buffer, FILENAME_MAX);
        assert(errno == 0);
        buffer[FILENAME_MAX - 1] = '\0';
        return std::string(buffer); 
    }

    static std::string getFileContent(const std::string &fileName) {
        std::ifstream file(fileName.c_str());

        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            return buffer.str();
        } else {
            std::cout << "Coul'd not open the file: " << fileName << std::endl;
        }

        return ""; 
    }

    static void writeFileToDisk(const std::string &fileName, const std::string &fileContents) {
        std::fstream file;

        file.open(fileName.c_str(), std::ios::out|std::ios::binary);

        if (file.is_open()) {
            file << fileContents;
            file.close();
        } else {
            std::cout << "Coul'd not open the file: " << fileName << " to write" << std::endl;
        }
    }

    // keep in mind that's not a recursive function
    // at this time we don't support subfolder's on the content folder
    static std::vector<std::string> getListFilesOnDirectory(const std::string &path) {
        std::vector<std::string> files;

        DIR *dirPointer   = 0;
        dirent *fileEntry = 0;

        dirPointer = opendir(path.c_str());

        if (dirPointer) {
            while ((fileEntry = readdir(dirPointer))) {
                std::string file = (fileEntry->d_name);

                if (fileEntry->d_type != DT_DIR) {
                    files.push_back(file);
                }
            }
        } else {
            std::cerr << "Error opening the Directory: " << path << std::endl;
        }
        return files;
    }

    static std::string toLower(std::string &string) {
        std::transform(string.begin(), string.end(), string.begin(), tolower);
        return string;
    }

    static std::string removeSpaces(std::string &string) {
        string.erase(std::remove(string.begin(), string.end(), ' '), string.end());
        return string;
    }

    static std::string getFileNameWithoutExtension(const std::string &fileName) {        
        std::size_t posDot = fileName.find_last_of(".");
        return fileName.substr(0, posDot);
    }

    static bool fileExists(const std::string &fileName) {
        struct stat buffer;
        return (stat (fileName.c_str(), &buffer) == 0); 
    }

private:
    Utils() {

    }
};

// Static class elements
std::string Utils::regexPattern = std::string("\\{\\{\\s*([a-zA-Z0-9_-]+)\\s*\\}\\}");
std::regex Utils::regexTest     = std::regex(Utils::regexPattern);

struct ConfigurationEntry {
    std::string key;
    std::string value;
};

class ConfigurationFile {
    static std::vector<ConfigurationEntry> mConfigurations;
public:

    static void save(cont std::string &fileName, const ConcisoProject &project) {
        std::string file;
        file << "# Conciso Project Auto-gen file" << std::endl;
        file << "name: " << project.name << std::endl;
        file << "url: " << project.name << std::endl;
        file << "templateHTML: " << project.name << std::endl;
        file << "content: " << project.name << std::endl;
        file << "output: " << project.name << std::endl;
        file << "# insert variables below" << std::endl;
        file << "# title: My Title" << std::endl;

        Utils::writeFileToDisk(fileName, file);
    }

    static void clear() {
        ConfigurationFile::mConfigurations.clear();
    }

    static void setProject(ConcisoProject &project) {
        for (uint32 index = 0; index < ConfigurationFile::mConfigurations.size(); index++) {
            ConfigurationEntry entry = ConfigurationFile::mConfigurations[index];

            if (entry.key == "name") {
                project.name = entry.value;
            } else if (entry.key == "url") {
                project.url = entry.value;
            } else if (entry.key == "basepath") {
                project.basePath = entry.value;
            } else if (entry.key == "content") {
                project.contentPath = entry.value;
            } else if (entry.key == "output") {
                project.outputPath = entry.value;
            } else if (entry.key == "templatehtml") {
                ConcisoTemplate tmpl;
                tmpl.name               = "defaultTemplate";
                tmpl.code               = Utils::getFileContent(entry.value);
                project.defaultTemplate = tmpl;
            }
        }

        if (!project.basePath.empty()) {
            project.basePath    =  Utils::getCurrentWorkingDir();
            project.contentPath = project.basePath + "/" + project.contentPath;
            project.outputPath  = project.basePath + "/" + project.outputPath;
        }
    }

    static void load(const std::string &fileName) {
        std::string contents           = Utils::getFileContent(fileName);
        std::vector<std::string> lines = Utils::convertFileContentsToLines(contents);

        for (uint32 index = 0; index < lines.size(); index++) {
            std::string line = lines[index];
            if (line[0] == '#') {
                continue;
            }

            std::size_t hasComments = line.find_first_of("#");

            if (hasComments != std::string::npos) {
                line = line.substr(0, hasComments);
            }

            // quick check
            std::string lineWithoutSpace = line;
            lineWithoutSpace = Utils::removeSpaces(lineWithoutSpace);
            if (line.size() < 2) {
                continue;
            }

            std::size_t hasColon = line.find_first_of(":");

            if (hasColon == std::string::npos) {
                std::cerr << RED << "Could not parse the configuration file on line " << RED << index << RED << ":" << RED << " " << line << RESET << std::endl;
                std::cerr << RED << "You must use " << TEXT_BOLD("OPTION: VALUE") << RESET << std::endl;
                exit(0);
            }

            ConfigurationEntry config;

            config.key = line.substr(0, hasColon);
            config.key = Utils::removeSpaces(config.key);
            config.key = Utils::toLower(config.key);

            config.value = line.substr(hasColon + 1);

            if (config.value[0] == ' ') {
                config.value =  config.value.substr(1);
            }

            ConfigurationFile::mConfigurations.push_back(config);
        }
    }
};

std::vector<ConfigurationEntry> ConfigurationFile::mConfigurations;


class ExpandMetadata {
    static void expandGlobalMetadata(std::string &line) {
        // expand the project metadata
    }

    static void expandDynamicMetadata(std::string &line) {
        // expand the metadata like time, date and stuff
        // {{\s*([A-Za-z0-9|\s"]+)\s*}} for the string below
        // include or the {{ SOME | "Another thing "}}
        // {{\s*([A-Za-z0-9|\s"\.]+)\s*}} <-- for include 
    }
};



static void printUsage() {
    std::cout << std::endl << "Usage: conciso [-C MyProject.conciso] [-c MyProject.conciso] [-d] [-s] " << std::endl << std::endl;

    std::cout << "Create a new Conciso project" << std::endl;
    std::cout << "    " << "create " << TEXT_UNDERLINE("conciso_file") << std::endl;
    std::cout << "    " << "--create " << TEXT_UNDERLINE("conciso_file") << std::endl;
    std::cout << "    " << "-C " << TEXT_UNDERLINE("conciso_file") << std::endl << std::endl;

    std::cout << "Define the configuration file" << std::endl;
    std::cout << "    " << "--config " << TEXT_UNDERLINE("conciso_file") << std::endl;
    std::cout << "    " << "-c " << TEXT_UNDERLINE("conciso_file") << std::endl << std::endl;

    std::cout << "Create a local server on the output folder" << std::endl;
    std::cout << "    " << "server" << std::endl;
    std::cout << "    " << "--server" << std::endl;
    std::cout << "    " << "-s" << std::endl << std::endl;

    std::cout << "Deploy the project to the configured method" << std::endl;
    std::cout << "    " << "deploy" << std::endl;
    std::cout << "    " << "--deploy" << std::endl;
    std::cout << "    " << "-d" << std::endl << std::endl;

}

class CommandLine {
public:
    struct CommandLineArg {
        std::string name;
        std::string value;
    };

private:
    CommandLine () {

    }

    static std::vector<CommandLineArg> mArgs;
public:
    
    static void parse(int argc, char **argv) {

        // argv[0] == program name
        for (uint32 index = 1; index < argc; index++) {
            std::string argument(argv[index]);

            if (argument == "--config" || argument == "-c") {
                // set the configuration file
                CommandLineArg arg;
                arg.name  = "config";
                
                if ((index + 1 < argc) && (argv[index + 1][0] != '-')) {
                    arg.value = std::string(argv[index + 1]);
                    index++; // we increment another time since we get the value from the argument array
                    CommandLine::mArgs.push_back(arg);
                } else {
                    std::cerr << TEXT_RED("You must provide the configuration file name") << std::endl;
                    printUsage();
                    exit(0);
                }

            } else if (argument == "create" || argument == "--create" || argument == "-C") {
                CommandLineArg arg;
                arg.name = "create";

                if (index + 1 < argc)  {
                    arg.value = std::string(argv[index + 1]);
                    index++;
                } else {
                    arg.value = DEFAULT_CONFIG_FILE;
                }  
                
                CommandLine::mArgs.push_back(arg);
                // implement this
            } else {
                std::cerr << RED << TEXT_BOLD("Unknow argument: ") << RED << argv[index] << RESET << std::endl;
                printUsage();
                exit(0);
            }
        }

    }

    static bool isOptionSet(const std::string &optionName) {
        for (uint32 index = 0; index < CommandLine::mArgs.size(); index++) {
            if (CommandLine::mArgs[index].name == optionName) {
                return true;
            }
        }

        return false;
    }

    static std::string getOptionValue(const std::string &optionName) {
        for (uint32 index = 0; index < CommandLine::mArgs.size(); index++) {
            if (CommandLine::mArgs[index].name == optionName) {
                return CommandLine::mArgs[index].value;
            }
        }

        return "";
    }
};

std::vector<CommandLine::CommandLineArg> CommandLine::mArgs;

class Conciso {
private:
    ConcisoProject mProject;
public:    

    void createProject(std::string &config) {
        if (config.empty()) {
            config = DEFAULT_CONFIG_FILE;
        }

        std::cout << "Project Name: ";
        mProject.name = CommandLine::readLine();

        std::cout << "Project URL: ";
        mProject.url = CommandLine::readLine();

        std::cout << "Content path: ";
        mProject.contentPath = CommandLine::readLine();

        std::cout << "Output path: ";
        mProject.outputPath = CommandLine::readLine();

        std::cout << "Output path: ";
        mProject.outputPath = CommandLine::readLine();

        std::cout << "Output path: ";
        ConcisoTemplate defaultTemplate;
        defaultTemplate.name     = CommandLine::readLine();
        mProject.defaultTemplate = defaultTemplate;

        ConfigurationFile::save(config, mProject);
    }


    void loadFiles() {
        // get list of files
        // load all the files
    }

    void parseFiles() {

    }

    void generateTemplate() {

    }

    void saveToDisk() {
        
    }

    Conciso(int argc, char **argv) {
        std::cout << TEXT_BLUE("Conciso - Markdown Bloggin") << std::endl;

        CommandLine::parse(argc, argv);

        if (CommandLine::isOptionSet("config")) {
            ConfigurationFile::load(CommandLine::getOptionValue("config"));
            ConfigurationFile::setProject(mProject);
        } else {
            if (Utils::fileExists(DEFAULT_CONFIG_FILE)) {
                ConfigurationFile::load(DEFAULT_CONFIG_FILE);
                ConfigurationFile::setProject(mProject);
            } else {
                printUsage();
                exit(0);
            }
        }

        if (CommandLine::isOptionSet("create")) {
            createProject(ConfigurationFile::getOptionValue("config"));
        } else {
            // try to parse the files and put on the output folder

        }


        loadFiles();
        parseFiles();
        generateTemplate();
        saveToDisk();


        // server

        // deploy 
        if (CommandLine::isOptionSet("deploy")) {

        }

    }



};

}

int main(int argc, char **argv) {
    Conciso::Conciso conciso(argc, argv);
    return 0;
}