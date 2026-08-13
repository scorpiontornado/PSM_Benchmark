#ifndef SUBGRAPHMATCHING_MATCHINGCOMMAND_H
#define SUBGRAPHMATCHING_MATCHINGCOMMAND_H

#include "utility/commandparser.h"
#include <map>
#include <iostream>
#include "utility/statistics/Logs.h"
// enum OptionKeyword {
//     Algorithm = 0,          // -a, The algorithm name, compulsive parameter
//     QueryGraphFile = 1,     // -q, The query graph file path, compulsive parameter
//     DataGraphFile = 2,      // -d, The data graph file path, compulsive parameter
//     ThreadCount = 3,        // -n, The number of thread, optional parameter
//     DepthThreshold = 4,     // -d0,The threshold to control the depth for splitting task, optional parameter
//     WidthThreshold = 5,     // -w0,The threshold to control the width for splitting task, optional parameter
//     IndexType = 6,          // -i, The type of index, vertex centric or edge centric
//     Filter = 7,             // -filter, The strategy of filtering
//     Order = 8,              // -order, The strategy of ordering
//     Engine = 9,             // -engine, The computation engine
//     MaxOutputEmbeddingNum = 10, // -num, The maximum output embedding num
//     SpectrumAnalysisTimeLimit = 11, // -time_limit, The time limit for executing a query in seconds
//     SpectrumAnalysisOrderNum = 12, // -order_num, The number of matching orders generated
//     DistributionFilePath = 13,          // -dis_file, The output path of the distribution array
//     CSRFilePath = 14,                   // -csr, The input csr file path
//     OutputFile = 15,                     // -o, output file path(absolute) 
//     ThreadNumbers = 16,                  // -threadnums, thread numbers
//     SplitTypes = 17,                     // -split, split types
//     ScheduleTypes = 18                   // -schedule, schedule types
// };

// 把ThreadCount给删了，这玩意没用

enum OptionKeyword {
    Algorithm = 0,          // -a, The algorithm name, compulsive parameter
    QueryGraphFile = 1,     // -q, The query graph file path, compulsive parameter
    DataGraphFile = 2,      // -d, The data graph file path, compulsive parameter
    DepthThreshold = 3,     // -d0,The threshold to control the depth for splitting task, optional parameter
    WidthThreshold = 4,     // -w0,The threshold to control the width for splitting task, optional parameter
    IndexType = 5,          // -i, The type of index, vertex centric or edge centric
    Filter = 6,             // -filter, The strategy of filtering
    Order = 7,              // -order, The strategy of ordering
    Engine = 8,             // -engine, The computation engine
    MaxOutputEmbeddingNum = 9, // -num, The maximum output embedding num
    SpectrumAnalysisTimeLimit = 10, // -time_limit, The time limit for executing a query in seconds
    SpectrumAnalysisOrderNum = 11, // -order_num, The number of matching orders generated
    DistributionFilePath = 12,          // -dis_file, The output path of the distribution array
    CSRFilePath = 13,                   // -csr, The input csr file path
    OutputFile = 14,                     // -o, output file path(absolute)
    ThreadNumbers = 15,                  // -threadnums, thread numbers
    SplitTypes = 16,                     // -split, split types
    ScheduleTypes = 17,                   // -schedule, schedule types
    QorCandiType = 18,                   // -QorCandi, split_Q or split_candidates
    QpatternType = 19,                   // -Qpattern, STAR or CHAIN
    JoinMethodType = 20,                  // -JoinMethod, LEFTDEEP or RIGHTDEEP
    BackMethodTypes = 21,                 // -BackMethod, DPiso or CIRCINUS
    MaterializeType = 22                  // -materialize, none | globallock | threadlocal
};

class MatchingCommand : public CommandParser{
private:
    std::map<OptionKeyword, std::string> options_key;
    std::map<OptionKeyword, std::string> options_value;

private:
    void processOptions();    

public:
    MatchingCommand(int argc, char **argv);

    void printCommandOptions();

    std::string getDataGraphFilePath() {
        return options_value[OptionKeyword::DataGraphFile];
    }

    std::string getQueryGraphFilePath() {
        return options_value[OptionKeyword::QueryGraphFile];
    }

    std::string getAlgorithm() {
        return options_value[OptionKeyword::Algorithm];
    }

    std::string getIndexType() {
        return options_value[OptionKeyword::IndexType] == "" ? "VertexCentric" : options_value[OptionKeyword::IndexType];
    }

    // std::string getThreadCount() {
    //     return options_value[OptionKeyword::ThreadCount] == "" ? "1" : options_value[OptionKeyword::ThreadCount];
    // }

    std::string getDepthThreshold() {
        return options_value[OptionKeyword::DepthThreshold] == "" ? "0" : options_value[OptionKeyword::DepthThreshold];
    }

    std::string getWidthThreshold() {
        return options_value[OptionKeyword::WidthThreshold] == "" ? "1" : options_value[OptionKeyword::WidthThreshold];
    }

    std::string getFilterType() {
        return options_value[OptionKeyword::Filter] == "" ? "CFL" : options_value[OptionKeyword::Filter];
    }

    std::string getOrderType() {
        return options_value[OptionKeyword::Order] == "" ? "GQL" : options_value[OptionKeyword::Order];
    }

    std::string getEngineType() {
        return options_value[OptionKeyword::Engine] == "" ? "LFTJ" : options_value[OptionKeyword::Engine];
    }

    std::string getMaximumEmbeddingNum() {
        return options_value[OptionKeyword::MaxOutputEmbeddingNum] == "" ? "MAX" : options_value[OptionKeyword::MaxOutputEmbeddingNum];
    }

    std::string getTimeLimit() {
        return options_value[OptionKeyword::SpectrumAnalysisTimeLimit] == "" ? "60" : options_value[OptionKeyword::SpectrumAnalysisTimeLimit];
    }

    std::string getOrderNum() {
        return options_value[OptionKeyword::SpectrumAnalysisOrderNum] == "" ? "100" : options_value[OptionKeyword::SpectrumAnalysisOrderNum];
    }

    std::string getDistributionFilePath() {
        return options_value[OptionKeyword::DistributionFilePath] == "" ? "temp.distribution" : options_value[OptionKeyword::DistributionFilePath];
    }

    std::string getCSRFilePath() {
        return options_value[OptionKeyword::CSRFilePath] == "" ? "" : options_value[OptionKeyword::CSRFilePath];
    }

    std::string getOutputFile() {
        return options_value[OptionKeyword::OutputFile] == "" ? "/root/yt/subgraph_test/output/" : options_value[OptionKeyword::OutputFile];
    }

    std::string getThreadNumbers() {
        return options_value[OptionKeyword::ThreadNumbers] == "" ? "1" : options_value[OptionKeyword::ThreadNumbers];
    }

    std::string getSplitType() {
        return options_value[OptionKeyword::SplitTypes] == "" ? "shixuan" : options_value[OptionKeyword::SplitTypes];
    }

    std::string getScheduleType() {
        return options_value[OptionKeyword::ScheduleTypes] == "" ? "steal" : options_value[OptionKeyword::ScheduleTypes];
    }

    std::string getQorCandiType() {
        return options_value[OptionKeyword::QorCandiType] == "" ? "NOSPLIT" : options_value[OptionKeyword::QorCandiType];
    }

    std::string getQpatternType() {
        return options_value[OptionKeyword::QpatternType] == "" ? "STAR" : options_value[OptionKeyword::QpatternType];
    }
    std::string getJoinMethod() {
        return options_value[OptionKeyword::JoinMethodType] == "" ? "LEFTDEEP" : options_value[OptionKeyword::JoinMethodType];
    }

    std::string getBackMethodTypes() {
        return options_value[OptionKeyword::BackMethodTypes] == "" ? "DPiso" : options_value[OptionKeyword::BackMethodTypes];
    }

    std::string getMaterializeType() {
        return options_value[OptionKeyword::MaterializeType] == "" ? "none" : options_value[OptionKeyword::MaterializeType];
    }
};


#endif //SUBGRAPHMATCHING_MATCHINGCOMMAND_H
