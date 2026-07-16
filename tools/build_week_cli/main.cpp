#include "BuildWeekCli.h"

#include <exception>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    try {
        return media::ffmpeg::graph::build_week::runBuildWeekCli(argc, argv);
    } catch (const std::invalid_argument& error) {
        std::cerr << "[Build Week] " << error.what() << "\n\n"
                  << media::ffmpeg::graph::build_week::buildWeekUsage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "[Build Week] fatal error: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "[Build Week] fatal unknown error\n";
        return 1;
    }
}