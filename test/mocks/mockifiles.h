#pragma once

#include "environment/ifiles.h"
#include "mls-unit-test/mock.h"

class MockIFiles : public IFiles {
public:
    MOCK_METHOD1(std::vector<Token>,
                 findFiles,
                 (Token pattern),
                 const override);

    using popenRetT = std::pair<int, std::string>;
    MOCK_METHOD1(popenRetT,
                 popenWithResult,
                 (std::string command),
                 const override);

    MOCK_METHOD1(time_t,
                 getTimeChanged,
                 (const std::string &path),
                 const override);

    MOCK_METHOD0(std::string, currentDirectory, (), const override);

    MOCK_METHOD1(bool,
                 currentDirectory,
                 (std::string directory),
                 const override);

    MOCK_METHOD1(std::vector<std::string>,
                 listFiles,
                 (std::string directory),
                 const override);

    MOCK_METHOD1(bool, isDirectory, (const std::string &path), const override);

    MOCK_METHOD1(void, createDirectory, (std::string dir), const override);

    MOCK_METHOD1(std::string,
                 getDirectory,
                 (std::string filename),
                 const override);

    MOCK_METHOD1(std::vector<std::string>,
                 listRecursive,
                 (std::string directory),
                 const override);

    MOCK_METHOD1(int, remove, (std::string filename), const override);

    MOCK_METHOD2(void,
                 replaceFile,
                 (std::string name, std::string value),
                 const override);
    MOCK_METHOD2(void,
                 appendToFile,
                 (std::string name, std::string value),
                 const override);

    //! @throws std::runtime_exception on fail
    MOCK_METHOD2(void,
                 copyFile,
                 (std::string source, std::string destination),
                 const override);

    MOCK_METHOD1(std::vector<std::string>,
                 readLines,
                 (std::string source),
                 const override);

    using parseDepFileT = std::pair<std::vector<std::string>, std::string>;
    MOCK_METHOD1(parseDepFileT, parseDepFile, (Token depFile), const override);
};
