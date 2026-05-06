#include "telemetry.hpp"

#include <iostream>

int main(int argc, char** argv) {
    // The executable expects exactly one telemetry log path.
    if (argc != 2) {
        std::cerr << "usage: telemetry_check <input_path>\n";
        return 1;
    }

    Frame frames[MAX_TELEMETRY_FRAMES];
    
    const std::optional<int> frame_count_optional = read_frames(argv[1], frames, MAX_TELEMETRY_FRAMES);
    if (!frame_count_optional.has_value()) {
        std::cerr << "Error: There was an error while reading frames" << std::endl;
        return 1;
    }
    const int frame_count = frame_count_optional.value();

    const bool frames_are_valid = validate_frames(frames, frame_count);
    if (!frames_are_valid) {
        std::cerr << "Error: Invalid frames passed" << std::endl;
        return 1;
    }

    const Summary summary = summarize(frames, frame_count);
    print_summary(summary);

    return 0;
}
