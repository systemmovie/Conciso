class Conciso {
public:
    Conciso() : mConfigFile(DEFAULT_CONFIG_FILE) {
        mProject.path   = Utils::getCurrentWorkingDir();
        mProject.output = "output";
    }

    void printUsage() {
        // TODO(Pedro): <--
    }

    void parseArguments(int argc, char **argv) {
        if (argc > 1) {
            uint8 numberArgs = argc - 1;

            for(uint8 index = 0; index < numberArgs; index++) {
                // store the current arg and the next arg 
                // if we can
                std::string currentArg(argv[index]);
                std::string nextArg;

                if ((index + 1) < numberArgs) {
                    nextArg = std::string(argv[index + 1]);
                }

                // For now we only parse the configuration file argument
                // and create
                // conciso -c test.conciso
                // conciso --config test.conciso
                if (currentArg == "-c" || currentArg == "--config") {
                    mConfigFile = nextArg;
                    index++;
                    continue;
                }

                // conciso create teste.conciso
                // conciso --create teste.conciso
                // conciso create
                if (currentArg == "create" || currentArg == "--create") {
                    if (!nextArg.empty() && nextArg != "-c" && nextArg != "--config") {
                        mConfigFile = nextArg;
                        index++;
                    }
                    continue;
                }
            }
        }
        // else we try to read the default config file
    }

    void execute() {
        if (parseConfigurationFile()) {
            // update the content path
            mProject.content = mProject.path + "/" + mProject.content;
            mProject.output  = mProject.path + "/" + mProject.output;

            mkdir(mProject.output.c_str(), ACCESSPERMS);

            std::vector<std::string> filesFromDirectory = Utils::getListFilesOnDirectory(mProject.content);
            std::vector<std::string> markDownFiles      = Utils::getFilesFromType(filesFromDirectory, ".md");

            // this will populate our mFiles vector
            openMarkDownFiles(markDownFiles);

            // convert Markdown to HTML
            for (uint32 index = 0; index < mFiles.size(); index++) {
                expandMetadata(mFiles[index].content, mFiles[index]);
                mFiles[index].content = discountcpp::getHTMLFromMarkdownString(mFiles[index].content); 
            }

            // now that we have generated the HTML on all the files
            // we can re-parse to populate the category list
            // date and special metadata

            insertTemplateOnFiles();

            writeFiles();
        } 
    }

private:
    // TODO(Pedro): implement the categories...
    // list and menu generation and so on
    // pagination as well (or maybe leave it to user)
    // implement on javascript
    // maybe make a function to generate valid json categories
    // or even post lists

    void writeFiles() {
        for (uint32 index = 0; index < mFiles.size(); index++) {
            std::string outputFileName =  mFiles[index].name;

            std::size_t posDot = outputFileName.find_last_of(".");

            outputFileName = outputFileName.substr(0, posDot);
            outputFileName = mProject.output + "/" + outputFileName + ".html";

            Utils::writeFileToDisk(outputFileName, mFiles[index].content);
        }
    }

    void insertTemplateOnFiles() {
        // load the default template
        templateHTMLCode = Utils::getFileContent(mProject.path + "/" + mProject.templateHTML);

        for (uint32 index = 0; index < mFiles.size(); index++) {
            // if there is no template we just continue
            if (templateHTMLCode.empty() && mFiles[index].templateHTML.empty()) {
                continue;
            }

            std::string currentTemplate = templateHTMLCode;

            if (!mFiles[index].templateHTML.empty()) {
                currentTemplate = Utils::getFileContent(mProject.path + "/" + mFiles[index].templateHTML);
            }

            ConcisoFile stubFile;
            expandMetadata(currentTemplate, stubFile);
            expandTemplateMedata(currentTemplate, mFiles[index]);

            std::size_t posContentReplace;
            std::string searchPattern = "&lbrace;&lbrace;conciso_content&rbrace;&rbrace;";
            posContentReplace = currentTemplate.find(searchPattern);

            if (posContentReplace == std::string::npos) {
                std::cout << "The template from file " << mFiles[index].name << " does not contain {{ conciso_content }} marker" << std::endl;
                continue;
            }

            std::string firstPartTemplate  = currentTemplate.substr(0, posContentReplace);
            std::string secondPartTemplate = currentTemplate.substr(posContentReplace + searchPattern.size());

            mFiles[index].content = firstPartTemplate + mFiles[index].content + secondPartTemplate;
        }

    }

    void expandTemplateMedata(std::string &content, ConcisoFile &file) {
        // handle the custom metadata data from the file, like title, and stuff

    }

    void expandMetadata(std::string &content, ConcisoFile &file) {
        // convert to an array of lines
        std::vector<std::string> fileLines;

        std::size_t pos  = 0;
        std::size_t prev = 0;
        while ((pos = content.find('\n', prev)) != std::string::npos) {
            fileLines.push_back(content.substr(prev, pos - prev));
            prev = pos + 1;
        }
        // for the last line
        fileLines.push_back(content.substr(prev));

        

        // parse the header metadata
        std::string headerMetaStyle = DEFAULT_HEADER_METADATA;
        for (uint32 index = 0; index < fileLines.size(); index++) {
            std::string line;

            line = fileLines[index];

            std::size_t checkIsMetadata = line.find(headerMetaStyle);

            if (checkIsMetadata == std::string::npos) {
                continue;
            }

            std::string key;
            std::string value;
            std::size_t firstColon = line.find_first_of(":", headerMetaStyle.size());

            key = line.substr(headerMetaStyle.size(), firstColon - headerMetaStyle.size());

            // remove spaces
            std::size_t posSpaces = 0;
            while ((posSpaces = key.find(" ")) != std::string::npos) {
                key.replace(posSpaces, 1, "");
            }

            value = line.substr(firstColon + 1, (line.size() - 2) - firstColon);
            // remove the fist space from the value
            posSpaces = value.find(" ");
            if (posSpaces != std::string::npos && posSpaces == 0) {
                value.replace(posSpaces, 1, "");
            }

            // key to lower
            std::transform(key.begin(), key.end(), key.begin(), tolower);

            if (key == "category") {
                file.category = value;
            } else if (key == "templatehtml") {
                file.templateHTML = value;
            } else if (key == "date") {
                std::size_t posBraces;
                posBraces = value.find_first_of("{{");

                if (posBraces != std::string::npos) {
                    file.date = Utils::getCurrentTime();
                    
                    // we have to update the md file since the date on the 
                    // header of the file shoul'd not be update every single time
                    updateHeaderDate(file.name);

                } else {
                    file.date = value;
                }
            } else if (key == "author") {
                file.author = value;
            } else if (key == "title") {
                file.title = value;
            } else if (key == "description") {
                file.description = value;
            } else if (key == "keywords") {
                file.keywords = value;
            } else if (key == "visible") {
                std::transform(value.begin(), value.end(), value.begin(), tolower);
                if (value ==  "false") {
                    file.visible = false;    
                } else {
                    file.visible = true;
                }
            }

            // remove the metadata
            fileLines[index] = "";
        }

        // regenerate the file
        content = "";

        for (uint32 index = 0; index < fileLines.size(); index++) {
            content = content + "\n" + fileLines[index];
        }    

        std::size_t startBraces = 0;
        while ((startBraces = content.find_first_of("{{")) != std::string::npos) {

            std::size_t endBraces = content.find_first_of("}}", startBraces);
            // we have to take in count the braces size, so we add and subtract 2
            std::string metadata = content.substr(startBraces + 2, (endBraces - 2) - startBraces);

            // remove spaces
            std::size_t posRemove = 0;
            while ((posRemove = metadata.find(" ")) != std::string::npos) {
                metadata.replace(posRemove, 1, "");
            }

            // to lower
            std::transform(metadata.begin(), metadata.end(), metadata.begin(), tolower);

            bool hasMatch = false;

            if (metadata == "name") {
                hasMatch = true;
                metadata =  mProject.name;
            } else if (metadata == "url") {
                hasMatch = true;
                metadata =  mProject.url;
            } else if (metadata == "templatehtml") {
                hasMatch = true;
                metadata =  mProject.templateHTML;
            } else if (metadata == "path") {
                hasMatch = true;
                metadata =  mProject.path;
            } else if (metadata == "content") {
                hasMatch = true;
                metadata =  mProject.content;
            } else if (metadata == "date") {
                metadata = Utils::getCurrentTime();
            } else {
                for (uint32 indexMeta = 0; indexMeta < mProject.metadata.size(); indexMeta++) {
                    if (metadata == mProject.metadata[indexMeta].name) {
                        hasMatch = true;
                        metadata = mProject.metadata[indexMeta].value;
                        break;
                    }
                }
            }

            if (hasMatch) {;
                content.replace(startBraces, (endBraces + 2) - startBraces, metadata);
            } else {
                content.replace(startBraces, (endBraces + 2) - startBraces, "&lbrace;&lbrace;" + metadata + "&rbrace;&rbrace;");
            }
        }
    }

    void updateHeaderDate(const std::string &fileName) {
        std::string filePath    = mProject.content + "/" + fileName;
        std::string fileContent = Utils::getFileContent(filePath);

        if (fileContent.empty()) {
            return;
        }

        std::size_t startBraces = fileContent.find_first_of("{{");

        if (startBraces == std::string::npos) {
            std::cout << "Coul'd not find the date header on the md file: " << filePath << std::endl;
            return;
        }

        std::size_t endBraces = fileContent.find_first_of("}}", startBraces);

        if (endBraces == std::string::npos) {
            std::cout << "Coul'd not find the date end braces on the header of the md file: " << filePath << std::endl;
            return;
        }

        fileContent.replace(startBraces, (endBraces + 2) - startBraces, Utils::getCurrentTime());
        
        Utils::writeFileToDisk(filePath, fileContent);
    }

    void openMarkDownFiles(const  std::vector<std::string> &files) {
        for (uint32 index = 0; index < files.size(); index++) {
            ConcisoFile file;
            std::string fileName = mProject.content + "/" + files[index];

            std::size_t posDot = files[index].find_last_of(".");

            file.name    = files[index].substr(0, posDot - files[index].size());
            file.content = Utils::getFileContent(fileName);
            mFiles.push_back(file);
        }
    }

    bool parseConfigurationFile() {
        uint8 defaultFieldMask = 0x0;

        std::fstream file;

        file.open(mConfigFile.c_str(), std::ios::in);

        if (file.is_open()) {
            std::string line;

            while (!file.eof()) {

                std::getline(file, line);

                // check line
                if (isLineValid(line)) {
                    std::string key;
                    std::string value;

                    key   = extractKey(line);
                    value = extractValue(line);
                    
                    std::transform(key.begin(), key.end(), key.begin(), tolower);

                    if (key == "name") {
                        mProject.name    = value;
                        defaultFieldMask = defaultFieldMask | 0x1;
                    } else if (key == "url") {
                        mProject.url     = value;
                        defaultFieldMask = defaultFieldMask | 0x2;
                    } else if (key == "path") {
                        mProject.path    = value;
                        defaultFieldMask = defaultFieldMask | 0x4;
                    } else if (key == "templatehtml" || key == "template") {
                        mProject.templateHTML = value;
                        defaultFieldMask      = defaultFieldMask | 0x8;
                    } else if (key == "content") {
                        mProject.content = value;
                        defaultFieldMask = defaultFieldMask | 0x10;
                    } else if (key == "output") {
                        mProject.output  = value;
                        defaultFieldMask = defaultFieldMask | 0x20;
                    } else {
                        Metadata metadata;
                        metadata.name  = key;
                        metadata.value = value;

                        mProject.metadata.push_back(metadata);
                    }
                }
            }

            file.close();

            if (defaultFieldMask >= 0x1b) {
                return true;
            }
        }

        // there is a error, we just need to figuere out

        if (!(defaultFieldMask & 0x1)) {
            std::cout <<  "Error: name not found on the configuration file" << std::endl;
        } else if (!(defaultFieldMask & 0x2)) {
            std::cout <<  "Error: url not found on the configuration file" << std::endl;
        } else if (!(defaultFieldMask & 0x8)) {
            std::cout <<  "Error: templateHTML not found on the configuration file" << std::endl;
        } else if (!(defaultFieldMask & 0x10)) {
            std::cout <<  "Error: templateHTML not found on the configuration file" << std::endl;
        } else {
            std::cout << "Error: Coul'd not parse the configuration file, check if the file exists or has the right permissions" << std::endl;
        }

        return false;
    }

    std::string extractKey(std::string line) {
        std::size_t pos = line.find("=");

        line = line.substr(0, pos);
        pos  = line.find(" ");
        if (pos != std::string::npos) {
            line = line.substr(0, pos);
        }
        return line;
    }

    std::string extractValue(std::string line) {
        std::size_t pos    = line.find("=");
        std::string result = line.substr(pos + 1, line.size());

        // trim
        std::size_t first = result.find_first_not_of("\t "); 
        std::size_t last  = result.find_last_not_of(' ');
        result            = result.substr(first, (last - first + 1));

        // remove \r, can cause problems on Mac and Unix, Belive me...
        if (result[result.size() - 1] == '\r') {
            result = result.substr(0, result.size() - 1);
        }

        // Fix \n in file not been reconized
        std::size_t posNewLine = 0;
        while ((posNewLine = result.find("\\n", posNewLine)) != std::string::npos) {
            result.replace(posNewLine, 2, " \n");
        }

        return result;
    }

    bool isLineValid(std::string line) {
        // if the line is empty we just return
        if (line.empty()) {
            return false;
        }

        // remove all the spaces and tab from the start
        std::size_t pos = line.find_first_not_of("\t ");

        // error if we had only one tab on the line
        if (pos == std::string::npos) {
            return false;
        }

        // remove the tab
        line = line.substr(pos, line.size());

        // try find the comment
        pos = line.find_first_of("#");

        // we just remove the comment until the end
        // so we can put a comment on a line
        if (pos != std::string::npos) {
           line = line.substr(0, pos);
        }

        // Remove the space at the end as well
        pos  = line.find_last_not_of("\t ");
        line = line.substr(0, pos + 1);

        // verify if  the equals is on the right position
        // if start with equals or it's at end we just return false
        if (line.find("=") <= 0 || line.find("=") >= line.size() - 1) {
            return false;
        }

        // else everything is ok
        return true;
    }

    std::string mConfigFile;
    ConcisoProject mProject;
    std::vector<ConcisoFile> mFiles;
    std::string templateHTMLCode;
};
