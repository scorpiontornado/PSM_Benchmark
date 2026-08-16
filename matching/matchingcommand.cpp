#include "matchingcommand.h"

MatchingCommand::MatchingCommand(const int argc, char **argv) : CommandParser(argc, argv) {
    // Initialize options value
    options_key[OptionKeyword::Algorithm] = "-a";
    options_key[OptionKeyword::IndexType] = "-i";
    options_key[OptionKeyword::QueryGraphFile] = "-q";
    options_key[OptionKeyword::DataGraphFile] = "-d";
    // options_key[OptionKeyword::ThreadCount] = "-n";
    options_key[OptionKeyword::DepthThreshold] = "-d0";
    options_key[OptionKeyword::WidthThreshold] = "-w0";
    options_key[OptionKeyword::Filter] = "-filter";
    options_key[OptionKeyword::Order] = "-order";
    options_key[OptionKeyword::Engine] = "-engine";
    options_key[OptionKeyword::MaxOutputEmbeddingNum] = "-num";
    options_key[OptionKeyword::SpectrumAnalysisTimeLimit] = "-time_limit";
    options_key[OptionKeyword::SpectrumAnalysisOrderNum] = "-order_num";
    options_key[OptionKeyword::DistributionFilePath] = "-dis_file";
    options_key[OptionKeyword::CSRFilePath] = "-csr";
    options_key[OptionKeyword::OutputFile] = "-OutputFile";
    options_key[OptionKeyword::ThreadNumbers] = "-threadnums";
    options_key[OptionKeyword::SplitTypes] = "-split";
    options_key[OptionKeyword::ScheduleTypes] = "-schedule";
    options_key[OptionKeyword::QorCandiType] = "-QorCandi";
    options_key[OptionKeyword::QpatternType] = "-Qpattern";
    options_key[OptionKeyword::JoinMethodType] = "-JoinMethod";
    options_key[OptionKeyword::BackMethodTypes] = "-BackMethods";
    options_key[OptionKeyword::ResultModeType] = "-mode";
    options_key[OptionKeyword::ResultSinkType] = "-sink";
    processOptions();
};

void MatchingCommand::processOptions() {
    // Query graph file path
    options_value[OptionKeyword::QueryGraphFile] = getCommandOption(options_key[OptionKeyword::QueryGraphFile]);;

    // Data graph file path
    options_value[OptionKeyword::DataGraphFile] = getCommandOption(options_key[OptionKeyword::DataGraphFile]);

    // Algorithm
    options_value[OptionKeyword::Algorithm] = getCommandOption(options_key[OptionKeyword::Algorithm]);

    // // Thread count
    // options_value[OptionKeyword::ThreadCount] = getCommandOption(options_key[OptionKeyword::ThreadCount]);

    // Depth threshold
    options_value[OptionKeyword::DepthThreshold] = getCommandOption(options_key[OptionKeyword::DepthThreshold]);

    // Width threshold
    options_value[OptionKeyword::WidthThreshold] = getCommandOption(options_key[OptionKeyword::WidthThreshold]);

    // Index Type
    options_value[OptionKeyword::IndexType] = getCommandOption(options_key[OptionKeyword::IndexType]);

    // Filter Type
    options_value[OptionKeyword::Filter] = getCommandOption(options_key[OptionKeyword::Filter]);

    // Order Type
    options_value[OptionKeyword::Order] = getCommandOption(options_key[OptionKeyword::Order]);

    // Engine Type
    options_value[OptionKeyword::Engine] = getCommandOption(options_key[OptionKeyword::Engine]);

    // Maximum output embedding num.
    options_value[OptionKeyword::MaxOutputEmbeddingNum] = getCommandOption(options_key[OptionKeyword::MaxOutputEmbeddingNum]);

    // Time Limit
    options_value[OptionKeyword::SpectrumAnalysisTimeLimit] = getCommandOption(options_key[OptionKeyword::SpectrumAnalysisTimeLimit]);

    // Order Num
    options_value[OptionKeyword::SpectrumAnalysisOrderNum] = getCommandOption(options_key[OptionKeyword::SpectrumAnalysisOrderNum]);

    // Distribution File Path
    options_value[OptionKeyword::DistributionFilePath] = getCommandOption(options_key[OptionKeyword::DistributionFilePath]);

    // CSR file path
    options_value[OptionKeyword::CSRFilePath] = getCommandOption(options_key[OptionKeyword::CSRFilePath]);

    // Output file path
    options_value[OptionKeyword::OutputFile] = getCommandOption(options_key[OptionKeyword::OutputFile]);

    // Thread Numbers
    options_value[OptionKeyword::ThreadNumbers] = getCommandOption(options_key[OptionKeyword::ThreadNumbers]);

    // Split Types
    options_value[OptionKeyword::SplitTypes] = getCommandOption(options_key[OptionKeyword::SplitTypes]);

    // Schedule Types
    options_value[OptionKeyword::ScheduleTypes] = getCommandOption(options_key[OptionKeyword::ScheduleTypes]);

    // QorCandi Types
    options_value[OptionKeyword::QorCandiType] = getCommandOption(options_key[OptionKeyword::QorCandiType]);

    // Qpattern Types
    options_value[OptionKeyword::QpatternType] = getCommandOption(options_key[OptionKeyword::QpatternType]);

    // Joinmethod Types
    options_value[OptionKeyword::JoinMethodType] = getCommandOption(options_key[OptionKeyword::JoinMethodType]);

    // Joinmethod Types
    options_value[OptionKeyword::BackMethodTypes] = getCommandOption(options_key[OptionKeyword::BackMethodTypes]);

    // Result Mode
    options_value[OptionKeyword::ResultModeType] = getCommandOption(options_key[OptionKeyword::ResultModeType]);

    // Result Sink Type
    options_value[OptionKeyword::ResultSinkType] = getCommandOption(options_key[OptionKeyword::ResultSinkType]);
}

void MatchingCommand::printCommandOptions() {
    std::string cmd;
    for (int i = Algorithm; i <= QorCandiType; ++i) {
        OptionKeyword keyword = static_cast<OptionKeyword>(i);
        cmd += std::to_string(static_cast<int>(keyword)) + ": " + options_value[keyword] + "\n";
    }
    std::cout << cmd << std::endl;;
}